#ifndef ZMQSUBSCRIBER_H
#define ZMQSUBSCRIBER_H

#include <QThread>
#include <QStringList>
#include <atomic>
#include <zmq.hpp>

// streamer.py (5555) ve predict.py (5556) tarafindan PUB edilen SYS/AI/SPEC
// satirlarini GUI thread'ini bloklamadan okumak icin ayri bir QThread'de
// calisan tek SUB soketi. Ikisine de ayni soketle connect edilir -- ZeroMQ
// bir SUB soketin birden fazla PUB uc noktasina baglanmasina izin verir.
class ZmqSubscriber : public QThread
{
    Q_OBJECT
public:
    explicit ZmqSubscriber(const QStringList &endpoints, QObject *parent = nullptr);
    void stop();

signals:
    void lineReceived(const QString &line);

protected:
    void run() override;

private:
    QStringList mEndpoints;
    std::atomic<bool> mRunning{true};
};

#endif // ZMQSUBSCRIBER_H
