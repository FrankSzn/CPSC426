#ifndef PEER_HH
#define PEER_HH

#include <QObject>
#include <QHostInfo>
#include <QHostAddress>
#include <QString>

class Peer : public QObject
{

    Q_OBJECT

public:
    Peer(QString hostname, QHostAddress ipAddr, quint16 udpPort);
    Peer(QString hostString);
    QString getHostName();
    QHostAddress getIpAddress();
    quint16 getPort();
    bool isEnabled();

private:
    QString hostName;
    QHostAddress ipAddress;
    quint16 port;
    bool enabled;

public slots:
    void setHostEnabled(QHostInfo info);
};

#endif // PEER_HH
