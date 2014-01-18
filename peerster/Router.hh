#ifndef ROUTER_HH
#define ROUTER_HH

#include <QHash>
#include <QVariantMap>
#include "Peer.hh"

class Router : public QObject
{
    Q_OBJECT

public:
    Router();
    void addRoutingNextHop(QString origin, Peer *sender);
    QPair<QHostAddress, quint16> *lookupOrigin(QString origin);

private:
    QHash<QString, QPair<QHostAddress,quint16>* > *routingTable;

signals:
    void originsListChanged(QList<QString> originsList);
};

#endif // ROUTER_HH
