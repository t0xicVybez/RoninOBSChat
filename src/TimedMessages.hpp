#pragma once
#include <QObject>
#include <QList>
#include <QTimer>
#include <QJsonArray>

struct TimedMessage {
    QString text;
    int     intervalMinutes = 10;
    bool    enabled         = true;

    // Runtime only
    QTimer *timer = nullptr;
};

class TimedMessages : public QObject {
    Q_OBJECT

public:
    explicit TimedMessages(QObject *parent = nullptr);
    ~TimedMessages();

    void startAll();
    void stopAll();

    void addMessage(const TimedMessage &msg);
    void removeMessage(int index);
    void updateMessage(int index, const TimedMessage &msg);
    const QList<TimedMessage> &messages() const { return m_messages; }

    QJsonArray toJson() const;
    void fromJson(const QJsonArray &arr);

signals:
    void sendMessage(const QString &text);

private:
    void attachTimer(TimedMessage &msg);
    void detachTimer(TimedMessage &msg);

    QList<TimedMessage> m_messages;
    bool m_running = false;
};
