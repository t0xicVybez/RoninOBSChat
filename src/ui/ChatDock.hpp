#pragma once
#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include "../ChatBot.hpp"

class ChatDock : public QDockWidget {
    Q_OBJECT

public:
    explicit ChatDock(ChatBot *bot, QWidget *parent = nullptr);

private slots:
    void onConnectionChanged(bool connected);
    void onMessageReceived(const ChatMessage &msg);
    void onBotSentMessage(const QString &text);
    void onConnectClicked();
    void onSettingsClicked();

private:
    void appendLine(const QString &html);
    void updateStatusLabel(bool connected);

    ChatBot      *m_bot;
    QListWidget  *m_chatView;
    QLabel       *m_statusLabel;
    QPushButton  *m_connectBtn;
    QPushButton  *m_settingsBtn;
};
