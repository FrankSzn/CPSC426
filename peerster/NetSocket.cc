#include <unistd.h>
#include <QHostInfo>
#include <time.h>
#include <QDataStream>
#include <iostream>
#include <sstream>

#include "NetSocket.hh"

/* Constructor and binding application to a port
======================================================================================================================================================================*/

NetSocket::NetSocket(bool noForward)
{
    // Pick a range of four UDP ports to try to allocate by default,
    // computed based on my Unix user ID.
    // This makes it trivial for up to four Peerster instances per user
    // to find each other on the same host,
    // barring UDP port conflicts with other applications
    // (which are quite possible).
    // We use the range from 32768 to 49151 for this purpose.
    myPortMin = 32768 + (getuid() % 4096)*4;
    myPortMax = myPortMin + 3;

    noForwardFlag = noForward;
}

bool NetSocket::bind()
{
    // Try to bind to each of the range myPortMin..myPortMax in turn.
    for (int p = myPortMin; p <= myPortMax; p++) {

        if (QUdpSocket::bind(p)) {
            qDebug() << "bound to UDP port " << p;
            myCurrentPort = p;

            // setup neighbors list and their timers
            neighborsList = getLocalNeighborsList(myCurrentPort);
            neighborTimers = new QMap<Peer*, QTimer*>();
            for (int idx = 0; idx < neighborsList->size(); idx++) {
                QTimer *timer = new QTimer(this);
                timer->setSingleShot(true);
                connect(timer,SIGNAL(timeout()),this,SLOT(onNeighborTimerTimeout()));
                neighborTimers->insert(neighborsList->value(idx), timer);
            }

            // setup image processor class with neighbors list
            imageProcessor = new ImageProcessor(neighborsList);
            connect(this->imageProcessor, SIGNAL(sendImageChunk(QPair<QVector<uint>*,QVector<uint>*>*,int,Peer*)),
                    this, SLOT(sendImageChunkToPeer(QPair<QVector<uint>*,QVector<uint>*>*,int,Peer*)));

            // set up message manager after choosing a host identifier
            srand(time(NULL));		// sets up rand() with random seed
            int randomIdentifier = rand();
            hostIdentifier = QHostInfo::localHostName() + ":" + QString::number(myCurrentPort) +
                    "-" + QString::number(randomIdentifier);
            //hostIdentifier = QHostInfo::localHostName().append(QString::number(p));
            messageManager = new MessageManager(hostIdentifier);

            fileShareManager = new FileShareManager();

            // setup the router class
            router = new Router();

            // wire up the initialized socket to receive messages
            connect(this, SIGNAL(readyRead()),
                    this, SLOT(readMessage()));

            // Anti-entropy: periodically send status message to random neighbor to
            // ensure that all neighbors receive all the messages
            setupBackgroundTimer();

            // Periodically generate route rumor message to "announce" oneself
            setupPeriodicRouteRumors();

            return true;


        }
    }

    qDebug() << "Oops, no ports in my default range " << myPortMin
             << "-" << myPortMax << " available";
    return false;
}



/* Periodic Tasks
======================================================================================================================================================================*/


void NetSocket::setupBackgroundTimer() {
    QTimer *timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(startRumormongering()));
    timer->start(START_RUMORMONGERING_INTERVAL);

}

void NetSocket::setupPeriodicRouteRumors() {

    // send first route rumor message on startup
    sendRouteRumorMessage();

    QTimer *timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(sendRouteRumorMessage()));
    timer->start(ROUTE_RUMOR_MESSAGE_INTERVAL);
}

void NetSocket::startRumormongering() {
    Peer *neighbor = pickRandomNeighbor();
    QVariantMap *statusMessage = messageManager->getCurrentStatusMessage();
    if (neighbor != NULL) sendStatusMessage(statusMessage, neighbor);
}

