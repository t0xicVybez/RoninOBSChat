#pragma once
#include <QObject>
#include <QSet>
#include <QQueue>
#include "ChatMessage.hpp"
#include "YouTubeClient.hpp"
#include "CommandHandler.hpp"
#include "TimedMessages.hpp"
#include "AutoMod.hpp"
#include "ModerationLog.hpp"

class ChatBot : public QObject {
    Q_OBJECT

public:
    explicit ChatBot(QObject *parent = nullptr);

    YouTubeClient  *youtube()    const { return m_youtube; }
    CommandHandler *commands()   const { return m_commands; }
    TimedMessages  *timers()     const { return m_timers; }
    AutoMod        *automod()    const { return m_automod; }
    ModerationLog  *moderation() const { return m_moderation; }

    void loadConfig();
    void saveConfig();

signals:
    void messageReceived(const ChatMessage &msg);
    void botSentMessage(const QString &text);
    void autoModActionTaken(const ChatMessage &msg, const AutoModResult &result);

private slots:
    void onMessagesReceived(const QList<ChatMessage> &messages);
    void onTimedMessage(const QString &text);
    void onConnectionChanged(bool connected);

private:
    QString configFilePath() const;

    // Returns true if this message id has not been processed before. Bounded so
    // a long stream doesn't grow the set without limit.
    bool markSeen(const QString &id);

    // Moderator-only !untimeout / !unban commands. Returns true if handled.
    bool handleModerationCommand(const ChatMessage &msg);

    YouTubeClient  *m_youtube;
    CommandHandler *m_commands;
    TimedMessages  *m_timers;
    AutoMod        *m_automod;
    ModerationLog  *m_moderation;

    QSet<QString>    m_seenIds;
    QQueue<QString>  m_seenOrder;
};
