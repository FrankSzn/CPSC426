#include <QDebug>
#include <QList>
#include <QVariantList>
#include <QtCrypto>
#include "MessageManager.hh"

MessageManager::MessageManager(QString currentHostName) {

    hostName = currentHostName;
    seqNo = 0;

    statusMessage = new QVariantMap();
    QVariantMap *statusValuesMap = new QVariantMap();
    //statusValuesMap->insert(hostName,seqNo);
    statusMessage->insert(QObject::tr("Want"), *statusValuesMap);

    messagesDatabase = new QMap<QString,QList<Message> >();
}

/* Creates a new Rumor message with the specified <key, value> pairs */
QVariantMap *MessageManager::createNewRumorMessage(QString messageText) {

    QVariantMap *messageMap = new QVariantMap();
    if (messageText != NULL) {
        messageMap->insert("ChatText",messageText);
    } // else no ChatText -> for route rumor messages
    messageMap->insert("Origin",hostName);
    messageMap->insert("SeqNo",++seqNo);

    // Add the newly created message to the database
    addMessageToDatabase(*messageMap);

    // Update status value indicating seqNo of the latest message you sent
    QVariantMap statusValuesMap = statusMessage->find("Want").value().toMap();
    statusValuesMap.insert(hostName, seqNo + 1);
    statusMessage->insert("Want",statusValuesMap);

    // Test code --
    //	QVariantMap temp = statusMessage->find("Want").value().toMap();
    //	QVariantMap::iterator i;
    //	for (i = temp.begin(); i != temp.end(); i++) {
    //		qDebug() << "Status message contains:\n" << i.key() << ": " << i.value();
    //	}

    return messageMap;
}


/* Creates a new private message with the specified <key, value> pairs */
QVariantMap *MessageManager::createNewPrivateMessage(QString destination, QString messageText, quint32 hopLimit) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("Dest", destination);
    messageMap->insert("ChatText", messageText);
    messageMap->insert("HopLimit", hopLimit);

    return messageMap;
}

/* Creates a new block request message */
QVariantMap *MessageManager::createBlockRequestMessage(QString destination, quint32 hopLimit, QByteArray blockHash) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("Dest", destination);
    messageMap->insert("Origin", hostName);
    messageMap->insert("HopLimit", hopLimit);
    messageMap->insert("BlockRequest", blockHash);

    return messageMap;
}

/* Creates a new block reply message */
QVariantMap *MessageManager::createBlockReplyMessage(QString destination, quint32 hopLimit, QByteArray blockHash, QByteArray data) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("Dest", destination);
    messageMap->insert("Origin", hostName);
    messageMap->insert("HopLimit", hopLimit);
    messageMap->insert("BlockReply", blockHash);
    messageMap->insert("Data", data);

    return messageMap;
}

QVariantMap *MessageManager::decrementHopLimit(QVariantMap message) {

    QVariantMap *newMessage = new QVariantMap(message);
    quint16 hopLimit = newMessage->value("HopLimit").toUInt() - 1;
    newMessage->insert("HopLimit", hopLimit);
    return newMessage;
}

QVariantMap *MessageManager::createSearchRequestMessage(QString origin, QString searchKeywords, quint32 budget) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("Origin", origin);
    messageMap->insert("Search", searchKeywords);
    messageMap->insert("Budget", budget);

    return messageMap;

}

QVariantMap *MessageManager::createSearchReplyMessage(QString destination, quint32 hopLimit,
                                                      QString searchKeywords, QStringList matchedFiles,
                                                      QList<QByteArray> matchedFileHashes) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("Origin", hostName);
    messageMap->insert("Dest", destination);
    messageMap->insert("HopLimit", hopLimit);
    messageMap->insert("SearchReply", searchKeywords);

    QVariantList matchedFilesList;
    for (int i = 0; i < matchedFiles.length(); i++) {
        matchedFilesList << matchedFiles.at(i);
    }
    messageMap->insert("MatchNames", matchedFilesList);

    QVariantList matchedHashesList;
    for (int i = 0; i < matchedFileHashes.length(); i++) {
        matchedHashesList << matchedFileHashes.at(i);
    }
    messageMap->insert("MatchIDs", matchedHashesList);

    return messageMap;
}

