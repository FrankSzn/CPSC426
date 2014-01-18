#include <QStringList>
#include <QRegExp>

#include "Peer.hh"

Peer::Peer(QString hostname, QHostAddress ipAddr, quint16 udpPort) {

    hostName = hostname;
    ipAddress = ipAddr;
    port = udpPort;
    enabled = true;
}

Peer::Peer (QString hostString) {

    QStringList list = hostString.split(QRegExp(":"));
    if (list.size() == 2) {

        if(QHostAddress(list.at(0)).isNull()) {

            // hostname:port provided
            QString hostNameProvided = list.at(0);
            QHostInfo::lookupHost(hostNameProvided, this, SLOT(setHostEnabled(QHostInfo)));
            enabled = false;

        } else {

            // ipaddr:port provided
            ipAddress = QHostAddress(list.at(0));
            enabled = true;
        }

        hostName = "";
        port = list.at(1).toUInt();

        qDebug() << "Added new peer: " + hostString;

    } else {

        qDebug() << "Error: Host string provided is not well formed: " + hostString;
    }
}

QString Peer::getHostName() {
    return hostName;
}

QHostAddress Peer::getIpAddress() {
    return ipAddress;
}

quint16 Peer::getPort() {
    return port;
}

bool Peer::isEnabled() {
    return enabled;
}

void Peer::setHostEnabled(QHostInfo info) {
    if (!info.addresses().isEmpty()) {
        ipAddress = info.addresses().first();
        enabled = true;
    }
}
