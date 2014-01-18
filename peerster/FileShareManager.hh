#ifndef FILESHAREMANAGER_HH
#define FILESHAREMANAGER_HH

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include "SharedFile.hh"
#include "FileBlock.hh"
#include "OngoingDownload.hh"

#define BLOCK_SIZE (8192) // 8 kB
#define HASH_NUM_BYTES (32) // 32 bytes in SHA256 hash

class FileShareManager : public QObject
{
    Q_OBJECT

public:
    FileShareManager();

    void shareFiles(QStringList files);
    void splitFileAndHash(QString fileName);
    QByteArray fetchBlockData(QByteArray requestedBlockHash);
    void newDownloadFileRequest(QByteArray fileHash, QString fileName);
    bool isMetaFile(QVariantMap message);
    QByteArray createOngoingDownload(QVariantMap message);
    QByteArray receivedFileDataBlock(QVariantMap dataBlockMessage);
    QList<SharedFile *> *searchForSharedFiles(QString keywords);
    QList<QByteArray> *getFileHashList(QVariantMap searchReplyMessage);
    void receivedSearchResultFiles(QVariantMap searchReplyMessage);
    void startNewSearch(QString newSearchKeywords, quint32 initialSearchBudget);
    QString getDestinationForDownload(QString fileName);
    QByteArray getFileHashForDownload(QString fileName);
    int getNumberOfSearchHits();
    quint32 getIncrementedSearchBudget();
    QString getCurrentSearchKeywords();


private:
    QMap<QString, SharedFile *> *sharedFilesMap;      // < filename, sharedfile >
    QHash<QByteArray, FileBlock *> *sharedFilesHash; // FileBlock can be either actual file chunk or hash of a block
    QMap<QByteArray, QString> *fileRequestsSent;    // < filemeta hash, filename >
    QList<OngoingDownload *> *ongoingDownloadList;
    QMap<QString, QPair<QString, QByteArray> > *searchResultFiles;   // < filename, < destination, filemeta hash > >

    QString currentSearchKeywords;
    quint32 currentSearchBudget;

    QList<QByteArray> *getBlockHashList(QByteArray metaFileData);
};

#endif // FILESHAREMANAGER_HH
