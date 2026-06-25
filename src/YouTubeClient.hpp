#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "ChatMessage.hpp"

struct OAuthTokens {
    QString clientId;
    QString clientSecret;
    QString accessToken;
    QString refreshToken;
    qint64 expiresAt = 0; // Unix epoch seconds
};

class YouTubeClient : public QObject {
    Q_OBJECT

public:
    explicit YouTubeClient(QObject *parent = nullptr);

    // Credentials / token management
    void setCredentials(const QString &clientId, const QString &clientSecret);
    void setTokens(const OAuthTokens &tokens);
    const OAuthTokens &tokens() const { return m_tokens; }
    bool isAuthenticated() const;
    bool isConnected() const { return m_connected; }

    // Connect / disconnect from an active live stream
    void connectToStream();
    void disconnectFromStream();

    // OAuth device flow: call once credentials are set
    void startDeviceFlow();

    // Chat actions (no-ops if not connected)
    void sendMessage(const QString &text);
    void deleteMessage(const QString &messageId);
    void timeoutUser(const QString &channelId, int durationSeconds);
    void banUser(const QString &channelId);

signals:
    void messagesReceived(const QList<ChatMessage> &messages);
    void connectionChanged(bool connected);
    void errorOccurred(const QString &msg);

    // Device-flow progress
    void deviceFlowReady(const QString &verificationUrl, const QString &userCode);
    void deviceFlowCompleted();
    void deviceFlowFailed(const QString &reason);

private slots:
    void pollMessages();
    void pollDeviceToken();
    void scheduleTokenRefresh();

private:
    void fetchLiveChatId();
    void refreshAccessToken(std::function<void()> onDone = nullptr);
    QNetworkRequest authorizedRequest(const QUrl &url) const;

    QNetworkAccessManager *m_nam;
    QTimer *m_pollTimer;
    QTimer *m_devicePollTimer;

    OAuthTokens m_tokens;
    QString m_liveChatId;
    QString m_nextPageToken;
    QString m_deviceCode;

    bool m_connected = false;
    int  m_pollingIntervalMs = 5000;
};
