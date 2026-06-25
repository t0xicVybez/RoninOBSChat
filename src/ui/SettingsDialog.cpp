#include "SettingsDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(ChatBot *bot, QWidget *parent)
    : QDialog(parent)
    , m_bot(bot)
{
    setWindowTitle("Ronin Chat Bot — Settings");
    setMinimumSize(640, 480);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildYouTubeTab(),  "YouTube");
    tabs->addTab(buildCommandsTab(), "Commands");
    tabs->addTab(buildTimersTab(),   "Timers");
    tabs->addTab(buildAutoModTab(),  "AutoMod");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    loadCommandsTable();
    loadTimersTable();
    loadAutoModTable();
}

// ─────────────────────────────────────────────────────────────────────────────
// YouTube tab
// ─────────────────────────────────────────────────────────────────────────────

QWidget *SettingsDialog::buildYouTubeTab()
{
    auto *w      = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setAlignment(Qt::AlignTop);

    // Credentials group
    auto *credGroup = new QGroupBox("Google OAuth Credentials");
    auto *form      = new QFormLayout(credGroup);
    m_clientIdEdit     = new QLineEdit;
    m_clientSecretEdit = new QLineEdit;
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    m_clientIdEdit->setText(m_bot->youtube()->tokens().clientId);
    m_clientSecretEdit->setText(m_bot->youtube()->tokens().clientSecret);
    form->addRow("Client ID:",     m_clientIdEdit);
    form->addRow("Client Secret:", m_clientSecretEdit);
    layout->addWidget(credGroup);

    // Instructions
    auto *helpLabel = new QLabel(
        "<b>How to get credentials:</b><br>"
        "1. Go to <a href='https://console.cloud.google.com'>Google Cloud Console</a><br>"
        "2. Create a project → Enable <i>YouTube Data API v3</i><br>"
        "3. OAuth consent screen → External → Add scope <code>youtube</code><br>"
        "4. Create credentials → <i>OAuth client ID</i> → <b>TV and Limited Input devices</b><br>"
        "5. Paste the Client ID and Secret above, then click <b>Connect</b>."
    );
    helpLabel->setOpenExternalLinks(true);
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    // Auth button + status
    auto *row  = new QHBoxLayout;
    m_authBtn  = new QPushButton(m_bot->youtube()->isAuthenticated()
                                     ? "Re-authenticate" : "Connect YouTube");
    m_authStatusLabel = new QLabel(m_bot->youtube()->isAuthenticated()
                                       ? "✔ Authenticated" : "Not authenticated");
    if (m_bot->youtube()->isAuthenticated())
        m_authStatusLabel->setStyleSheet("color: #52e052;");
    row->addWidget(m_authBtn);
    row->addWidget(m_authStatusLabel);
    row->addStretch();
    layout->addLayout(row);

    connect(m_authBtn, &QPushButton::clicked, this, &SettingsDialog::onStartAuth);

    // Wire up device-flow signals (they may have been started before dialog opened)
    connect(m_bot->youtube(), &YouTubeClient::deviceFlowReady,
            this, &SettingsDialog::onDeviceFlowReady);
    connect(m_bot->youtube(), &YouTubeClient::deviceFlowCompleted,
            this, &SettingsDialog::onDeviceFlowCompleted);
    connect(m_bot->youtube(), &YouTubeClient::deviceFlowFailed,
            this, &SettingsDialog::onDeviceFlowFailed);

    layout->addStretch();
    return w;
}

void SettingsDialog::onStartAuth()
{
    m_bot->youtube()->setCredentials(m_clientIdEdit->text().trimmed(),
                                     m_clientSecretEdit->text().trimmed());
    m_authStatusLabel->setText("Starting…");
    m_authStatusLabel->setStyleSheet("");
    m_authBtn->setEnabled(false);
    m_bot->youtube()->startDeviceFlow();
}

