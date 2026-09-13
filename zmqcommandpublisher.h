#ifndef ZMQCOMMANDPUBLISHER_H
#define ZMQCOMMANDPUBLISHER_H

#include <QObject>
#include <QString>
#include <zmq.hpp>

// GUI thread'inde yasayan PUB soketi -- arayuzden streamer.py ve predict.py'ye
// komut gondermek icin (JAM_START/JAM_STOP, SDR_VERISI_ISTEK). Python
// taraflarinin ikisi de bu porta SUB olarak baglandigi icin burada bind edilir
// (streamer.py/predict.py'deki "tcp://127.0.0.1:5557" connect ile eslesmeli).
class ZmqCommandPublisher : public QObject
{
    Q_OBJECT
public:
    explicit ZmqCommandPublisher(const QString &bindEndpoint, QObject *parent = nullptr);

    void sendLine(const QString &line);

private:
    zmq::context_t mContext;
    zmq::socket_t mSocket;
};

#endif // ZMQCOMMANDPUBLISHER_H
