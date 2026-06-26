#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
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

    // AutoMod tab
    void onAddRule();
    void onEditRule();
    void onRemoveRule();

    // Dialog
    void onSave();

private:
    QWidget *buildYouTubeTab();
    QWidget *buildCommandsTab();
    QWidget *buildTimersTab();
    QWidget *buildAutoModTab();

    void loadCommandsTable();
    void loadTimersTable();
    void loadAutoModTable();

    ChatBot *m_bot;

    // YouTube tab widgets
    QLineEdit   *m_clientIdEdit;
    QLineEdit   *m_clientSecretEdit;
    QPushButton *m_authBtn;
    QLabel      *m_authStatusLabel;
    QSpinBox    *m_pollIntervalSpin;

    // Commands tab
    QTableWidget *m_commandsTable;

    // Timers tab
    QTableWidget *m_timersTable;

    // AutoMod tab
    QTableWidget *m_autoModTable;
};
