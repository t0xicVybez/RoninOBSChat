#pragma once
#include <QObject>
#include <QList>
#include <QDateTime>
#include <QJsonArray>
#include "ChatMessage.hpp"

enum class UserLevel { Everyone, ModsAndUp, BroadcasterOnly };

struct Command {
    QString   trigger;       // e.g. "!discord"
    QString   response;      // e.g. "Join us at discord.gg/xyz"
    int       cooldownSecs = 0;
    UserLevel userLevel    = UserLevel::Everyone;

    // Runtime only – not persisted
    QDateTime lastUsed;
};

class CommandHandler : public QObject {
    Q_OBJECT

public:
    explicit CommandHandler(QObject *parent = nullptr);

    // Returns true and sets responseOut if a command was matched + should fire
    bool process(const ChatMessage &msg, QString &responseOut);

    // CRUD
    void addCommand(const Command &cmd);
    void removeCommand(const QString &trigger);
    void updateCommand(const Command &cmd);
    const QList<Command> &commands() const { return m_commands; }

    // Persistence (JSON)
    QJsonArray toJson() const;
    void fromJson(const QJsonArray &arr);

private:
    bool userMayUse(const ChatMessage &msg, UserLevel level) const;

    QList<Command> m_commands;
};
