#include "YouTubeClient.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <obs-module.h>

// Google OAuth / YouTube endpoints
static constexpr const char *kDeviceCodeUrl   = "https://oauth2.googleapis.com/device/code";
static constexpr const char *kTokenUrl        = "https://oauth2.googleapis.com/token";
static constexpr const char *kBroadcastsUrl   = "https://www.googleapis.com/youtube/v3/liveBroadcasts";
static constexpr const char *kMessagesUrl     = "https://www.googleapis.com/youtube/v3/liveChat/messages";
static constexpr const char *kBansUrl         = "https://www.googleapis.com/youtube/v3/liveChat/bans";
static constexpr const char *kYTScope         = "https://www.googleapis.com/auth/youtube";

YouTubeClient::YouTubeClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
    , m_devicePollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &YouTubeClient::pollMessages);

    m_devicePollTimer->setSingleShot(false);
    m_devicePollTimer->setInterval(5000);
    connect(m_devicePollTimer, &QTimer::timeout, this, &YouTubeClient::pollDeviceToken);
}

// ── Credentials ───────────────────────────────────────────────────────────────

void YouTubeClient::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_tokens.clientId     = clientId;
    m_tokens.clientSecret = clientSecret;
}

void YouTubeClient::setTokens(const OAuthTokens &tokens)
{
    m_tokens = tokens;
}

bool YouTubeClient::isAuthenticated() const
{
    return !m_tokens.accessToken.isEmpty() || !m_tokens.refreshToken.isEmpty();
}

QNetworkRequest YouTubeClient::authorizedRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setRawHeader("Authorization",
                     QStringLiteral("Bearer %1").arg(m_tokens.accessToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return req;
}

// ── OAuth device flow ─────────────────────────────────────────────────────────

void YouTubeClient::startDeviceFlow()
{
    if (m_tokens.clientId.isEmpty() || m_tokens.clientSecret.isEmpty()) {
        emit deviceFlowFailed("Client ID and Client Secret must be set first.");
        return;
    }

    QNetworkRequest req{QUrl{kDeviceCodeUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id", m_tokens.clientId);
    body.addQueryItem("scope", kYTScope);

    auto *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit deviceFlowFailed(reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        m_deviceCode = doc["device_code"].toString();
        int interval = doc["interval"].toInt(5) * 1000;
        m_devicePollTimer->setInterval(interval);
        m_devicePollTimer->start();

        emit deviceFlowReady(doc["verification_url"].toString(),
                             doc["user_code"].toString());
    });
}

void YouTubeClient::pollDeviceToken()
{
    QNetworkRequest req{QUrl{kTokenUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id",     m_tokens.clientId);
    body.addQueryItem("client_secret", m_tokens.clientSecret);
    body.addQueryItem("device_code",   m_deviceCode);
    body.addQueryItem("grant_type",    "urn:ietf:params:oauth:grant-type:device_code");

    auto *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();

        if (doc.contains("error")) {
            // "authorization_pending" is expected while the user hasn't clicked yet
            if (doc["error"].toString() != "authorization_pending") {
                m_devicePollTimer->stop();
                emit deviceFlowFailed(doc["error_description"].toString());
            }
            return;
        }

        m_devicePollTimer->stop();
        m_tokens.accessToken  = doc["access_token"].toString();
        m_tokens.refreshToken = doc["refresh_token"].toString();
        m_tokens.expiresAt    = QDateTime::currentSecsSinceEpoch() + doc["expires_in"].toInt(3600);
        emit deviceFlowCompleted();
    });
}

// ── Token refresh ─────────────────────────────────────────────────────────────

void YouTubeClient::refreshAccessToken(std::function<void()> onDone)
{
    QNetworkRequest req{QUrl{kTokenUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id",     m_tokens.clientId);
    body.addQueryItem("client_secret", m_tokens.clientSecret);
    body.addQueryItem("refresh_token", m_tokens.refreshToken);
    body.addQueryItem("grant_type",    "refresh_token");

    auto *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, onDone]() {
        reply->deleteLater();
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        if (doc.contains("access_token")) {
            m_tokens.accessToken = doc["access_token"].toString();
            m_tokens.expiresAt   = QDateTime::currentSecsSinceEpoch() + doc["expires_in"].toInt(3600);
        }
        if (onDone) onDone();
    });
}

void YouTubeClient::scheduleTokenRefresh()
{
    qint64 expiresIn = m_tokens.expiresAt - QDateTime::currentSecsSinceEpoch();
    if (expiresIn < 120) {
        refreshAccessToken();
    }
}

// ── Connect / disconnect ──────────────────────────────────────────────────────

void YouTubeClient::connectToStream()
{
    if (!isAuthenticated()) {
        emit errorOccurred("Not authenticated. Complete YouTube OAuth first.");
        return;
    }

    auto doConnect = [this]() {
        fetchLiveChatId();
    };

    // Proactively refresh if token expires within 2 minutes
    if (QDateTime::currentSecsSinceEpoch() >= m_tokens.expiresAt - 120) {
        refreshAccessToken(doConnect);
    } else {
        doConnect();
    }
}

void YouTubeClient::disconnectFromStream()
{
    m_pollTimer->stop();
    m_liveChatId.clear();
    m_nextPageToken.clear();

    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
    }
}

void YouTubeClient::fetchLiveChatId()
{
    QUrl url(kBroadcastsUrl);
    QUrlQuery q;
    q.addQueryItem("part", "snippet");
    q.addQueryItem("broadcastStatus", "active");
    q.addQueryItem("broadcastType", "all");
    url.setQuery(q);

    auto *reply = m_nam->get(authorizedRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Failed to fetch broadcast: " + reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        auto items = doc["items"].toArray();
        if (items.isEmpty()) {
            emit errorOccurred("No active live broadcast found on this account.");
            return;
        }
        m_liveChatId = items[0][QLatin1String("snippet")].toObject()[QLatin1String("liveChatId")].toString();
        blog(LOG_INFO, "[RoninOBSChat] Live chat ID: %s",
             m_liveChatId.toUtf8().constData());

        m_connected   = true;
        m_connectedAt = QDateTime::currentDateTimeUtc();
        m_firstPoll   = true;
        emit connectionChanged(true);
        m_pollTimer->setInterval(m_pollingIntervalMs);
        m_pollTimer->start();
        pollMessages(); // Fetch immediately
    });
}