QVariantMap *MessageManager::createNewImageChunk(QPair<QVector<uint>*, QVector<uint>* >* imageChunk, int idx) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("ChunkID", idx);

    QList<uint> image1ChunkList = imageChunk->first->toList();
    QVariantList image1Chunk;
    for (int i = 0; i < image1ChunkList.length(); i++) {
        image1Chunk << image1ChunkList.at(i);
    }
    messageMap->insert("Image1", image1Chunk);

    QList<uint> image2ChunkList = imageChunk->second->toList();
    QVariantList image2Chunk;
    for (int i = 0; i < image2ChunkList.length(); i++) {
        image2Chunk << image2ChunkList.at(i);
    }
    messageMap->insert("Image2", image2Chunk);

    return messageMap;
}

QVariantMap *MessageManager::createNewImageCompResult(int idx, double result) {

    QVariantMap *messageMap = new QVariantMap();
    messageMap->insert("ChunkID", idx);
    messageMap->insert("Result", result);

    return messageMap;
}

/* returns true if successfully updated database else returns false (already saw message before) */
bool MessageManager::messageReceivedStatusUpdate(QVariantMap receivedMessage) {

    if (messageExistsInDatabase(receivedMessage)) {
        // Already saw the received message before
        return false;
    }

    // Received message was not seen before so add it to the database
    addMessageToDatabase(receivedMessage);

    // Update the status value map
    QVariantMap statusValuesMap = statusMessage->find("Want").value().toMap();
    QString originHost = receivedMessage.find("Origin").value().toString();
    quint32 originSeqNo = receivedMessage.find("SeqNo").value().toUInt();

    statusValuesMap.insert(originHost, originSeqNo + 1);
    statusMessage->insert("Want",statusValuesMap);

    // Test code --
    //	QVariantMap temp = statusMessage->find("Want").value().toMap();
    //	QVariantMap::iterator i;
    //	for (i = temp.begin(); i != temp.end(); i++) {
    //		qDebug() << "Received message:\n" << i.key() << ": " << i.value();
    //	}

    return true;
}

bool MessageManager::receivedMessageSequence(QVariantMap receivedMessage) {

    QString originHost = receivedMessage.find("Origin").value().toString();
    quint32 originSeqNo = receivedMessage.find("SeqNo").value().toUInt();

    if (messagesDatabase->contains(originHost)) {
        quint32 messageListLength = messagesDatabase->find(originHost).value().size();

        if (originSeqNo == messageListLength + 1) {
            return true;
        }

    } else if(originSeqNo == 1) {
        // Corner case of the first message being received from the sender
        return true;
    }

    return false;
}

void MessageManager::addMessageToDatabase(QVariantMap messageMap) {

    QString messageText = NULL; // Null for route rumor message
    if (messageMap.contains("ChatText")) {
        messageText = messageMap.find("ChatText").value().toString();
    }
    QString hostName = messageMap.find("Origin").value().toString();
    quint32 seqNo = messageMap.find("SeqNo").value().toUInt();
    Message *message = new Message(messageText, hostName, seqNo);

    if (!messagesDatabase->contains(hostName)) {
        // first message being created by the current host
        QList<Message> *hostMessageList = new QList<Message>();
        hostMessageList->append(*message);
        messagesDatabase->insert(hostName, *hostMessageList);

    } else {
        QList<Message> hostMessageList = messagesDatabase->find(hostName).value();
        hostMessageList.append(*message);
        messagesDatabase->insert(hostName, hostMessageList);
    }

}

bool MessageManager::messageExistsInDatabase(QVariantMap message) {

    QString originHost = message.find("Origin").value().toString();
    quint32 originSeqNo = message.find("SeqNo").value().toUInt();

    if (messagesDatabase->contains(originHost)) {
        quint32 messageListLength = messagesDatabase->find(originHost).value().size();
        if (messageListLength >= originSeqNo) {
            return true;
        }
    }
    return false;
}