void NetSocket::setupPeriodicSearchRequests() {

    searchRequestsTimer = new QTimer(this);
    connect(searchRequestsTimer,SIGNAL(timeout()),this,SLOT(sendPeriodicSearchRequest()));
    searchRequestsTimer->start(SEARCH_INTERVAL);
}


/* Sending messages from port
======================================================================================================================================================================*/

void NetSocket::sendMessage(QVariantMap *messageMap, Peer *peer) {

    // Serialize the Message map into a byte array
    QByteArray messageBytes;
    QDataStream *messageStream = new QDataStream(&messageBytes, QIODevice::WriteOnly);
    (*messageStream) << (*messageMap);
    delete messageStream;

    // Send the message over the network to the destination port
    writeDatagram(messageBytes, peer->getIpAddress(), peer->getPort());
}

void NetSocket::sendRumorMessage(QVariantMap *messageMap, Peer *neighbor) {

    sendMessage(messageMap, neighbor);

    // Wait for status message receipt from the destination or timeout!!
    startNeighborsTimer(neighbor);
}

void NetSocket::sendRumorMessageToAllNeighbors(QVariantMap *messageMap) {

    for (int i = 0; i < neighborsList->size(); i++) {
        sendRumorMessage(messageMap, neighborsList->at(i));
    }
}

void NetSocket::sendNewRumorMessage(QString message) {

    // Create a Map for the rumor message
    QVariantMap *messageMap = messageManager->createNewRumorMessage(message);
    Peer *randomNeighbor = pickRandomNeighbor();
    if (randomNeighbor != NULL) sendRumorMessage(messageMap, randomNeighbor);
}

/* a slot function that's executed periodically */
void NetSocket::sendRouteRumorMessage() {

    //qDebug() << "Sending route rumor message.";
    sendNewRumorMessage(NULL);
}

void NetSocket::sendStatusMessage(QVariantMap *messageMap, Peer *neighbor) {

    sendMessage(messageMap, neighbor);
}

void NetSocket::sendPrivateMessage(QString destination, QString message, quint32 hopLimit) {

    QVariantMap *privateMessage = messageManager->createNewPrivateMessage(destination,message,hopLimit);
    QPair<QHostAddress, quint16> *host = router->lookupOrigin(destination);

    if (host != NULL) {

        Peer *peer = searchForPeer(host->first, host->second);
        if(peer == NULL) return;        // shouldn't really happen but just for safety!

        sendMessage(privateMessage, peer);
    }
}

void NetSocket::sendNewPrivateMessage(QString destination, QString message) {

    sendPrivateMessage(destination, message, HOP_LIMIT);
}

void NetSocket::sendFileRequestMessage(QVariantMap *message, Peer *peer) {

    sendMessage(message, peer);

    // for now assuming that we always get a response

    // TODO: wait for a block response message or timeout
    //startNeighborsTimer(peer);
}

void NetSocket::sendBlockRequestMessage(QVariantMap *message, Peer *peer) {

    sendMessage(message, peer);
}

void NetSocket::sendBlockReplyMessage(QVariantMap *message, Peer *peer) {

    sendMessage(message, peer);
}

void NetSocket::sendSearchRequestMessage(QVariantMap *message) {

    QString origin = message->value("Origin").toString();
    QString searchKeywords = message->value("Search").toString();
    quint32 budget = message->value("Budget").toUInt();
    QVariantMap *newMessage;

    // Corner case of having more peers than budget
    if ((int) budget <= neighborsList->length()) {

        newMessage = messageManager->createSearchRequestMessage(origin, searchKeywords, 1);
        for (int i = 0; i < (int) budget; i++) {

            Peer *peer = neighborsList->at(i);
            sendMessage(newMessage, peer);
        }
    } else {

        int minBudgetPerPeer = budget / neighborsList->length();
        int numPeersWithSurplusBudget = budget % neighborsList->length();

        newMessage = messageManager->createSearchRequestMessage(origin, searchKeywords, minBudgetPerPeer + 1);
        for (int i = 0; i < numPeersWithSurplusBudget; i++) {

            Peer *peer = neighborsList->at(i);
            sendMessage(newMessage, peer);
        }

        newMessage = messageManager->createSearchRequestMessage(origin, searchKeywords, minBudgetPerPeer);
        for (int i = numPeersWithSurplusBudget; i < neighborsList->length(); i++) {

            Peer *peer = neighborsList->at(i);
            sendMessage(newMessage, peer);
        }
    }
}

