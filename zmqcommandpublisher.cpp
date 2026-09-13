#include "zmqcommandpublisher.h"

ZmqCommandPublisher::ZmqCommandPublisher(const QString &bindEndpoint, QObject *parent)
    : QObject(parent), mContext(1), mSocket(mContext, zmq::socket_type::pub)
{
    mSocket.bind(bindEndpoint.toStdString());
}

void ZmqCommandPublisher::sendLine(const QString &line)
{
    const std::string payload = line.toStdString();
    // dontwait: PUB soketi bu senaryoda (dusuk hizli komut trafigi) asla
    // dolmaz, ama yine de GUI thread'inin bloklanmasini istemiyoruz.
    mSocket.send(zmq::buffer(payload), zmq::send_flags::dontwait);
}
