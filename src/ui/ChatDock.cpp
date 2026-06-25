#include "ChatDock.hpp"
#include "SettingsDialog.hpp"

#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QVBoxLayout>

ChatDock::ChatDock(ChatBot *bot, QWidget *parent)
    : QDockWidget("Ronin Chat Bot", parent)
    , m_bot(bot)
{
    setObjectName("RoninOBSChatDock");
    setMinimumWidth(280);

    // ── Root widget ───────────────────────────────────────────────────────────
    auto *root   = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout();
    m_statusLabel  = new QLabel("● Disconnected", root);
    m_statusLabel->setStyleSheet("color: #e05252; font-weight: bold;");
    m_connectBtn   = new QPushButton("Connect", root);
    m_settingsBtn  = new QPushButton("Settings", root);
    m_connectBtn->setMinimumWidth(90);
    m_settingsBtn->setMinimumWidth(70);
    toolbar->addWidget(m_statusLabel);
    toolbar->addStretch();
    toolbar->addWidget(m_connectBtn);
    toolbar->addWidget(m_settingsBtn);
    layout->addLayout(toolbar);

    // ── Chat view ─────────────────────────────────────────────────────────────
    m_chatView = new QListWidget(root);
    m_chatView->setWordWrap(true);
    m_chatView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatView->setSelectionMode(QAbstractItemView::NoSelection);
    m_chatView->setStyleSheet(
        "QListWidget { background: #1a1a1a; color: #ececec; border: none; }"
        "QListWidget::item { padding: 3px 6px; border-bottom: 1px solid #2a2a2a; }"
    );
    layout->addWidget(m_chatView);

    setWidget(root);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_connectBtn,  &QPushButton::clicked, this, &ChatDock::onConnectClicked);
    connect(m_settingsBtn, &QPushButton::clicked, this, &ChatDock::onSettingsClicked);

    connect(m_bot->youtube(), &YouTubeClient::connectionChanged,
            this, &ChatDock::onConnectionChanged);
    connect(m_bot, &ChatBot::messageReceived,
            this, &ChatDock::onMessageReceived);
    connect(m_bot, &ChatBot::botSentMessage,
            this, &ChatDock::onBotSentMessage);
    connect(m_bot, &ChatBot::autoModActionTaken,
            this, &ChatDock::onAutoModAction);
    connect(m_bot->youtube(), &YouTubeClient::errorOccurred,
            this, [this](const QString &err) {
                appendLine(QStringLiteral("<span style='color:#e05252'>⚠ %1</span>").arg(err.toHtmlEscaped()));
            });
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ChatDock::onConnectionChanged(bool connected)
{
    updateStatusLabel(connected);
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    if (connected)
        appendLine("<span style='color:#52e052'>✔ Connected to YouTube live chat</span>");
    else
        appendLine("<span style='color:#e05252'>✖ Disconnected</span>");
}

void ChatDock::onMessageReceived(const ChatMessage &msg)
{
    QString color = msg.authorIsOwner    ? "#ffd700"
                  : msg.authorIsModerator? "#52a8e0"
                  : "#ececec";

    QString line = QStringLiteral(
        "<span style='color:%1;font-weight:bold'>%2</span>"
        "<span style='color:#888'>: </span>"
        "<span>%3</span>"
    ).arg(color,
          msg.authorDisplayName.toHtmlEscaped(),
          msg.text.toHtmlEscaped());

    appendLine(line);
}

void ChatDock::onBotSentMessage(const QString &text)
{
    appendLine(QStringLiteral(
        "<span style='color:#a57be0;font-weight:bold'>🤖 Bot</span>"
        "<span style='color:#888'>: </span>"
        "<span>%1</span>"
    ).arg(text.toHtmlEscaped()));
}

void ChatDock::onAutoModAction(const ChatMessage &msg, const AutoModResult &result)
{
    QString action;
    switch (result.action) {
    case AutoModAction::DeleteMessage: action = "deleted"; break;
    case AutoModAction::TimeoutUser:   action = QStringLiteral("timed out (%1s)").arg(result.timeoutSecs); break;
    case AutoModAction::BanUser:       action = "banned"; break;
    default: break;
    }

    appendLine(QStringLiteral(
        "<span style='color:#e07c52'>🛡 AutoMod: %1 %2 [%3]</span>"
    ).arg(msg.authorDisplayName.toHtmlEscaped(), action, result.matchedRule.toHtmlEscaped()));
}

void ChatDock::onConnectClicked()
{
    if (m_bot->youtube()->isConnected())
        m_bot->youtube()->disconnectFromStream();
    else
        m_bot->youtube()->connectToStream();
}

void ChatDock::onSettingsClicked()
{
    auto *dlg = new SettingsDialog(m_bot, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ChatDock::appendLine(const QString &html)
{
    auto *item = new QListWidgetItem();
    item->setText({}); // text is driven by delegate; we store html as data
    // Use a simple label-based approach via setData
    item->setData(Qt::UserRole, html);

    // For simplicity, strip tags for actual item text
    QString plain = html;
    plain.remove(QRegularExpression("<[^>]*>"));
    item->setText(plain);

    m_chatView->addItem(item);
    // Keep at most 500 lines
    while (m_chatView->count() > 500)
        delete m_chatView->takeItem(0);

    m_chatView->scrollToBottom();
}

void ChatDock::updateStatusLabel(bool connected)
{
    if (connected) {
        m_statusLabel->setText("● Connected");
        m_statusLabel->setStyleSheet("color: #52e052; font-weight: bold;");
    } else {
        m_statusLabel->setText("● Disconnected");
        m_statusLabel->setStyleSheet("color: #e05252; font-weight: bold;");
    }
}