void NetSocket::sendSearchReplyMessage(QVariantMap *message, Peer *peer) {

    sendMessage(message, peer);
}

void NetSocket::sendImageChunkToPeer(QPair<QVector<uint>*, QVector<uint>* >* imageChunk, int idx, Peer *peer) {
    qDebug() << "Sending image portion #" << idx;

    QVariantMap *imageChunkMessage = messageManager->createNewImageChunk(imageChunk, idx);
    sendMessage(imageChunkMessage, peer);
}


/* Neighbors/Peers list management
======================================================================================================================================================================*/

QList<Peer*> *NetSocket::getLocalNeighborsList(int myPort) {

    QList<Peer*> *neighbors = new QList<Peer*>();

    //QString hostName = QHostInfo::localHostName();
    QString hostName = "";
    QHostAddress ipAddress = QHostAddress::LocalHost;

    for (int port = myPortMin; port < myPortMax; port++) {
        if (port == myPort) continue;

        Peer *peer = new Peer(hostName, ipAddress, port);
        neighbors->append(peer);
    }

    // if(myPort == myPortMin) {
    // 	Peer *peer = new Peer(hostName, ipAddress, myPort + 1);
    // 	neighbors->append(peer);
    // } else if (myCurrentPort == myPortMax) {
    // 	Peer *peer = new Peer(hostName, ipAddress, myPort - 1);
    // 	neighbors->append(peer);
    // } else {
    // 	Peer *peer1 = new Peer(hostName, ipAddress, myPort + 1);
    // 	Peer *peer2 = new Peer(hostName, ipAddress, myPort - 1);
    // 	neighbors->append(peer1);
    // 	neighbors->append(peer2);
    // }

    return neighbors;
}

void NetSocket::addNewNeighbor(QString hostString) {

    Peer *newPeer = new Peer(hostString);

    // TODO: check if the peer already exists in the list!!
    neighborsList->append(newPeer);
}

void NetSocket::addNewNeighbor(QString hostName, QHostAddress hostIp, quint16 hostPort) {

    Peer *newPeer = new Peer(hostName, hostIp, hostPort);

    // TODO: check if the peer already exists in the list!!
    neighborsList->append(newPeer);
}

Peer *NetSocket::pickRandomNeighbor() {

    if (neighborsList == NULL || neighborsList->size() == 0) return NULL;

    int random = qrand() % neighborsList->size();

    // If the neighbor picked is not enabled then recursively pick another one
    if (!neighborsList->value(random)->isEnabled()) {
        return pickRandomNeighbor();
    } else {
        return neighborsList->value(random);
    }
}

//Peer *NetSocket::pickRandomNeighbor(Peer *exclude) {

//	if (neighborsList == NULL || neighborsList->size() == 0) return NULL;

//	int timesToTry = 5;
//	for (int i = 0; i < timesToTry; i++) {
//		int random = qrand() % neighborsList->size();
//		Peer *randomNeighbor = neighborsList->value(random);

//		// if (!areSameNeighbors(randomNeighbor, exclude) && neighborsList->value(random)->isEnabled()) {
//		// 	return randomNeighbor;
//		// }
//		if (!areSameNeighbors(randomNeighbor, exclude))	return randomNeighbor;
//	}

//	return NULL;
//}

