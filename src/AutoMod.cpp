#include "AutoMod.hpp"
#include <QJsonObject>

AutoMod::AutoMod(QObject *parent) : QObject(parent) {}

bool AutoMod::isExempt(const ChatMessage &msg) const
{
    return msg.authorIsModerator || msg.authorIsOwner;
}

AutoModResult AutoMod::check(const ChatMessage &msg) const
{
    if (isExempt(msg)) return {};

    for (const auto &rule : m_rules) {
        bool matched = false;

        if (rule.isRegex) {
            QRegularExpression::PatternOptions opts =
                rule.caseSensitive ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(rule.pattern, opts);
            matched = re.match(msg.text).hasMatch();
        } else {
            Qt::CaseSensitivity cs = rule.caseSensitive ? Qt::CaseSensitive
                                                        : Qt::CaseInsensitive;
            matched = msg.text.contains(rule.pattern, cs);
        }

        if (matched) {
            AutoModResult result;
            result.action      = rule.action;
            result.matchedRule = rule.label.isEmpty() ? rule.pattern : rule.label;
            result.timeoutSecs = rule.timeoutSecs;
            return result;
        }
    }
    return {};
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

void AutoMod::addRule(const AutoModRule &rule)
{
    m_rules.append(rule);
}

void AutoMod::removeRule(int index)
{
    if (index >= 0 && index < m_rules.size())
        m_rules.removeAt(index);
}

void AutoMod::updateRule(int index, const AutoModRule &rule)
{
    if (index >= 0 && index < m_rules.size())
        m_rules[index] = rule;
}

// ── Persistence ───────────────────────────────────────────────────────────────

QJsonArray AutoMod::toJson() const
{
    QJsonArray arr;
    for (const auto &rule : m_rules) {
        QJsonObject obj;
        obj["label"]         = rule.label;
        obj["pattern"]       = rule.pattern;
        obj["isRegex"]       = rule.isRegex;
        obj["caseSensitive"] = rule.caseSensitive;
        obj["action"]        = static_cast<int>(rule.action);
        obj["timeoutSecs"]   = rule.timeoutSecs;
        arr.append(obj);
    }
    return arr;
}

void AutoMod::fromJson(const QJsonArray &arr)
{
    m_rules.clear();
    for (const auto &val : arr) {
        auto obj = val.toObject();
        AutoModRule rule;
        rule.label         = obj["label"].toString();
        rule.pattern       = obj["pattern"].toString();
        rule.isRegex       = obj["isRegex"].toBool(false);
        rule.caseSensitive = obj["caseSensitive"].toBool(false);
        rule.action        = static_cast<AutoModAction>(obj["action"].toInt(1));
        rule.timeoutSecs   = obj["timeoutSecs"].toInt(300);
        m_rules.append(rule);
    }
}
