#include "ModerationLog.hpp"

ModerationLog::ModerationLog(QObject *parent) : QObject(parent) {}

void ModerationLog::recordPending(const QString &channelId, const QString &displayName,
                                  const QString &action, const QString &matchedRule,
                                  bool temporary, int durationSeconds)
{
    ModEntry e;
    e.channelId   = channelId;
    e.displayName = displayName;
    e.action      = action;
    e.matchedRule = matchedRule;
    e.temporary   = temporary;
    e.appliedAt   = QDateTime::currentDateTimeUtc();
    if (temporary)
        e.expiresAt = e.appliedAt.addSecs(durationSeconds);

    m_entries.append(e);
    emit changed();
}

void ModerationLog::attachBanId(const QString &channelId, const QString &banId)
{
    // Match the newest pending entry for this channel that still lacks an id.
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].channelId == channelId && m_entries[i].banId.isEmpty()) {
            m_entries[i].banId = banId;
            emit changed();
            return;
        }
    }
}

void ModerationLog::removeByBanId(const QString &banId)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].banId == banId) {
            m_entries.removeAt(i);
            emit changed();
            return;
        }
    }
}

bool ModerationLog::hasActiveBan(const QString &channelId) const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const auto &e : m_entries) {
        if (e.channelId != channelId) continue;
        if (!e.temporary) return true;                 // permanent ban
        if (!e.expiresAt.isValid() || e.expiresAt > now) return true; // active / pending timeout
    }
    return false;
}

QString ModerationLog::banIdForChannel(const QString &channelId) const
{
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].channelId == channelId && !m_entries[i].banId.isEmpty())
            return m_entries[i].banId;
    }
    return {};
}

QString ModerationLog::banIdForName(const QString &displayName) const
{
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].displayName.compare(displayName, Qt::CaseInsensitive) == 0
            && !m_entries[i].banId.isEmpty())
            return m_entries[i].banId;
    }
    return {};
}

void ModerationLog::pruneExpired()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool removed = false;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const auto &e = m_entries[i];
        if (e.temporary && e.expiresAt.isValid() && e.expiresAt <= now) {
            m_entries.removeAt(i);
            removed = true;
        }
    }
    if (removed) emit changed();
}
