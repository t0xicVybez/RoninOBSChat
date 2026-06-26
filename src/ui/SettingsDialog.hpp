#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include "../ChatBot.hpp"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(ChatBot *bot, QWidget *parent = nullptr);

private slots:
    // YouTube tab
    void onStartAuth();
    void onDeviceFlowReady(const QString &url, const QString &code);
    void onDeviceFlowCompleted();
    void onDeviceFlowFailed(const QString &reason);

    // Commands tab
    void onAddCommand();
    void onEditCommand();
    void onRemoveCommand();

    // Timers tab
    void onAddTimer();
    void onEditTimer();
    void onRemoveTimer();

    // Dialog
    void onSave();

private:
    QWidget *buildYouTubeTab();
    QWidget *buildCommandsTab();
    QWidget *buildTimersTab();

    void loadCommandsTable();
    void loadTimersTable();

    ChatBot *m_bot;

    // YouTube tab widgets
    QLineEdit   *m_clientIdEdit;
    QLineEdit   *m_clientSecretEdit;
    QPushButton *m_authBtn;
    QLabel      *m_authStatusLabel;

    // Commands tab
    QTableWidget *m_commandsTable;

    // Timers tab
    QTableWidget *m_timersTable;
};
