#pragma once
#include <QObject>
#include <QList>
#include <QRegularExpression>
#include <QJsonArray>
#include "ChatMessage.hpp"

enum class AutoModAction { None, DeleteMessage, TimeoutUser, BanUser };

struct AutoModRule {
    QString       pattern;
    bool          isRegex      = false;
    bool          caseSensitive= false;
    AutoModAction action       = AutoModAction::DeleteMessage;
    int           timeoutSecs  = 300; // used when action == TimeoutUser
    QString       label;              // friendly name shown in UI
};

struct AutoModResult {
    AutoModAction action = AutoModAction::None;
    QString       matchedRule;
    int           timeoutSecs = 0;
};

class AutoMod : public QObject {
    Q_OBJECT

public:
    explicit AutoMod(QObject *parent = nullptr);

    AutoModResult check(const ChatMessage &msg) const;

    void addRule(const AutoModRule &rule);
    void removeRule(int index);
    void updateRule(int index, const AutoModRule &rule);
    const QList<AutoModRule> &rules() const { return m_rules; }

    // Mods and the broadcaster are never moderated
    bool isExempt(const ChatMessage &msg) const;

    QJsonArray toJson() const;
    void fromJson(const QJsonArray &arr);

private:
    QList<AutoModRule> m_rules;
};
