#pragma once
#include <QString>
#include <QDateTime>

struct ChatMessage {
    QString id;
    QString authorChannelId;
    QString authorDisplayName;
    bool authorIsModerator = false;
    bool authorIsOwner = false;
    QString text;
    QDateTime publishedAt;
};