// ── Message polling ───────────────────────────────────────────────────────────

void YouTubeClient::pollMessages()
{
    scheduleTokenRefresh();

    if (m_liveChatId.isEmpty()) return;

    QUrl url(kMessagesUrl);
    QUrlQuery q;
    q.addQueryItem("liveChatId", m_liveChatId);
    q.addQueryItem("part", "snippet,authorDetails");
    if (!m_nextPageToken.isEmpty())
        q.addQueryItem("pageToken", m_nextPageToken);
    url.setQuery(q);

    auto *reply = m_nam->get(authorizedRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Poll error: " + reply->errorString());
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        m_nextPageToken = doc["nextPageToken"].toString();
        int interval = doc["pollingIntervalMillis"].toInt(5000);
        m_pollingIntervalMs = interval;
        m_pollTimer->setInterval(interval);

        auto items = doc["items"].toArray();
        if (items.isEmpty()) return;

        QList<ChatMessage> messages;
        for (const auto &val : items) {
            auto item   = val.toObject();
            auto snip   = item["snippet"].toObject();
            auto author = item["authorDetails"].toObject();

            ChatMessage msg;
            msg.id                 = item["id"].toString();
            msg.text               = snip["displayMessage"].toString();
            msg.publishedAt        = QDateTime::fromString(snip["publishedAt"].toString(), Qt::ISODate);
            msg.authorChannelId    = author["channelId"].toString();
            msg.authorDisplayName  = author["displayName"].toString();
            msg.authorIsModerator  = author["isChatModerator"].toBool();
            msg.authorIsOwner      = author["isChatOwner"].toBool();

            // The first poll is the backlog snapshot; also guard on timestamp so
            // a reconnect can never re-moderate messages from before we connected.
            msg.isHistorical = m_firstPoll ||
                               (msg.publishedAt.isValid() && msg.publishedAt < m_connectedAt);
            messages.append(msg);
        }
        m_firstPoll = false;
        emit messagesReceived(messages);
    });
}

// ── Chat actions ──────────────────────────────────────────────────────────────

void YouTubeClient::sendMessage(const QString &text)
{
    if (!m_connected || m_liveChatId.isEmpty()) return;

    QJsonObject body{
        {"snippet", QJsonObject{
            {"liveChatId", m_liveChatId},
            {"type", "textMessageEvent"},
            {"textMessageDetails", QJsonObject{{"messageText", text}}}
        }}
    };

    QUrl url(kMessagesUrl);
    QUrlQuery q;
    q.addQueryItem("part", "snippet");
    url.setQuery(q);

    auto req = authorizedRequest(url);
    auto *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
    });
}

void YouTubeClient::deleteMessage(const QString &messageId)
{
    if (!m_connected) return;

    QUrl url(kMessagesUrl);
    QUrlQuery q;
    q.addQueryItem("id", messageId);
    url.setQuery(q);

    auto *reply = m_nam->deleteResource(authorizedRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, messageId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString body = QString::fromUtf8(reply->readAll());
            blog(LOG_WARNING, "[RoninOBSChat] Delete failed for %s: %s",
                 messageId.toUtf8().constData(), body.toUtf8().constData());
            emit errorOccurred("Delete failed: " + body);
        } else {
            blog(LOG_INFO, "[RoninOBSChat] Deleted message %s",
                 messageId.toUtf8().constData());
        }
    });
}