bool NetSocket::areSameNeighbors(Peer *peer1, Peer *peer2) {

    //QString hostName1 = peer1->getHostName();
    QHostAddress ipAddr1 = peer1->getIpAddress();
    quint16 port1 = peer2->getPort();

    //QString hostName2 = peer2->getHostName();
    QHostAddress ipAddr2 = peer2->getIpAddress();
    quint16 port2 = peer2->getPort();

    /*hostName1.compare(hostName2, Qt::CaseSensitive) == 0 &&*/
    if (ipAddr1 == ipAddr2 && port1 == port2) {

        return true;
    }

    return false;

}

// Adds the neighbor if it doesn't already exist in the Neighbors list
bool NetSocket::dynamicAddNeighbor(Peer *newPeer) {

    for (int idx = 0; idx < neighborsList->size(); idx++) {
        if (!areSameNeighbors(newPeer,neighborsList->value(idx))) {
            neighborsList->append(newPeer);
            return true;
        }
    }
    return false;
}

Peer *NetSocket::searchForPeer(QHostAddress ipAddress, quint16 port) {

    for (int idx = 0; idx < neighborsList->size(); idx++) {
        Peer *peer = neighborsList->at(idx);
        if (peer->getIpAddress() == ipAddress && peer->getPort() == port) {
            return peer;
        }
    }
    return NULL;
}

void NetSocket::startNeighborsTimer(Peer *neighbor) {

    if (neighborTimers->contains(neighbor)) {
        QTimer *timer = neighborTimers->find(neighbor).value();
        timer->start(NEIGHBOR_TIMER_DURATION);
    }
}

bool NetSocket::stopNeighborsTimer(Peer *neighbor) {

    if (neighborTimers->contains(neighbor)) {
        QTimer *neighborTimer = neighborTimers->find(neighbor).value();
        neighborTimer->stop();
        return true;
    }
    return false;
}

void NetSocket::onNeighborTimerTimeout() {
    // Do stuff here if did not receive response from the neighbor in allocated time
    //qDebug() << "Timer timed out!";
}


/* Reading messages from port
======================================================================================================================================================================*/

void NetSocket::readMessage() {

    // Create data structures to hold incoming message bytes and sender's network info
    QByteArray messageBytes;
    messageBytes.resize(pendingDatagramSize());
    QHostAddress senderIp;
    quint16 senderPort;

    // Read incoming message into a byte array
    readDatagram(messageBytes.data(), messageBytes.size(), &senderIp, &senderPort);

    // Deserialize received message into a Message map
    QVariantMap messageMap;
    QDataStream *messageStream = new QDataStream(&messageBytes, QIODevice::ReadOnly);
    (*messageStream) >> messageMap;
    delete messageStream;

    //QString hostName = QHostInfo::fromName(peerIp.toString()).hostName();
    QString hostName = "";
    Peer *sender = new Peer(hostName, senderIp, senderPort);

    // if we haven't seen the neighbor before, add to PeerList
    dynamicAddNeighbor(sender);

    if (messageManager->isRouteRumorMessage(messageMap)) {
        // Message is route rumor message

        //qDebug() << "Received route rumor message";

        gotRumorMessage(messageMap, sender, true);

    } else if (messageManager->isValidRumorMessage(messageMap)) {
        // message is valid rumor message
        //qDebug() << "Received rumor message";

        gotRumorMessage(messageMap, sender, false);

    } else if (messageManager->isValidStatusMessage(messageMap)) {
        // Message is of type Status Message

        //qDebug() << "Received status message!";

        gotStatusMessage(messageMap, sender);

    } else if (messageManager->isValidPrivateMessage(messageMap)) {
        // Message is of type Private Message

        //qDebug() << "Received private message";

        gotPrivateMessage(messageMap);

    } else if (messageManager->isValidBlockRequest(messageMap)) {
        // Message if of type Block Request

        qDebug() << "Received block request message";

        gotBlockRequest(messageMap, sender);

    } else if (messageManager->isValidBlockReply(messageMap)) {
        // Message if of type Block Reply

        qDebug() << "Received block reply message";

        gotBlockReply(messageMap, sender);

    } else if (messageManager->isValidSearchRequest(messageMap)) {
        // Message is of type Search Request

        qDebug() << "Received search request message";

        gotSearchRequest(messageMap, sender);

    } else if (messageManager->isValidSearchReply(messageMap)) {
        // Message is of type Search Reply

        qDebug() << "Received search reply message";

        gotSearchReply(messageMap);

    } else if (messageManager->isValidImageChunk(messageMap)) {
        // Message is of type Image chunk

        qDebug() << "Received image chunk to process";

        gotImageChunk(messageMap, sender);
    } else if (messageManager->isValidImageCompResult(messageMap)) {
        // Message is of type image chunk result

        qDebug() << "Received result for image chunk";

        gotImageCompResult(messageMap);
    }
}