QVariantMap *MessageManager::getNewMessageToSend(QVariantMap sendersStatusMessage) {

    QVariantMap *messageToFetch = new QVariantMap();

    QVariantMap statusValuesMap = statusMessage->find("Want").value().toMap();
    QVariantMap senderStatusValuesMap = sendersStatusMessage.find("Want").value().toMap();

    // Compare own status message with sender's status message
    // to search for a message we can send to the sender
    QVariantMap::iterator mapIterator;
    for (mapIterator = statusValuesMap.begin();
         mapIterator != statusValuesMap.end(); mapIterator++) {

        QString origin = mapIterator.key();
        quint32 originSeqNo = mapIterator.value().toUInt();

        if (senderStatusValuesMap.contains(origin)) {

            quint32 sendersOriginSeqNo = senderStatusValuesMap.find(origin).value().toUInt();
            if (originSeqNo > sendersOriginSeqNo) {

                int seqNoSought = sendersOriginSeqNo;
                QList<Message> messageList = messagesDatabase->find(origin).value();

                Message messageToSend = messageList.value(seqNoSought - 1);
                if (messageToSend.getChatText() != NULL) {
                    messageToFetch->insert("ChatText", messageToSend.getChatText());
                }
                messageToFetch->insert("Origin", messageToSend.getOrigin());
                messageToFetch->insert("SeqNo", messageToSend.getSeqNo());

                break;
            }
        } else {

            QList<Message> messageList = messagesDatabase->find(origin).value();

            Message messageToSend = messageList.value(0);
            if (messageToSend.getChatText() != NULL) {
                messageToFetch->insert("ChatText", messageToSend.getChatText());
            }
            messageToFetch->insert("Origin", messageToSend.getOrigin());
            messageToFetch->insert("SeqNo", messageToSend.getSeqNo());
        }

    }

    // empty QVariantMap returned if there's no new message we have to send to the sender
    return messageToFetch;

}

bool MessageManager::hasNewMessageToFetch(QVariantMap sendersStatusMessage) {

    QVariantMap statusValuesMap = statusMessage->find("Want").value().toMap();
    QVariantMap senderStatusValuesMap = sendersStatusMessage.find("Want").value().toMap();

    // Compare sender's status message with own status message
    // to search for a message we can fetch from the sender
    QVariantMap::iterator mapIterator;
    for (mapIterator = senderStatusValuesMap.begin();
         mapIterator != senderStatusValuesMap.end(); mapIterator++) {

        QString origin = mapIterator.key();

        if (!statusValuesMap.contains(origin)) return true;

        quint32 sendersOriginSeqNo = mapIterator.value().toUInt();
        quint32 originSeqNo = statusValuesMap.find(origin).value().toUInt();

        if (sendersOriginSeqNo > originSeqNo) return true;

    }

    return false;
}

QVariantMap *MessageManager::getCurrentStatusMessage() {
    return statusMessage;
}

bool MessageManager::isValidRumorMessage(QVariantMap message) {

    if (message.contains("ChatText") && message.contains("Origin") && message.contains("SeqNo")) {

        QString chatText = message.find("ChatText").value().toString();
        QString origin = message.find("Origin").value().toString();
        quint32 seqNo = message.find("SeqNo").value().toUInt();

        return (!chatText.isEmpty() && !origin.isEmpty() && seqNo > 0);
    }

    return false;
}

bool MessageManager::isValidStatusMessage(QVariantMap message) {

    // TODO: How do we handle an empty map (value of "Want") here??
    return (message.contains("Want"));
}

bool MessageManager::isRouteRumorMessage(QVariantMap message) {

    // Route rumor message must not contain any ChatText
    if (message.contains("Origin") && message.contains("SeqNo") && !message.contains("ChatText")) {

        QString origin = message.find("Origin").value().toString();

        // Route rumor message is allowed to have seqNo = 0 (unsigned so don't check)
        return (!origin.isEmpty());
    }

    return false;
}

