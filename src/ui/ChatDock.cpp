#include "ChatDock.hpp"
#include "SettingsDialog.hpp"

#include <QDateTime>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QTimer>
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

    // ── Moderation panel ──────────────────────────────────────────────────────
    auto *modLabel = new QLabel("Active timeouts / bans", root);
    modLabel->setStyleSheet("color: #aaa; font-weight: bold; margin-top: 4px;");
    layout->addWidget(modLabel);

    m_modList = new QListWidget(root);
    m_modList->setMaximumHeight(110);
    m_modList->setStyleSheet(
        "QListWidget { background: #1a1a1a; color: #ececec; border: 1px solid #2a2a2a; }"
        "QListWidget::item { padding: 2px 6px; }"
    );
    layout->addWidget(m_modList);

    m_liftBtn = new QPushButton("Lift selected", root);
    m_liftBtn->setEnabled(false);
    layout->addWidget(m_liftBtn);

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

    // Moderation panel wiring
    connect(m_bot->moderation(), &ModerationLog::changed,
            this, &ChatDock::refreshModeration);
    connect(m_liftBtn, &QPushButton::clicked, this, &ChatDock::onLiftSelected);
    connect(m_modList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_liftBtn->setEnabled(m_modList->currentItem() != nullptr);
    });

    // Periodically expire timeouts that YouTube has already auto-lifted.
    auto *pruneTimer = new QTimer(this);
    pruneTimer->setInterval(10000);
    connect(pruneTimer, &QTimer::timeout, this, [this]() {
        m_bot->moderation()->pruneExpired();
    });
    pruneTimer->start();

    refreshModeration();
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
    case AutoModAction::DeleteMessage: action = "deleted message of"; break;
    case AutoModAction::TimeoutUser:   action = QStringLiteral("timed out (%1s)").arg(result.timeoutSecs); break;
    case AutoModAction::BanUser:       action = "banned"; break;
    default: break;
    }

    appendLine(QStringLiteral(
        "<span style='color:#e07c52'>🛡 AutoMod: %1 %2 [rule: %3]</span>"
    ).arg(action, msg.authorDisplayName.toHtmlEscaped(), result.matchedRule.toHtmlEscaped()));
}

void ChatDock::refreshModeration()
{
    m_modList->clear();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (const auto &e : m_bot->moderation()->entries()) {
        QString detail;
        if (e.temporary) {
            qint64 remaining = e.expiresAt.isValid() ? now.secsTo(e.expiresAt) : 0;
            if (remaining < 0) remaining = 0;
            detail = QStringLiteral("Timeout · %1s left").arg(remaining);
        } else {
            detail = "Banned";
        }

        QString label = QStringLiteral("%1 — %2 [%3]")
                            .arg(e.displayName, detail, e.matchedRule);
        if (e.banId.isEmpty())
            label += "  (applying…)";

        auto *item = new QListWidgetItem(label, m_modList);
        item->setData(Qt::UserRole, e.banId); // empty until YouTube responds
    }

    m_liftBtn->setEnabled(m_modList->currentItem() != nullptr);
}

void ChatDock::onLiftSelected()
{
    auto *item = m_modList->currentItem();
    if (!item) return;

    const QString banId = item->data(Qt::UserRole).toString();
    if (banId.isEmpty()) {
        appendLine("<span style='color:#e0a052'>Ban still applying — try again in a moment.</span>");
        return;
    }
    m_bot->youtube()->liftBan(banId);
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