void NetSocket::gotRumorMessage(QVariantMap messageMap, Peer *sender, bool isRouteRumor) {

    bool sawMessageBefore = messageManager->messageExistsInDatabase(messageMap);

    // Add sender info to routing table
    QString origin = messageMap.value("Origin").toString();
    // If reseen message contains direct route and we had indirect route then override
    if (sawMessageBefore && origin != hostIdentifier && !messageManager->containsLastAddress(messageMap)) {
        router->addRoutingNextHop(origin, sender);
    }
    else if (!sawMessageBefore && origin != hostIdentifier) {
        router->addRoutingNextHop(origin, sender);
    }


    if (!sawMessageBefore) {

        // Add new neighbor if message contains LastIP and LastPort fields
        if (messageManager->containsLastAddress(messageMap)) {
            QString hostName = "";
            QHostAddress hostIp = QHostAddress(messageMap.value("LastIP").toUInt());
            quint16 hostPort = messageMap.value("LastPort").toUInt();
            addNewNeighbor(hostName, hostIp, hostPort);
        }

        if(!messageManager->receivedMessageSequence(messageMap)) {
            QVariantMap *statusMessage = messageManager->getCurrentStatusMessage();
            sendStatusMessage(statusMessage, sender);
            return;
        }

        // Insert/Overwrite LastIP and LastPort keys in the message
        messageManager->insertAddressInRumorMessage(&messageMap, sender->getIpAddress(), sender->getPort());

        // Update statusMessage data structure
        messageManager->messageReceivedStatusUpdate(messageMap);

        // Send status message on receipt of the rumor message
        QVariantMap *statusMessage = messageManager->getCurrentStatusMessage();
        sendStatusMessage(statusMessage, sender);



        // Route rumor message forwarded to all neighbors
        if (isRouteRumor && origin != hostIdentifier) {
            sendRumorMessageToAllNeighbors(&messageMap);

        } else if (!noForwardFlag) { // only forward a chat rumor message if noForward flag is not set
            Peer *randomNeighbor = pickRandomNeighbor();
            if (randomNeighbor != NULL) sendRumorMessage(&messageMap, randomNeighbor);
        }

        // Send signal to ChatDialog so that the message string is displayed on the dialog
        if (messageMap.contains("ChatText")) {
            QString message = messageMap.find("ChatText").value().toString();
            QString origin = messageMap.find("Origin").value().toString();
            emit receivedMessage("<" + origin + ">: " + message);
        }

        // For testing only!!
        //		QVariantMap::iterator i;
        //		qDebug() << "Received rumor message: ";
        //		for (i = messageMap.begin(); i != messageMap.end(); i++) {
        //			qDebug() << i.key() << ": " << i.value();
        //		}
    }
}

