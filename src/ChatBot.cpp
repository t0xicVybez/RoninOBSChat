#include "ChatBot.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <obs-module.h>

ChatBot::ChatBot(QObject *parent)
    : QObject(parent)
    , m_youtube(new YouTubeClient(this))
    , m_commands(new CommandHandler(this))
    , m_timers(new TimedMessages(this))
    , m_automod(new AutoMod(this))
    , m_moderation(new ModerationLog(this))
{
    connect(m_youtube, &YouTubeClient::messagesReceived,
            this, &ChatBot::onMessagesReceived);

    connect(m_youtube, &YouTubeClient::connectionChanged,
            this, &ChatBot::onConnectionChanged);

    connect(m_timers, &TimedMessages::sendMessage,
            this, &ChatBot::onTimedMessage);

    // Keep the moderation log in sync with what actually happened on YouTube:
    // fill in ban ids when they come back, and drop entries when lifted.
    connect(m_youtube, &YouTubeClient::banCreated, this,
            [this](const QString &channelId, const QString &banId, bool, int) {
                m_moderation->attachBanId(channelId, banId);
            });
    connect(m_youtube, &YouTubeClient::banLifted,
            m_moderation, &ModerationLog::removeByBanId);
    connect(m_youtube, &YouTubeClient::banFailed,
            m_moderation, &ModerationLog::removePendingByChannel);
}

// Bounded de-dup: remember up to 2000 recent message ids so a message is only
// ever acted on once. The set is cleared on (re)connect in onConnectionChanged.
bool ChatBot::markSeen(const QString &id)
{
    if (id.isEmpty() || m_seenIds.contains(id)) return false;
    m_seenIds.insert(id);
    m_seenOrder.enqueue(id);
    if (m_seenOrder.size() > 2000)
        m_seenIds.remove(m_seenOrder.dequeue());
    return true;
}

// ── Routing ───────────────────────────────────────────────────────────────────

void ChatBot::onMessagesReceived(const QList<ChatMessage> &messages)
{
    for (const auto &msg : messages) {
        emit messageReceived(msg); // always display, even history

        // Never moderate or run commands on backlog, and never act on the same
        // message twice (overlapping pages / reconnect).
        if (msg.isHistorical) continue;
        if (!markSeen(msg.id))  continue;

        // Moderator lift commands take priority over everything else.
        if (handleModerationCommand(msg))
            continue;

        // ── AutoMod (mods/owner are exempt inside check()) ──────────────────
        AutoModResult result = m_automod->check(msg);
        if (result.action != AutoModAction::None) {
            blog(LOG_INFO, "[RoninOBSChat] AutoMod match: user=%s rule=%s action=%d",
                 msg.authorDisplayName.toUtf8().constData(),
                 result.matchedRule.toUtf8().constData(),
                 static_cast<int>(result.action));

            switch (result.action) {
            case AutoModAction::DeleteMessage:
                m_youtube->deleteMessage(msg.id);
                break;
            case AutoModAction::TimeoutUser:
                // Always remove the offending message, then time the user out.
                m_youtube->deleteMessage(msg.id);
                // One active ban per user — don't stack on a message burst.
                if (m_moderation->hasActiveBan(msg.authorChannelId)) {
                    blog(LOG_INFO, "[RoninOBSChat] Timeout skipped — %s already has an active ban",
                         msg.authorDisplayName.toUtf8().constData());
                } else {
                    m_youtube->timeoutUser(msg.authorChannelId, result.timeoutSecs);
                    m_moderation->recordPending(msg.authorChannelId, msg.authorDisplayName,
                                                "Timeout", result.matchedRule,
                                                true, result.timeoutSecs);
                }
                break;
            case AutoModAction::BanUser:
                // Always remove the offending message, then ban the user.
                m_youtube->deleteMessage(msg.id);
                if (m_moderation->hasActiveBan(msg.authorChannelId)) {
                    blog(LOG_INFO, "[RoninOBSChat] Ban skipped — %s already has an active ban",
                         msg.authorDisplayName.toUtf8().constData());
                } else {
                    m_youtube->banUser(msg.authorChannelId);
                    m_moderation->recordPending(msg.authorChannelId, msg.authorDisplayName,
                                                "Ban", result.matchedRule, false, 0);
                }
                break;
            default: break;
            }
            emit autoModActionTaken(msg, result);
            continue; // moderated messages don't run commands
        }

        // ── Commands ────────────────────────────────────────────────────────
        QString response;
        if (m_commands->process(msg, response)) {
            m_youtube->sendMessage(response);
            emit botSentMessage(response);
        }
    }
}

