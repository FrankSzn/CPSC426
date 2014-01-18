#ifndef PEERSESSION_HH
#define PEERSESSION_HH

#include <QObject>
#include <QList>
#include "Peer.hh"

// A class for handling a single peer session in distributed image matching

class PeerSession: public QObject {

    Q_OBJECT

public:
    PeerSession(Peer *peer, QList<int> *assignedIndices);
    void startSession();
    bool receivedResultForSession();
    int getLoadStillToProcess();
    void reassignPeer(Peer *peer);
    Peer *getPeer();

private:
    Peer *peer;
    QList<int> *assignedIndices;

signals:
    void sendDataChunk(int idx, Peer *peer);

};

#endif // PEERSESSION_HH