void NetSocket::gotStatusMessage(QVariantMap messageMap, Peer *sender) {

    // Stop timer appropriately if this is one of the status messages we were waiting for
    stopNeighborsTimer(sender);

    QVariantMap *messageToSend = messageManager->getNewMessageToSend(messageMap);

    // Check if the returned map is NOT empty -> implies no new message to send
    if (!messageToSend->isEmpty()) {

        if (!noForwardFlag) {
            //qDebug() << "Sending new message..";

            // Send the new message to the sender of Status message
            sendRumorMessage(messageToSend, sender);
        }

    } else if (messageManager->hasNewMessageToFetch(messageMap)) {

        //qDebug() << "Fetching new message..";

        // We don't have a new message to send .. send our own status message to the sender
        QVariantMap *statusMessage = messageManager->getCurrentStatusMessage();
        sendStatusMessage(statusMessage, sender);

    } else {

        // Heads: Do nothing OR Tails: pick a new neighbor and start rumormongering
        int random = qrand() % 2;

        if (random == 0) {
            QVariantMap *statusMessage = messageManager->getCurrentStatusMessage();
            //Peer *randomNeighbor = pickRandomNeighbor(sender);
            Peer *randomNeighbor = pickRandomNeighbor();
            if (randomNeighbor != NULL) sendStatusMessage(statusMessage, randomNeighbor);
        }
    }
}

void NetSocket::gotPrivateMessage(QVariantMap messageMap) {

    // If the private message is intended for us then display it
    if (messageMap.value("Dest") == hostIdentifier) {

        // Send signal to ChatDialog so that the message string is displayed on the dialog
        QString message = messageMap.find("ChatText").value().toString();
        QString origin = "Private Message";
        emit receivedMessage("<" + origin + ">: " + message);

    } // forward the message if HopLimit > 0 and no forward flag is not set

    else if (messageMap.value("HopLimit").toUInt() > 0 && !noForwardFlag) {

        QString destination = messageMap.value("Dest").toString();
        QString message = messageMap.value("ChatText").toString();
        quint32 hopLimit = messageMap.value("HopLimit").toUInt() - 1;
        sendPrivateMessage(destination, message, hopLimit);
    }
}

void NetSocket::routeMessage(QVariantMap *message) {

    // Find a peer in our routing table to forward the message to
    QString origin = message->value("Origin").toString();
    QPair<QHostAddress, quint16> *targetRouter = router->lookupOrigin(origin);
    Peer *peer = searchForPeer(targetRouter->first, targetRouter->second);
    if (peer == NULL) return;       // shouldn't really happen but just for safety!

    sendMessage(message, peer);
}

void NetSocket::gotBlockRequest(QVariantMap message, Peer *sender) {

    QString destination = message.value("Dest").toString();

    if (destination == hostIdentifier) {    // Message intended for us

        // Fetch block data for the request SHA key
        QByteArray requestedBlockHash = message.value("BlockRequest").toByteArray();
        QByteArray data = fileShareManager->fetchBlockData(requestedBlockHash);

        if (!data.isEmpty()) {

            // Send the origin back the message with the data .. send it to sender who will forward it back
            QString origin = message.value("Origin").toString();
            QVariantMap *replyMessage = messageManager->createBlockReplyMessage(origin, HOP_LIMIT, requestedBlockHash, data);
            sendBlockReplyMessage(replyMessage, sender);
        }

    } else if (message.value("HopLimit").toUInt() > 0 && !noForwardFlag) {

        // Forward the new message with decremented hop limit
        QVariantMap *messageToForward = messageManager->decrementHopLimit(message);
        routeMessage(messageToForward);
    }
}

