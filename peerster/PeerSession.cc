#include "PeerSession.hh"

PeerSession::PeerSession(Peer *peer, QList<int> *assignedIndices) {
    this->peer = peer;
    this->assignedIndices = assignedIndices;
}

// Emit a signal to send the correct data chunk
void PeerSession::startSession() {
    emit sendDataChunk(assignedIndices->first(), peer);
}

// If results received for the session, send the data chunk at next assigned index
// return false if already sent all data chunks for the assigned indices
bool PeerSession::receivedResultForSession() {

    assignedIndices->removeFirst();

    if (assignedIndices->isEmpty()) return false;

    emit sendDataChunk(assignedIndices->first(), peer);
    return true;
}

// Returns how much load (indices) the peer still has to process
int PeerSession::getLoadStillToProcess() {
    return assignedIndices->size();
}

// Allow reassignment of peer for load balancing purposes
void PeerSession::reassignPeer(Peer *peer) {
    this->peer = peer;
    this->startSession();
}

Peer *PeerSession::getPeer() {
    return this->peer;
}
