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
{
    connect(m_youtube, &YouTubeClient::messagesReceived,
            this, &ChatBot::onMessagesReceived);

    connect(m_youtube, &YouTubeClient::connectionChanged,
            this, &ChatBot::onConnectionChanged);

    connect(m_timers, &TimedMessages::sendMessage,
            this, &ChatBot::onTimedMessage);
}

// ── Routing ───────────────────────────────────────────────────────────────────

void ChatBot::onMessagesReceived(const QList<ChatMessage> &messages)
{
    for (const auto &msg : messages) {
        emit messageReceived(msg);

        // AutoMod runs before command processing
        AutoModResult result = m_automod->check(msg);
        if (result.action != AutoModAction::None) {
            switch (result.action) {
            case AutoModAction::DeleteMessage:
                m_youtube->deleteMessage(msg.id);
                break;
            case AutoModAction::TimeoutUser:
                m_youtube->timeoutUser(msg.authorChannelId, result.timeoutSecs);
                break;
            case AutoModAction::BanUser:
                m_youtube->banUser(msg.authorChannelId);
                break;
            default: break;
            }
            emit autoModActionTaken(msg, result);
            continue; // Skip command processing for moderated messages
        }

        // Command processing
        QString response;
        if (m_commands->process(msg, response)) {
            m_youtube->sendMessage(response);
            emit botSentMessage(response);
        }
    }
}

void ChatBot::onTimedMessage(const QString &text)
{
    m_youtube->sendMessage(text);
    emit botSentMessage(text);
}

void ChatBot::onConnectionChanged(bool connected)
{
    if (connected) {
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