void NetSocket::gotBlockReply(QVariantMap message, Peer *sender) {


    QString destination = message.value("Dest").toString();
    if (destination == hostIdentifier) {        // Message intended for us

        if (fileShareManager->isMetaFile(message)) {
            // Message contains block list metafile

            // Request for the first block
            QByteArray firstBlockHash = fileShareManager->createOngoingDownload(message);

            if (!firstBlockHash.isEmpty()) {
                // Send Block Request message for the first block (back to the sender??)
                QString destination = message.value("Origin").toString();
                QVariantMap *newBlockRequest = messageManager->
                        createBlockRequestMessage(destination, HOP_LIMIT, firstBlockHash);
                sendBlockRequestMessage(newBlockRequest, sender);
            }

        } else {
            // Message contains file block data

            QByteArray nextBlockHash = fileShareManager->receivedFileDataBlock(message);
            if (!nextBlockHash.isEmpty()) {
                // Send Block Request message for the next block
                QString destination = message.value("Origin").toString();
                QVariantMap *newBlockRequest = messageManager->
                        createBlockRequestMessage(destination, HOP_LIMIT, nextBlockHash);
                sendBlockRequestMessage(newBlockRequest, sender);
            }
        }
    } else if (message.value("HopLimit").toUInt() > 0 && !noForwardFlag){

        // Forward the new message with decremented hop limit
        QVariantMap *messageToForward = messageManager->decrementHopLimit(message);
        routeMessage(messageToForward);
    }
}

void NetSocket::gotSearchRequest(QVariantMap message, Peer *sender) {

    // Search for files locally first and send reply if match(es) found
    QString keywords = message.value("Search").toString();
    QList<SharedFile *> *localMatches = fileShareManager->searchForSharedFiles(keywords);
    if (!localMatches->isEmpty()) {
        // Found local matches .. send Search Reply message back to sender

        QString destination = message.value("Origin").toString();
        QString searchKeywords = message.value("Search").toString();

        QStringList matchedFileNames;
        QList<QByteArray> fileHashesArray;
        for (int i = 0; i < localMatches->length(); i++) {

            SharedFile *matchedFile = localMatches->at(i);
            matchedFileNames.append(matchedFile->getFileName());
            fileHashesArray.append(matchedFile->getFileHash());
        }

        QVariantMap *searchReplyMessage = messageManager->
                createSearchReplyMessage(destination, HOP_LIMIT, searchKeywords, matchedFileNames, fileHashesArray);
        sendSearchReplyMessage(searchReplyMessage, sender);
    }

    // Decrement budget and forward if budget > 0
    if (message.value("Budget").toUInt() > 0 && !noForwardFlag) {

        QString origin = message.value("Origin").toString();
        QString keywords = message.value("Search").toString();
        quint16 budget = message.value("Budget").toUInt() - 1;
        QVariantMap *newMessage = messageManager->createSearchRequestMessage(origin, keywords, budget);
        sendSearchRequestMessage(newMessage);
    }
}

void NetSocket::gotSearchReply(QVariantMap message) {

    QString destination = message.value("Dest").toString();

    if (destination == hostIdentifier) {
        // Search reply intended for us
        //qDebug() << message.value("MatchIDs").toByteArray().toHex();

        // Store searched file info for future downloads
        fileShareManager->receivedSearchResultFiles(message);

        // Send list of filenames in Search reply to gui by emiting a signal
        QVariantList filesList = message.value("MatchNames").toList();
        QStringList fileNamesList;
        for (int i = 0; i < filesList.length(); i++) {

            // Get the last part of filename after slash from the absolute path
            QStringList slashDelimFileName = filesList.at(i).toString().split("/");
            QString fileName = slashDelimFileName.last();
            fileNamesList.append(fileName);
        }

        // stop the search request timer if threshold number of matches have been received
        if (fileShareManager->getNumberOfSearchHits() >= SEARCH_MATCH_THRESHOLD) {
            searchRequestsTimer->stop();
        }

        emit receivedSearchResults(fileNamesList);


    } else if (message.value("HopLimit").toUInt() > 0 && !noForwardFlag) {
        // Search reply not intended for us .. decrement hop limit and forward

        // Forward the new message with decremented hop limit
        QVariantMap *messageToForward = messageManager->decrementHopLimit(message);
        routeMessage(messageToForward);
    }
}

