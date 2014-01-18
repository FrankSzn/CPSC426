#include "Router.hh"

Router::Router()
{
    // setup table of destinations for routing
    routingTable = new QHash<QString, QPair<QHostAddress, quint16>* >();
}

void Router::addRoutingNextHop(QString origin, Peer *sender) {

    QHostAddress senderIp = sender->getIpAddress();
    quint16 senderPort = sender->getPort();
    QPair<QHostAddress, quint16>* senderAddr = new QPair<QHostAddress, quint16>(senderIp, senderPort);
    routingTable->insert(origin, senderAddr);

    emit originsListChanged(routingTable->keys());

    //qDebug() << "New routing entry: " << origin + ", " << senderIp.toString() + ", " << senderPort;
}

QPair<QHostAddress, quint16> *Router::lookupOrigin(QString origin) {

    return routingTable->value(origin);
}
