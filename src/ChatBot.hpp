#pragma once
#include <QObject>
#include "ChatMessage.hpp"
#include "YouTubeClient.hpp"
#include "CommandHandler.hpp"
#include "TimedMessages.hpp"
#include "AutoMod.hpp"

class ChatBot : public QObject {
    Q_OBJECT

public:
    explicit ChatBot(QObject *parent = nullptr);

    YouTubeClient  *youtube()  const { return m_youtube; }
    CommandHandler *commands() const { return m_commands; }
    TimedMessages  *timers()   const { return m_timers; }
    AutoMod        *automod()  const { return m_automod; }

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
    YouTubeClient  *m_youtube;
    CommandHandler *m_commands;
    TimedMessages  *m_timers;
    AutoMod        *m_automod;

    QString configFilePath() const;
};