void NetSocket::gotImageChunk(QVariantMap message, Peer *sender) {

    // Read in the message
    int idx = message.value("ChunkID").toInt();
    QVariantList image1 = message.value("Image1").toList();
    QVariantList image2 = message.value("Image2").toList();

    QList<uint> image1List, image2List;
    for (int i = 0; i < image1.length(); i++) {
        image1List.append(image1.at(i).toUInt());
        image2List.append(image2.at(i).toUInt());
    }

    // Do image comparison for the received chunks
    double compResult = imageProcessor->receivedImageChunk(image1List, image2List);

    // Send the image comparison results back to the sender
    QVariantMap *resultMessage = messageManager->createNewImageCompResult(idx, compResult);
    sendMessage(resultMessage, sender);
}

void NetSocket::gotImageCompResult(QVariantMap message) {

    int idx = message.value("ChunkID").toInt();
    double result = message.value("Result").toDouble();

    imageProcessor->receivedResult(idx, result);
}


/* File sharing
======================================================================================================================================================================*/

void NetSocket::startFileDownload(QString targetId, QString fileHash) {

    qDebug() << "Starting file download";

    // Need to read in chars in QString as Hex as follows into QByteArray
    QByteArray fileHashArray = QByteArray::fromHex(fileHash.toUtf8());

    // Use a dummy file name .. this functionality was for testing
    QString fileName = "downloadedFile";
    createNewFileDownload(targetId, fileHashArray, fileName);

}

void NetSocket::startFileDownload(QString fileName) {

    qDebug() << "Downloading" << fileName;

    // Fetch destination and filehash to start file download
    QString destination = fileShareManager->getDestinationForDownload(fileName);
    QByteArray fileHash = fileShareManager->getFileHashForDownload(fileName);
    createNewFileDownload(destination, fileHash, fileName);

}

void NetSocket::createNewFileDownload(QString destination, QByteArray fileHash, QString fileName) {

    // Target id should be in our routing table
    QPair<QHostAddress, quint16> *targetRouter = router->lookupOrigin(destination);
    if (targetRouter == NULL) {
        qDebug() << "Cannot find a peer to forward download request";
    } else {
        qDebug() << "Forwarding download request message to " + destination;

        // Get the peer to whom to send the block request message to
        Peer *peer = searchForPeer(targetRouter->first, targetRouter->second);
        if(peer == NULL) return;        // shouldn't really happen but just for safety!

        // Create a new block request message containing hash of file sought

        qDebug() << "Start file download for" << fileHash.toHex();
        QVariantMap *blockRequestMessage = messageManager->createBlockRequestMessage(destination, HOP_LIMIT, fileHash);

        // Update data structure to keep track of file requests sent
        fileShareManager->newDownloadFileRequest(fileHash, fileName);

        // Forward the message to be routed to the targetId
        sendFileRequestMessage(blockRequestMessage, peer);

    }
}

/* File searching
======================================================================================================================================================================*/

void NetSocket::startNewFileSearch(QString searchKeywords) {

    fileShareManager->startNewSearch(searchKeywords, SEARCH_BUDGET);
    startFileSearch(searchKeywords, SEARCH_BUDGET);
    setupPeriodicSearchRequests();
}

void NetSocket::startFileSearch(QString searchKeywords, quint32 searchBudget) {

    qDebug() << "Started searching for" << searchKeywords;

    QVariantMap *searchRequestMessage = messageManager->createSearchRequestMessage(hostIdentifier, searchKeywords, searchBudget);
    sendSearchRequestMessage(searchRequestMessage);
}

void NetSocket::sendPeriodicSearchRequest() {

    quint32 newBudget = fileShareManager->getIncrementedSearchBudget();

    // Stop the timer if budget exceeds limit
    if (newBudget >= SEARCH_BUDGET_LIMIT) {
        searchRequestsTimer->stop();
    } else {

        QString currentSearchKeywords = fileShareManager->getCurrentSearchKeywords();
        startFileSearch(currentSearchKeywords, newBudget);
    }
}