void YouTubeClient::timeoutUser(const QString &channelId, int durationSeconds)
{
    if (!m_connected) return;

    QJsonObject body{
        {"snippet", QJsonObject{
            {"liveChatId", m_liveChatId},
            {"type", "temporary"},
            {"banDurationSeconds", durationSeconds},
            {"bannedUserDetails", QJsonObject{{"channelId", channelId}}}
        }}
    };

    QUrl url(kBansUrl);
    QUrlQuery q;
    q.addQueryItem("part", "snippet");
    url.setQuery(q);

    blog(LOG_INFO, "[RoninOBSChat] Timeout request: channel=%s duration=%ds",
         channelId.toUtf8().constData(), durationSeconds);

    auto *reply = m_nam->post(authorizedRequest(url),
                              QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, channelId, durationSeconds]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errBody = QString::fromUtf8(reply->readAll());
            blog(LOG_WARNING, "[RoninOBSChat] Timeout failed for %s: %s",
                 channelId.toUtf8().constData(), errBody.toUtf8().constData());
            emit banFailed(channelId);
            emit errorOccurred("Timeout failed: " + errBody);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        // YouTube returns the id with its trailing '=' already percent-encoded
        // (%3D). Decode to the canonical form so we don't double-encode on lift.
        QString banId = QUrl::fromPercentEncoding(doc["id"].toString().toUtf8());
        blog(LOG_INFO, "[RoninOBSChat] Timeout created: channel=%s banId=%s",
             channelId.toUtf8().constData(), banId.toUtf8().constData());
        emit banCreated(channelId, banId, true, durationSeconds);
    });
}

void YouTubeClient::banUser(const QString &channelId)
{
    if (!m_connected) return;

    QJsonObject body{
        {"snippet", QJsonObject{
            {"liveChatId", m_liveChatId},
            {"type", "permanent"},
            {"bannedUserDetails", QJsonObject{{"channelId", channelId}}}
        }}
    };

    QUrl url(kBansUrl);
    QUrlQuery q;
    q.addQueryItem("part", "snippet");
    url.setQuery(q);

    blog(LOG_INFO, "[RoninOBSChat] Ban request: channel=%s",
         channelId.toUtf8().constData());

    auto *reply = m_nam->post(authorizedRequest(url),
                              QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errBody = QString::fromUtf8(reply->readAll());
            blog(LOG_WARNING, "[RoninOBSChat] Ban failed for %s: %s",
                 channelId.toUtf8().constData(), errBody.toUtf8().constData());
            emit banFailed(channelId);
            emit errorOccurred("Ban failed: " + errBody);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        // See note in timeoutUser: decode the percent-encoded id from YouTube.
        QString banId = QUrl::fromPercentEncoding(doc["id"].toString().toUtf8());
        blog(LOG_INFO, "[RoninOBSChat] Ban created: channel=%s banId=%s",
             channelId.toUtf8().constData(), banId.toUtf8().constData());
        emit banCreated(channelId, banId, false, 0);
    });
}

void YouTubeClient::liftBan(const QString &banId)
{
    if (!m_connected || banId.isEmpty()) return;

    // Normalise first (in case it still carries YouTube's %3D), then encode the
    // id exactly once. Going through QUrlQuery would double-encode the '='.
    QString canonical = QUrl::fromPercentEncoding(banId.toUtf8());
    QUrl url(kBansUrl);
    url.setQuery("id=" + QString::fromUtf8(QUrl::toPercentEncoding(canonical)));

    auto *reply = m_nam->deleteResource(authorizedRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, banId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errBody = QString::fromUtf8(reply->readAll());
            // A timeout that already expired (or a ban already lifted) no longer
            // exists, so YouTube rejects the id. That's effectively success —
            // drop it from the list instead of showing a scary error.
            if (errBody.contains("invalidLiveChatBanId")) {
                blog(LOG_INFO, "[RoninOBSChat] Lift: ban %s already expired/lifted",
                     banId.toUtf8().constData());
                emit errorOccurred("Timeout/ban already expired — removed from list.");
                emit banLifted(banId);
                return;
            }
            blog(LOG_WARNING, "[RoninOBSChat] Lift failed for %s: %s",
                 banId.toUtf8().constData(), errBody.toUtf8().constData());
            emit errorOccurred("Lift failed: " + errBody);
            return;
        }
        blog(LOG_INFO, "[RoninOBSChat] Lifted ban %s", banId.toUtf8().constData());
        emit banLifted(banId);
    });
}
