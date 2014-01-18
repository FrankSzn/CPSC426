#include <QVariantMap>
#include <QTimer>
#include <QHostAddress>

class Message
{

private:
	QString origin;
	quint32 seqNo;
    QString chatText;

public:
	Message() {}
	Message(QString text, QString originHost, quint32 sequenceNo) {
		chatText = text;
		origin = originHost;
		seqNo = sequenceNo;
	}

	QString getChatText() {
		return chatText;
	}

	QString getOrigin() {
		return origin;
	}

	quint32 getSeqNo() {
		return seqNo;
	}

};

class MessageManager
{

public:
	MessageManager(QString currentHostName);

	QVariantMap *createNewRumorMessage(QString messageText);
    QVariantMap *createNewPrivateMessage(QString destination, QString messageText, quint32 hopLimit);
    QVariantMap *createBlockRequestMessage(QString destination, quint32 hopLimit, QByteArray blockHash);
    QVariantMap *createBlockReplyMessage(QString destination, quint32 hopLimit, QByteArray blockHash, QByteArray data);
    QVariantMap *decrementHopLimit(QVariantMap message);
    QVariantMap *createSearchRequestMessage(QString origin, QString searchKeywords, quint32 budget);
    QVariantMap *createSearchReplyMessage(QString destination, quint32 hopLimit, QString searchKeywords,
                                          QStringList matchedFiles, QList<QByteArray> matchedFileHashes);
    QVariantMap *createNewImageChunk(QPair<QVector<uint>*, QVector<uint>* >* imageChunk, int idx);
    QVariantMap *createNewImageCompResult(int idx, double result);

    bool messageReceivedStatusUpdate(QVariantMap receivedMessage);
	bool receivedMessageSequence(QVariantMap receivedMessage);
	void addMessageToDatabase(QVariantMap messageMap);
	bool messageExistsInDatabase(QVariantMap message);
	QVariantMap *getCurrentStatusMessage();
	QVariantMap *getNewMessageToSend(QVariantMap sendersStatusMessage);
	bool hasNewMessageToFetch(QVariantMap sendersStatusMessage);

    bool isValidRumorMessage(QVariantMap message);
	bool isValidStatusMessage(QVariantMap message);
    bool isRouteRumorMessage(QVariantMap message);
    bool isValidPrivateMessage(QVariantMap message);
    void insertAddressInRumorMessage(QVariantMap *rumorMessage, QHostAddress hostIp, quint16 hostPort);
    bool containsLastAddress(QVariantMap rumorMessage);
    bool isValidBlockRequest(QVariantMap message);
    bool isValidBlockReply(QVariantMap message);
    bool isValidSearchRequest(QVariantMap message);
    bool isValidSearchReply(QVariantMap message);
    bool isValidImageChunk(QVariantMap message);
    bool isValidImageCompResult(QVariantMap message);

private:
	QString hostName;						// current host's name
	quint32 seqNo;						// running count of current host's message sequence number
	QVariantMap *statusMessage;			// summary of all set of messages seen so far
	QMap<QString,QList<Message> > *messagesDatabase;		// contains all the messages seen so far

};
