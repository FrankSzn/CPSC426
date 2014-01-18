#ifndef NET_SOCKET_HH
#define NET_SOCKET_HH

#include <QUdpSocket>
#include <QVariantMap>

#include "MessageManager.hh"
#include "FileShareManager.hh"
#include "Peer.hh"
#include "Router.hh"
#include "ImageProcessor.hh"

#define NEIGHBOR_TIMER_DURATION (1000)
#define START_RUMORMONGERING_INTERVAL (10000)
#define ROUTE_RUMOR_MESSAGE_INTERVAL (60000)
#define SEARCH_INTERVAL (1000)

#define HOP_LIMIT (10)
#define SEARCH_BUDGET (2)
#define SEARCH_BUDGET_LIMIT (100)
#define SEARCH_MATCH_THRESHOLD (10)


class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
    NetSocket(bool noForward);

	// Bind this socket to a Peerster-specific default port.
	bool bind();

    void setupBackgroundTimer();
    void setupPeriodicRouteRumors();
    void setupPeriodicSearchRequests();

    void sendMessage(QVariantMap *message, Peer *peer);
	void sendRumorMessage(QVariantMap *messageMap, Peer *neighbor);
    void sendRumorMessageToAllNeighbors(QVariantMap *messageMap);
	void sendNewRumorMessage(QString message);
	void sendStatusMessage(QVariantMap *messageMap, Peer *neighbor);
    void sendPrivateMessage(QString destination, QString message, quint32 hopLimit);
    void sendNewPrivateMessage(QString destination, QString message);
    void sendFileRequestMessage(QVariantMap *message, Peer *peer);
    void sendBlockRequestMessage(QVariantMap *message, Peer *peer);
    void sendBlockReplyMessage(QVariantMap *message, Peer *peer);
    void sendSearchRequestMessage(QVariantMap *message);
    void sendSearchReplyMessage(QVariantMap *message, Peer *peer);

    QList<Peer*> *getLocalNeighborsList(int myPort);
	void addNewNeighbor(QString hostString);
    void addNewNeighbor(QString hostName, QHostAddress hostIp, quint16 hostPort);
	Peer *pickRandomNeighbor();
	Peer *pickRandomNeighbor(Peer *exclude);
	bool dynamicAddNeighbor(Peer *newPeer);
	bool areSameNeighbors(Peer *peer1, Peer *peer2);
    Peer *searchForPeer(QHostAddress ipAddress, quint16 port);
    void startNeighborsTimer(Peer *neighbor);
    bool stopNeighborsTimer(Peer *neighbor);

    void gotRumorMessage(QVariantMap messageMap, Peer *sender, bool isRouteRumor);
	void gotStatusMessage(QVariantMap messageMap, Peer *sender);
    void gotPrivateMessage(QVariantMap messageMap);
    void routeMessage(QVariantMap *message);
    void gotBlockRequest(QVariantMap message, Peer *sender);
    void gotBlockReply(QVariantMap message, Peer *sender);
    void gotSearchRequest(QVariantMap message, Peer *sender);
    void gotSearchReply(QVariantMap message);
    void gotImageChunk(QVariantMap message, Peer *sender);
    void gotImageCompResult(QVariantMap message);

    void startFileDownload(QString targetId, QString fileHash);
    void startFileDownload(QString fileName);
    void createNewFileDownload(QString destination, QByteArray fileHash, QString fileName);

    void startNewFileSearch(QString searchKeywords);
    void startFileSearch(QString searchKeywords, quint32 searchBudget);


    QString hostIdentifier;
    Router *router;
    bool noForwardFlag;
    FileShareManager *fileShareManager;
    ImageProcessor *imageProcessor;         // For distributed image matching

private:
	int myPortMin, myPortMax;
	MessageManager *messageManager;
	int myCurrentPort;
	QList<Peer*> *neighborsList;
    QMap<Peer*,QTimer*> *neighborTimers;	// when waiting for a status message from the neighbor
    QTimer *searchRequestsTimer;

public slots:
	void readMessage();

    void onNeighborTimerTimeout();
	void startRumormongering();
    void sendRouteRumorMessage();
    void sendPeriodicSearchRequest();
    void sendImageChunkToPeer(QPair<QVector<uint>*, QVector<uint>* >* imageChunk, int idx, Peer *peer);

signals:
	void receivedMessage(QString message);
    void receivedSearchResults(QStringList searchResultFiles);

};

#endif // NET_SOCKET_HH
