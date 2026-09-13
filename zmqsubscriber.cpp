#include "zmqsubscriber.h"

ZmqSubscriber::ZmqSubscriber(const QStringList &endpoints, QObject *parent)
    : QThread(parent), mEndpoints(endpoints)
{
}

void ZmqSubscriber::stop()
{
    mRunning = false;
    wait();
}

void ZmqSubscriber::run()
{
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    for (const QString &endpoint : mEndpoints)
        subscriber.connect(endpoint.toStdString());
    subscriber.set(zmq::sockopt::subscribe, "");
    subscriber.set(zmq::sockopt::rcvtimeo, 200);

    while (mRunning)
    {
        zmq::message_t message;
        const auto result = subscriber.recv(message, zmq::recv_flags::none);
        if (!result)
            continue; // zaman asimi (200ms), mRunning'i tekrar kontrol etmek icin dongude devam

        const QString line = QString::fromUtf8(static_cast<const char *>(message.data()), int(message.size())).trimmed();
        if (!line.isEmpty())
            emit lineReceived(line);
    }
}
