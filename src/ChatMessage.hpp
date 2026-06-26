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

    // True for chat history pulled on connect (or anything published before we
    // connected). Displayed in the dock but never moderated, so AutoMod can't
    // retroactively punish messages from before the bot was running.
    bool isHistorical = false;
};
