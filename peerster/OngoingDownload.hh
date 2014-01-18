#ifndef ONGOINGDOWNLOAD_HH
#define ONGOINGDOWNLOAD_HH

#include <QString>
#include <QList>
#include <QByteArray>
#include <QVariantMap>

class OngoingDownload {

public:
    OngoingDownload(QString saveFileDir, QString fileName, int fileSize, QList<QByteArray> *blocks);

    QByteArray getNextBlockToRequest();
    bool blockBelongsToFile(QByteArray blockHash);
    void receivedBlock(QVariantMap blockReplyMessage);
    int getNumberOfPendingBlocks();
    void dumpDataToFile();

private:
    QString saveFileDir;
    QString fileName;
    int fileSize;
    QList<QByteArray> *pendingBlockRequests;
    QList<QByteArray> *data;

};

#endif // ONGOINGDOWNLOAD_HH
