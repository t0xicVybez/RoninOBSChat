#include "CommandHandler.hpp"
#include <QJsonObject>

CommandHandler::CommandHandler(QObject *parent) : QObject(parent) {}

bool CommandHandler::userMayUse(const ChatMessage &msg, UserLevel level) const
{
    switch (level) {
    case UserLevel::Everyone:        return true;
    case UserLevel::ModsAndUp:       return msg.authorIsModerator || msg.authorIsOwner;
    case UserLevel::BroadcasterOnly: return msg.authorIsOwner;
    }
    return false;
}

bool CommandHandler::process(const ChatMessage &msg, QString &responseOut)
{
    // Commands must start with '!'
    if (!msg.text.startsWith('!')) return false;

    // Grab the first word (the trigger)
    QString trigger = msg.text.section(' ', 0, 0).toLower();

    for (auto &cmd : m_commands) {
        if (cmd.trigger.toLower() != trigger) continue;
        if (!userMayUse(msg, cmd.userLevel)) return false;

        // Cooldown check
        if (cmd.cooldownSecs > 0 && cmd.lastUsed.isValid()) {
            qint64 elapsed = cmd.lastUsed.secsTo(QDateTime::currentDateTime());
            if (elapsed < cmd.cooldownSecs) return false;
        }

        cmd.lastUsed = QDateTime::currentDateTime();
        responseOut = cmd.response;
        return true;
    }
    return false;
}

void CommandHandler::addCommand(const Command &cmd)
{
    // Replace if trigger already exists
    for (auto &existing : m_commands) {
        if (existing.trigger.toLower() == cmd.trigger.toLower()) {
            existing = cmd;
            return;
        }
    }
    m_commands.append(cmd);
}

void CommandHandler::removeCommand(const QString &trigger)
{
    m_commands.removeIf([&](const Command &c) {
        return c.trigger.toLower() == trigger.toLower();
    });
}

void CommandHandler::updateCommand(const Command &cmd)
{
    addCommand(cmd); // addCommand already handles the update path
}

QJsonArray CommandHandler::toJson() const
{
    QJsonArray arr;
    for (const auto &cmd : m_commands) {
        QJsonObject obj;
        obj["trigger"]     = cmd.trigger;
        obj["response"]    = cmd.response;
        obj["cooldown"]    = cmd.cooldownSecs;
        obj["userLevel"]   = static_cast<int>(cmd.userLevel);
        arr.append(obj);
    }
    return arr;
}

void CommandHandler::fromJson(const QJsonArray &arr)
{
    m_commands.clear();
    for (const auto &val : arr) {
        auto obj = val.toObject();
        Command cmd;
        cmd.trigger      = obj["trigger"].toString();
        cmd.response     = obj["response"].toString();
        cmd.cooldownSecs = obj["cooldown"].toInt(0);
        cmd.userLevel    = static_cast<UserLevel>(obj["userLevel"].toInt(0));
        m_commands.append(cmd);
    }
}
