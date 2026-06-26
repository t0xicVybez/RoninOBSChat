#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>

// One moderation action the bot has taken this session. Runtime-only — bans are
// session state and are never persisted to config.
struct ModEntry {
    QString   channelId;
    QString   displayName;
    QString   banId;        // empty until YouTube returns it (filled by attachBanId)
    QString   action;       // "Timeout" or "Ban"
    QString   matchedRule;  // friendly rule name / pattern that triggered it
    QDateTime appliedAt;
    QDateTime expiresAt;     // only meaningful for temporary timeouts
    bool      temporary = false;
};

// Tracks the bans/timeouts the bot has issued so they can be lifted again.
// YouTube only lets you remove a ban by its id, so we must remember each one.
class ModerationLog : public QObject {
    Q_OBJECT

public:
    explicit ModerationLog(QObject *parent = nullptr);

    // Record the action immediately (before YouTube replies) so the per-user
    // guard can prevent a burst of messages from stacking duplicate bans.
    void recordPending(const QString &channelId, const QString &displayName,
                       const QString &action, const QString &matchedRule,
                       bool temporary, int durationSeconds);

    // Fill in the banId once YouTube returns it (matched to the newest pending
    // entry for that channel that still lacks an id).
    void attachBanId(const QString &channelId, const QString &banId);

    void removeByBanId(const QString &banId);

    // Drop the newest still-pending (no banId yet) entry for a channel. Called
    // when a ban/timeout POST fails so a failed action can't falsely mark the
    // user as banned forever.
    void removePendingByChannel(const QString &channelId);

    // True if this channel already has an unexpired timeout / active ban (or a
    // pending one awaiting its id). Used to avoid re-banning on every message.
    bool hasActiveBan(const QString &channelId) const;

    // Best-effort lookups for the !untimeout / !unban chat commands. Returns the
    // banId of the most recent active entry, or empty if none / not yet known.
    QString banIdForName(const QString &displayName) const;
    QString banIdForChannel(const QString &channelId) const;

    const QList<ModEntry> &entries() const { return m_entries; }

    // Drop temporary timeouts that have expired on YouTube. Call periodically so
    // the dock list reflects reality; emits changed() if anything was removed.
    void pruneExpired();

signals:
    void changed();

private:
    QList<ModEntry> m_entries;
};
