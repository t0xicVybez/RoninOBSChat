#include "TimedMessages.hpp"
#include <QJsonObject>

TimedMessages::TimedMessages(QObject *parent) : QObject(parent) {}

TimedMessages::~TimedMessages()
{
    stopAll();
}

// ── Timer management ──────────────────────────────────────────────────────────

void TimedMessages::attachTimer(TimedMessage &msg)
{
    if (msg.timer) return;
    msg.timer = new QTimer(this);
    msg.timer->setInterval(msg.intervalMinutes * 60 * 1000);
    msg.timer->setSingleShot(false);
    connect(msg.timer, &QTimer::timeout, this, [this, &msg]() {
        emit sendMessage(msg.text);
    });
    if (m_running && msg.enabled)
        msg.timer->start();
}

void TimedMessages::detachTimer(TimedMessage &msg)
{
    if (!msg.timer) return;
    msg.timer->stop();
    msg.timer->deleteLater();
    msg.timer = nullptr;
}

// ── Public API ────────────────────────────────────────────────────────────────

void TimedMessages::startAll()
{
    m_running = true;
    for (auto &msg : m_messages) {
        if (msg.enabled && msg.timer)
            msg.timer->start();
    }
}

void TimedMessages::stopAll()
{
    m_running = false;
    for (auto &msg : m_messages) {
        if (msg.timer)
            msg.timer->stop();
    }
}

void TimedMessages::addMessage(const TimedMessage &msg)
{
    m_messages.append(msg);
    attachTimer(m_messages.last());
}

void TimedMessages::removeMessage(int index)
{
    if (index < 0 || index >= m_messages.size()) return;
    detachTimer(m_messages[index]);
    m_messages.removeAt(index);
}

void TimedMessages::updateMessage(int index, const TimedMessage &updated)
{
    if (index < 0 || index >= m_messages.size()) return;
    detachTimer(m_messages[index]);
    m_messages[index] = updated;
    attachTimer(m_messages[index]);
}

// ── Persistence ───────────────────────────────────────────────────────────────

QJsonArray TimedMessages::toJson() const
{
    QJsonArray arr;
    for (const auto &msg : m_messages) {
        QJsonObject obj;
        obj["text"]     = msg.text;
        obj["interval"] = msg.intervalMinutes;
        obj["enabled"]  = msg.enabled;
        arr.append(obj);
    }
    return arr;
}

void TimedMessages::fromJson(const QJsonArray &arr)
{
    stopAll();
    for (auto &msg : m_messages) detachTimer(msg);
    m_messages.clear();

    for (const auto &val : arr) {
        auto obj = val.toObject();
        TimedMessage msg;
        msg.text            = obj["text"].toString();
        msg.intervalMinutes = obj["interval"].toInt(10);
        msg.enabled         = obj["enabled"].toBool(true);
        m_messages.append(msg);
        attachTimer(m_messages.last());
    }
}