bool ChatBot::handleModerationCommand(const ChatMessage &msg)
{
    if (!(msg.authorIsModerator || msg.authorIsOwner))
        return false;

    const QString text = msg.text.trimmed();
    const QString lower = text.toLower();
    if (!lower.startsWith("!untimeout") && !lower.startsWith("!unban"))
        return false;

    // Everything after the command word is the target display name (with or
    // without a leading @). YouTube gives us no channelId for plain @mentions,
    // so we match by the display name we stored when we issued the ban.
    int sp = text.indexOf(' ');
    QString target = sp >= 0 ? text.mid(sp + 1).trimmed() : QString();
    if (target.startsWith('@')) target.remove(0, 1);

    if (target.isEmpty()) {
        m_youtube->sendMessage("Usage: !untimeout <name>  (or !unban <name>)");
        return true;
    }

    QString banId = m_moderation->banIdForName(target);
    if (banId.isEmpty()) {
        m_youtube->sendMessage(QStringLiteral("No active timeout/ban found for %1.").arg(target));
        return true;
    }

    m_youtube->liftBan(banId);
    m_youtube->sendMessage(QStringLiteral("Lifted timeout/ban for %1.").arg(target));
    return true;
}

void ChatBot::onTimedMessage(const QString &text)
{
    m_youtube->sendMessage(text);
    emit botSentMessage(text);
}

void ChatBot::onConnectionChanged(bool connected)
{
    if (connected) {
        m_seenIds.clear();
        m_seenOrder.clear();
        m_timers->startAll();
    } else {
        m_timers->stopAll();
    }
}

// ── Config ────────────────────────────────────────────────────────────────────

QString ChatBot::configFilePath() const
{
    char *path = obs_module_get_config_path(obs_current_module(), "config.json");
    QString result = QString::fromUtf8(path);
    bfree(path);
    return result;
}

void ChatBot::loadConfig()
{
    QString path = configFilePath();

    // Ensure the config directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    auto doc = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    // YouTube credentials & tokens
    if (doc.contains("youtube")) {
        auto yt = doc["youtube"].toObject();
        OAuthTokens tokens;
        tokens.clientId     = yt["clientId"].toString();
        tokens.clientSecret = yt["clientSecret"].toString();
        tokens.accessToken  = yt["accessToken"].toString();
        tokens.refreshToken = yt["refreshToken"].toString();
        tokens.expiresAt    = yt["expiresAt"].toVariant().toLongLong();
        m_youtube->setTokens(tokens);
    }

    if (doc.contains("commands"))
        m_commands->fromJson(doc["commands"].toArray());

    if (doc.contains("timers"))
        m_timers->fromJson(doc["timers"].toArray());

    if (doc.contains("automod"))
        m_automod->fromJson(doc["automod"].toArray());

    blog(LOG_INFO, "[RoninOBSChat] Config loaded from %s", path.toUtf8().constData());
}

void ChatBot::saveConfig()
{
    QJsonObject doc;

    const auto &t = m_youtube->tokens();
    doc["youtube"] = QJsonObject{
        {"clientId",     t.clientId},
        {"clientSecret", t.clientSecret},
        {"accessToken",  t.accessToken},
        {"refreshToken", t.refreshToken},
        {"expiresAt",    QString::number(t.expiresAt)}
    };

    doc["commands"] = m_commands->toJson();
    doc["timers"]   = m_timers->toJson();
    doc["automod"]  = m_automod->toJson();

    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
}