void SettingsDialog::onDeviceFlowReady(const QString &url, const QString &code)
{
    m_authStatusLabel->setText("Waiting for authorization…");

    QMessageBox box(this);
    box.setWindowTitle("Authorize Ronin Chat Bot");
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<b>Open this URL in your browser:</b><br>"
        "<a href='%1'>%1</a><br><br>"
        "<b>Enter this code when prompted:</b><br>"
        "<span style='font-size:20pt;letter-spacing:4px;'>%2</span><br><br>"
        "This dialog will close automatically once you approve."
    ).arg(url, code));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
    QDesktopServices::openUrl(QUrl(url));
}

void SettingsDialog::onDeviceFlowCompleted()
{
    m_authStatusLabel->setText("✔ Authenticated");
    m_authStatusLabel->setStyleSheet("color: #52e052;");
    m_authBtn->setText("Re-authenticate");
    m_authBtn->setEnabled(true);
    m_bot->saveConfig();
    QMessageBox::information(this, "Success", "YouTube account connected successfully!");
}

void SettingsDialog::onDeviceFlowFailed(const QString &reason)
{
    m_authStatusLabel->setText("✖ Failed");
    m_authStatusLabel->setStyleSheet("color: #e05252;");
    m_authBtn->setEnabled(true);
    QMessageBox::critical(this, "Auth Failed", reason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Commands tab
// ─────────────────────────────────────────────────────────────────────────────

QWidget *SettingsDialog::buildCommandsTab()
{
    auto *w      = new QWidget;
    auto *layout = new QVBoxLayout(w);

    m_commandsTable = new QTableWidget(0, 4, w);
    m_commandsTable->setHorizontalHeaderLabels({"Trigger", "Response", "Cooldown (s)", "Who"});
    m_commandsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_commandsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commandsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_commandsTable);

    auto *btnRow = new QHBoxLayout;
    auto *addBtn  = new QPushButton("Add");
    auto *editBtn = new QPushButton("Edit");
    auto *delBtn  = new QPushButton("Remove");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(addBtn,  &QPushButton::clicked, this, &SettingsDialog::onAddCommand);
    connect(editBtn, &QPushButton::clicked, this, &SettingsDialog::onEditCommand);
    connect(delBtn,  &QPushButton::clicked, this, &SettingsDialog::onRemoveCommand);

    return w;
}

void SettingsDialog::loadCommandsTable()
{
    m_commandsTable->setRowCount(0);
    static const char *levelNames[] = {"Everyone", "Mods+", "Broadcaster"};

    for (const auto &cmd : m_bot->commands()->commands()) {
        int row = m_commandsTable->rowCount();
        m_commandsTable->insertRow(row);
        m_commandsTable->setItem(row, 0, new QTableWidgetItem(cmd.trigger));
        m_commandsTable->setItem(row, 1, new QTableWidgetItem(cmd.response));
        m_commandsTable->setItem(row, 2, new QTableWidgetItem(QString::number(cmd.cooldownSecs)));
        m_commandsTable->setItem(row, 3, new QTableWidgetItem(levelNames[static_cast<int>(cmd.userLevel)]));
    }
}

void SettingsDialog::onAddCommand()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Add Command");
    auto *form = new QFormLayout(&dlg);

    auto *triggerEdit   = new QLineEdit;
    auto *responseEdit  = new QTextEdit;
    auto *cooldownSpin  = new QSpinBox;
    auto *levelCombo    = new QComboBox;

    cooldownSpin->setRange(0, 3600);
    cooldownSpin->setSuffix(" s");
    levelCombo->addItems({"Everyone", "Mods+", "Broadcaster only"});
    responseEdit->setFixedHeight(80);

    form->addRow("Trigger (e.g. !discord):", triggerEdit);
    form->addRow("Response:",                responseEdit);
    form->addRow("Cooldown:",                cooldownSpin);
    form->addRow("Who can use:",             levelCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    Command cmd;
    cmd.trigger      = triggerEdit->text().trimmed();
    cmd.response     = responseEdit->toPlainText().trimmed();
    cmd.cooldownSecs = cooldownSpin->value();
    cmd.userLevel    = static_cast<UserLevel>(levelCombo->currentIndex());

    if (cmd.trigger.isEmpty() || cmd.response.isEmpty()) return;

    m_bot->commands()->addCommand(cmd);
    loadCommandsTable();
}

void SettingsDialog::onEditCommand()
{
    int row = m_commandsTable->currentRow();
    if (row < 0) return;

    const auto &cmds = m_bot->commands()->commands();
    if (row >= cmds.size()) return;
    Command cmd = cmds[row];

    QDialog dlg(this);
    dlg.setWindowTitle("Edit Command");
    auto *form = new QFormLayout(&dlg);

    auto *triggerEdit   = new QLineEdit(cmd.trigger);
    auto *responseEdit  = new QTextEdit(cmd.response);
    auto *cooldownSpin  = new QSpinBox;
    auto *levelCombo    = new QComboBox;

    cooldownSpin->setRange(0, 3600);
    cooldownSpin->setSuffix(" s");
    cooldownSpin->setValue(cmd.cooldownSecs);
    levelCombo->addItems({"Everyone", "Mods+", "Broadcaster only"});
    levelCombo->setCurrentIndex(static_cast<int>(cmd.userLevel));
    responseEdit->setFixedHeight(80);

    form->addRow("Trigger:", triggerEdit);
    form->addRow("Response:", responseEdit);
    form->addRow("Cooldown:", cooldownSpin);
    form->addRow("Who can use:", levelCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    cmd.trigger      = triggerEdit->text().trimmed();
    cmd.response     = responseEdit->toPlainText().trimmed();
    cmd.cooldownSecs = cooldownSpin->value();
    cmd.userLevel    = static_cast<UserLevel>(levelCombo->currentIndex());

    m_bot->commands()->updateCommand(cmd);
    loadCommandsTable();
}

void SettingsDialog::onRemoveCommand()
{
    int row = m_commandsTable->currentRow();
    if (row < 0) return;

    const auto &cmds = m_bot->commands()->commands();
    if (row >= cmds.size()) return;

    if (QMessageBox::question(this, "Remove Command",
            QStringLiteral("Remove command '%1'?").arg(cmds[row].trigger))
        == QMessageBox::Yes) {
        m_bot->commands()->removeCommand(cmds[row].trigger);
        loadCommandsTable();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Timers tab
// ─────────────────────────────────────────────────────────────────────────────

QWidget *SettingsDialog::buildTimersTab()
{
    auto *w      = new QWidget;
    auto *layout = new QVBoxLayout(w);

    m_timersTable = new QTableWidget(0, 3, w);
    m_timersTable->setHorizontalHeaderLabels({"Message", "Interval (min)", "Enabled"});
    m_timersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_timersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_timersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_timersTable);

    auto *btnRow = new QHBoxLayout;
    auto *addBtn  = new QPushButton("Add");
    auto *editBtn = new QPushButton("Edit");
    auto *delBtn  = new QPushButton("Remove");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(addBtn,  &QPushButton::clicked, this, &SettingsDialog::onAddTimer);
    connect(editBtn, &QPushButton::clicked, this, &SettingsDialog::onEditTimer);
    connect(delBtn,  &QPushButton::clicked, this, &SettingsDialog::onRemoveTimer);

    return w;
}

void SettingsDialog::loadTimersTable()
{
    m_timersTable->setRowCount(0);
    for (const auto &msg : m_bot->timers()->messages()) {
        int row = m_timersTable->rowCount();
        m_timersTable->insertRow(row);
        m_timersTable->setItem(row, 0, new QTableWidgetItem(msg.text));
        m_timersTable->setItem(row, 1, new QTableWidgetItem(QString::number(msg.intervalMinutes)));
        m_timersTable->setItem(row, 2, new QTableWidgetItem(msg.enabled ? "Yes" : "No"));
    }
}

void SettingsDialog::onAddTimer()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Add Timed Message");
    auto *form = new QFormLayout(&dlg);

    auto *msgEdit      = new QTextEdit;
    auto *intervalSpin = new QSpinBox;
    auto *enabledBox   = new QCheckBox("Enabled");

    msgEdit->setFixedHeight(80);
    intervalSpin->setRange(1, 1440);
    intervalSpin->setSuffix(" min");
    intervalSpin->setValue(10);
    enabledBox->setChecked(true);

    form->addRow("Message:", msgEdit);
    form->addRow("Interval:", intervalSpin);
    form->addRow("", enabledBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    TimedMessage tm;
    tm.text            = msgEdit->toPlainText().trimmed();
    tm.intervalMinutes = intervalSpin->value();
    tm.enabled         = enabledBox->isChecked();

    if (tm.text.isEmpty()) return;

    m_bot->timers()->addMessage(tm);
    loadTimersTable();
}

void SettingsDialog::onEditTimer()
{
    int row = m_timersTable->currentRow();
    if (row < 0) return;

    const auto &msgs = m_bot->timers()->messages();
    if (row >= msgs.size()) return;
    TimedMessage tm = msgs[row];

    QDialog dlg(this);
    dlg.setWindowTitle("Edit Timed Message");
    auto *form = new QFormLayout(&dlg);

    auto *msgEdit      = new QTextEdit(tm.text);
    auto *intervalSpin = new QSpinBox;
    auto *enabledBox   = new QCheckBox("Enabled");

    msgEdit->setFixedHeight(80);
    intervalSpin->setRange(1, 1440);
    intervalSpin->setSuffix(" min");
    intervalSpin->setValue(tm.intervalMinutes);
    enabledBox->setChecked(tm.enabled);

    form->addRow("Message:", msgEdit);
    form->addRow("Interval:", intervalSpin);
    form->addRow("", enabledBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    tm.text            = msgEdit->toPlainText().trimmed();
    tm.intervalMinutes = intervalSpin->value();
    tm.enabled         = enabledBox->isChecked();

    m_bot->timers()->updateMessage(row, tm);
    loadTimersTable();
}

void SettingsDialog::onRemoveTimer()
{
    int row = m_timersTable->currentRow();
    if (row < 0) return;

    if (QMessageBox::question(this, "Remove Timer", "Remove this timed message?")
        == QMessageBox::Yes) {
        m_bot->timers()->removeMessage(row);
        loadTimersTable();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AutoMod tab
// ─────────────────────────────────────────────────────────────────────────────

QWidget *SettingsDialog::buildAutoModTab()
{
    auto *w      = new QWidget;
    auto *layout = new QVBoxLayout(w);

    auto *helpLabel = new QLabel(
        "Rules are checked in order. Mods and the broadcaster are always exempt."
    );
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    m_autoModTable = new QTableWidget(0, 5, w);
    m_autoModTable->setHorizontalHeaderLabels({"Label", "Pattern", "Regex", "Action", "Timeout (s)"});
    m_autoModTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_autoModTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_autoModTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_autoModTable);

    auto *btnRow = new QHBoxLayout;
    auto *addBtn  = new QPushButton("Add");
    auto *editBtn = new QPushButton("Edit");
    auto *delBtn  = new QPushButton("Remove");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(addBtn,  &QPushButton::clicked, this, &SettingsDialog::onAddRule);
    connect(editBtn, &QPushButton::clicked, this, &SettingsDialog::onEditRule);
    connect(delBtn,  &QPushButton::clicked, this, &SettingsDialog::onRemoveRule);

    return w;
}

void SettingsDialog::loadAutoModTable()
{
    static const char *actionNames[] = {"None", "Delete", "Timeout", "Ban"};
    m_autoModTable->setRowCount(0);

    for (const auto &rule : m_bot->automod()->rules()) {
        int row = m_autoModTable->rowCount();
        m_autoModTable->insertRow(row);
        m_autoModTable->setItem(row, 0, new QTableWidgetItem(rule.label));
        m_autoModTable->setItem(row, 1, new QTableWidgetItem(rule.pattern));
        m_autoModTable->setItem(row, 2, new QTableWidgetItem(rule.isRegex ? "Yes" : "No"));
        m_autoModTable->setItem(row, 3, new QTableWidgetItem(actionNames[static_cast<int>(rule.action)]));
        m_autoModTable->setItem(row, 4, new QTableWidgetItem(
            rule.action == AutoModAction::TimeoutUser ? QString::number(rule.timeoutSecs) : "—"));
    }
}

static QDialog *buildRuleDialog(AutoModRule &rule, QWidget *parent, const QString &title)
{
    auto *dlg  = new QDialog(parent);
    dlg->setWindowTitle(title);
    auto *form = new QFormLayout(dlg);

    auto *labelEdit     = new QLineEdit(rule.label);
    auto *patternEdit   = new QLineEdit(rule.pattern);
    auto *regexBox      = new QCheckBox("Pattern is a regular expression");
    auto *caseBox       = new QCheckBox("Case sensitive");
    auto *actionCombo   = new QComboBox;
    auto *timeoutSpin   = new QSpinBox;

    regexBox->setChecked(rule.isRegex);
    caseBox->setChecked(rule.caseSensitive);
    actionCombo->addItems({"Delete Message", "Timeout User", "Ban User"});
    actionCombo->setCurrentIndex(qMax(0, static_cast<int>(rule.action) - 1));
    timeoutSpin->setRange(1, 86400);
    timeoutSpin->setSuffix(" s");
    timeoutSpin->setValue(rule.timeoutSecs);

    form->addRow("Label (friendly name):", labelEdit);
    form->addRow("Pattern:", patternEdit);
    form->addRow("", regexBox);
    form->addRow("", caseBox);
    form->addRow("Action:", actionCombo);
    form->addRow("Timeout duration:", timeoutSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, [=, &rule]() {
        rule.label         = labelEdit->text().trimmed();
        rule.pattern       = patternEdit->text().trimmed();
        rule.isRegex       = regexBox->isChecked();
        rule.caseSensitive = caseBox->isChecked();
        rule.action        = static_cast<AutoModAction>(actionCombo->currentIndex() + 1);
        rule.timeoutSecs   = timeoutSpin->value();
        dlg->accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    return dlg;
}

void SettingsDialog::onAddRule()
{
    AutoModRule rule;
    auto *dlg = buildRuleDialog(rule, this, "Add AutoMod Rule");
    if (dlg->exec() == QDialog::Accepted && !rule.pattern.isEmpty()) {
        m_bot->automod()->addRule(rule);
        loadAutoModTable();
    }
    delete dlg;
}

void SettingsDialog::onEditRule()
{
    int row = m_autoModTable->currentRow();
    if (row < 0 || row >= m_bot->automod()->rules().size()) return;

    AutoModRule rule = m_bot->automod()->rules()[row];
    auto *dlg = buildRuleDialog(rule, this, "Edit AutoMod Rule");
    if (dlg->exec() == QDialog::Accepted) {
        m_bot->automod()->updateRule(row, rule);
        loadAutoModTable();
    }
    delete dlg;
}

void SettingsDialog::onRemoveRule()
{
    int row = m_autoModTable->currentRow();
    if (row < 0) return;

    if (QMessageBox::question(this, "Remove Rule", "Remove this AutoMod rule?")
        == QMessageBox::Yes) {
        m_bot->automod()->removeRule(row);
        loadAutoModTable();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Save
// ─────────────────────────────────────────────────────────────────────────────

void SettingsDialog::onSave()
{
    m_bot->saveConfig();
    accept();
}
