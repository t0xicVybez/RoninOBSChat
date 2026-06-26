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
    void onAutoModAction(const ChatMessage &msg, const AutoModResult &result);
    void onConnectClicked();
    void onSettingsClicked();

    // Moderation panel
    void refreshModeration();
    void onLiftSelected();

private:
    void appendLine(const QString &html);
    void updateStatusLabel(bool connected);

    ChatBot      *m_bot;
    QListWidget  *m_chatView;
    QLabel       *m_statusLabel;
    QPushButton  *m_connectBtn;
    QPushButton  *m_settingsBtn;

    // Active timeouts / bans, with a Lift button (YouTube can't lift timeouts
    // natively, so the plugin owns this).
    QListWidget  *m_modList;
    QPushButton  *m_liftBtn;
};