bool MessageManager::isValidPrivateMessage(QVariantMap message) {

    if (message.contains("Dest") && message.contains("ChatText") && message.contains("HopLimit")) {

        QString destination = message.find("Dest").value().toString();
        QString chatText = message.find("ChatText").value().toString();

        return (!destination.isEmpty() && !chatText.isEmpty());
    }

    return false;
}

void MessageManager::insertAddressInRumorMessage(QVariantMap *rumorMessage, QHostAddress hostIp, quint16 hostPort) {

    rumorMessage->insert("LastIP", hostIp.toIPv4Address());
    rumorMessage->insert("LastPort", hostPort);
}

bool MessageManager::containsLastAddress(QVariantMap rumorMessage) {

    if (rumorMessage.contains("LastIP") && rumorMessage.contains("LastPort")) {

        quint32 hostIp = rumorMessage.value("LastIP").toUInt();
        quint16 hostPort = rumorMessage.value("LastPort").toUInt();

        return (hostIp != 0 && hostPort != 0);
    }

    return false;
}

bool MessageManager::isValidBlockRequest(QVariantMap message) {

    if (message.contains("Dest") && message.contains("Origin") &&
            message.contains("HopLimit") && message.contains("BlockRequest")) {

        QString destination = message.value("Dest").toString();
        QString origin = message.value("Origin").toString();
        QByteArray blockRequest = message.value("BlockRequest").toByteArray();

        return (!destination.isEmpty() && !origin.isEmpty() && !blockRequest.isEmpty());
    }

    return false;

}

bool MessageManager::isValidBlockReply(QVariantMap message) {

    if (message.contains("Dest") && message.contains("Origin") &&
            message.contains("HopLimit") && message.contains("BlockReply") &&
            message.contains("Data")) {

        QString destination = message.value("Dest").toString();
        QString origin = message.value("Origin").toString();
        QByteArray blockReply = message.value("BlockReply").toByteArray();
        QByteArray data = message.value("Data").toByteArray();

        // Hash of data should explicitely match SHA hash held in blockReply field
        QByteArray dataHash = QCA::Hash("sha256").hash(data).toByteArray();

        return (!destination.isEmpty() && !origin.isEmpty() && !blockReply.isEmpty()
                && !data.isEmpty() && dataHash == blockReply);
    }

    return false;

}

bool MessageManager::isValidSearchRequest(QVariantMap message) {

    if (message.contains("Origin") && message.contains("Search") &&
            message.contains("Budget")) {

        QString origin = message.value("Origin").toString();
        QString search = message.value("Search").toString();

        return (!origin.isEmpty() && !search.isEmpty());
    }

    return false;
}

bool MessageManager::isValidSearchReply(QVariantMap message) {

    if (message.contains("Origin") && message.contains("Dest") && message.contains("HopLimit") &&
            message.contains("SearchReply") && message.contains("MatchNames") && message.contains("MatchIDs")) {

        QString origin = message.value("Origin").toString();
        QString destination = message.value("Dest").toString();
        QString search = message.value("SearchReply").toString();
        QVariantList matchNames = message.value("MatchNames").toList();
        QVariantList matchIds = message.value("MatchIDs").toList();

        return (!origin.isEmpty() && !destination.isEmpty() && !search.isEmpty() && !matchNames.isEmpty() &&
                !matchIds.isEmpty());
    }

    return false;
}

bool MessageManager::isValidImageChunk(QVariantMap message) {

    if (message.contains("ChunkID") && message.contains("Image1") && message.contains("Image2")) {

        int idx = message.value("ChunkID").toInt();
        QVariantList chunk1 = message.value("Image1").toList();
        QVariantList chunk2 = message.value("Image2").toList();

        return (!chunk1.isEmpty() && !chunk2.isEmpty() && chunk1.length() == chunk2.length() && idx >= 0);
    }

    return false;
}

bool MessageManager::isValidImageCompResult(QVariantMap message) {

    if (message.contains("ChunkID") && message.contains("Result")) {

        int idx = message.value("ChunkID").toInt();
        double result = message.value("Result").toDouble();

        return (idx >= 0 && result >= 0);
    }

    return false;
}
