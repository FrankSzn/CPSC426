#include "FileShareManager.hh"

#include <math.h>
#include <QFile>
#include <QDebug>
#include <QtCrypto>
#include <QByteArray>
#include <QList>
#include <QDir>

FileShareManager::FileShareManager() {
    sharedFilesMap = new QMap<QString, SharedFile*>();
    sharedFilesHash = new QHash<QByteArray, FileBlock *>();
    fileRequestsSent = new QMap<QByteArray, QString>();
    ongoingDownloadList = new QList<OngoingDownload *>();
    searchResultFiles = new QMap<QString, QPair<QString, QByteArray> >();
}

void FileShareManager::shareFiles(QStringList files) {
    qDebug() << "Started file sharing for" << files.size() << "files";

    // Divide each file into blocks and compute SHA-256 of each block
    for (int i = 0; i < files.size(); i++) {
        splitFileAndHash(files.at(i));
    }
}

/* Splits a given file into blocks and computes SHA-256 hash of each block
   Stores the necessary information in internal data structures
*/
void FileShareManager::splitFileAndHash(QString fileName) {

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    QDataStream fileStream(&file);

    int fileSize = file.size();
    int numBlocks = ceil((double) fileSize/BLOCK_SIZE);

    QByteArray blockListMeta;

    for (int i = 0; i < numBlocks; i++) {
        // Read in next block
        char *buffer = new char[BLOCK_SIZE];
        uint len = BLOCK_SIZE;
        int numBytesRead = fileStream.readRawData(buffer, len);
        QByteArray data = QByteArray::fromRawData(buffer, numBytesRead);

        // Hash the block and append it to the block list meta file
        //QByteArray hash = QCA::Hash("sha256").hash(buffer).toByteArray();
        QByteArray hash = QCA::Hash("sha256").hash(data).toByteArray();
        blockListMeta.append(hash);

        // Update Shared files hash (for each block)
        FileBlock *fileBlock = new FileBlock(fileName, TYPE_FILE_CHUNK, data);
        sharedFilesHash->insert(hash, fileBlock);
    }

    // Update Shared files map
    QByteArray fileHash = QCA::Hash("sha256").hash(blockListMeta).toByteArray();
    SharedFile *sharedFile = new SharedFile(fileName, fileSize, fileHash);
    // First strip out filename from the path!
    QString strippedFileName = fileName.split("/").last();
    sharedFilesMap->insert(strippedFileName, sharedFile);

    // Update Shared files hash (for the file)
    FileBlock *metaBlock = new FileBlock(fileName, TYPE_FILE_HASH, blockListMeta);
    sharedFilesHash->insert(fileHash, metaBlock);

    qDebug() << "Shared file size =" << fileSize << ", numBlocks =" << numBlocks
                << ", metafile size =" << blockListMeta.size();
    qDebug() << "Shared file hash =" << fileHash.toHex();

    file.close();
}

QByteArray FileShareManager::fetchBlockData(QByteArray requestedBlockHash) {

    FileBlock *requestedBlock = sharedFilesHash->value(requestedBlockHash);
    QByteArray data = NULL;
    if (requestedBlock != NULL) {
        data = requestedBlock->getFileBlockContent();
    }
    return data;
}

void FileShareManager::newDownloadFileRequest(QByteArray fileHash, QString fileName) {

    fileRequestsSent->insert(fileHash, fileName);
}

bool FileShareManager::isMetaFile(QVariantMap message) {

    QByteArray messageHash = message.value("BlockReply").toByteArray();
    QString fileName = fileRequestsSent->value(messageHash);

    // If no matching hash found in fileRequestsSent, message contains file data block
    if (fileName.isEmpty()) return false;
    else return true;
}

QByteArray FileShareManager::createOngoingDownload(QVariantMap message) {

    // We received response to the file request .. remove the request from fileRequestsSent
    QByteArray messageHash = message.value("BlockReply").toByteArray();
    QString fileName = fileRequestsSent->value(messageHash);
    fileRequestsSent->remove(messageHash);

    // Validate data in message first .. should be a multiple of HASH_NUM_BYTES
    QByteArray metaFileData = message.value("Data").toByteArray();
    if ((metaFileData.length() % HASH_NUM_BYTES) != 0) return NULL;

    // Create new OngoingDownload with blocks contained in message's data
    QString saveFileDir = QDir::homePath();
    int fileSize = (metaFileData.length() / HASH_NUM_BYTES) * BLOCK_SIZE;
    QList<QByteArray> *blockHashList = getBlockHashList(metaFileData);
    OngoingDownload *newDownload = new OngoingDownload(saveFileDir, fileName, fileSize, blockHashList);

    ongoingDownloadList->append(newDownload);

    // Return hash of the first block to fetch
    return newDownload->getNextBlockToRequest();
}

QList<QByteArray> *FileShareManager::getBlockHashList(QByteArray metaFileData) {

    QList<QByteArray> *blockHashList = new QList<QByteArray>();
    QByteArray *blockHash = new QByteArray();

    for (int idx = 0; idx < metaFileData.length(); idx++) {

        if ((idx % HASH_NUM_BYTES) == 0 && idx != 0) {
            blockHashList->append(*blockHash);
            blockHash = new QByteArray();
        }

        blockHash->append(metaFileData.at(idx));
    }

    // append the last accumulated blockHash too!
    blockHashList->append(*blockHash);

    return blockHashList;

}

QByteArray FileShareManager::receivedFileDataBlock(QVariantMap dataBlockMessage) {

    // Check if it's a block we need
    QByteArray blockHash = dataBlockMessage.value("BlockReply").toByteArray();
    OngoingDownload *blocksOngoingDownload = NULL;
    int downloadListIdx;
    for (downloadListIdx = 0; downloadListIdx < ongoingDownloadList->length(); downloadListIdx++) {

        if (ongoingDownloadList->at(downloadListIdx)->blockBelongsToFile(blockHash)) {
            blocksOngoingDownload = ongoingDownloadList->at(downloadListIdx);
            break;
        }
    }

    // Error checking to make sure that we requested the block that we received
    if (blocksOngoingDownload == NULL) return NULL;

    // Update ongoing download data stucture
    blocksOngoingDownload->receivedBlock(dataBlockMessage);

    if (blocksOngoingDownload->getNumberOfPendingBlocks() == 0) {

        // We downloaded the whole file successfully!
        blocksOngoingDownload->dumpDataToFile();
        ongoingDownloadList->removeAt(downloadListIdx);
        return NULL;
    }

    // Return hash of next block to download
    return blocksOngoingDownload->getNextBlockToRequest();
}

QList<SharedFile *> *FileShareManager::searchForSharedFiles(QString keywords) {

    // Split keywords string into tokens delimited by white space
    QStringList keywordsList = keywords.split(" ", QString::SkipEmptyParts, Qt::CaseInsensitive);

    QList<SharedFile *> *matchedSharedFiles = new QList<SharedFile *>();
    QStringList sharedFileNamesList = sharedFilesMap->keys();
    for (int keywordIdx = 0; keywordIdx < keywordsList.length(); keywordIdx++) {
        // for each keyword to search

        QString keyword = keywordsList.at(keywordIdx);
        for (int fileIdx = 0; fileIdx < sharedFileNamesList.length(); fileIdx++) {
            // for each shared file

            // match filename with keyword
            QString fileName = sharedFileNamesList.at(fileIdx);
            int matchIdx = fileName.indexOf(keyword, 0, Qt::CaseInsensitive);
            if (matchIdx >= 0) {
                // Match found!
                SharedFile *matchedFile = sharedFilesMap->value(fileName);
                matchedSharedFiles->append(matchedFile);
            }
        }
    }

    return matchedSharedFiles;
}

//QList<QByteArray> *FileShareManager::getFileHashList(QVariantMap searchReplyMessage) {

//    QByteArray fileHashes = searchReplyMessage.value("MatchIDs").toByteArray();

//    // Reuse the function that splits metafile into block hashes (32 bytes each)..
//    // it is similar to what we need i.e. split list of file hashes (32 bytes each)
//    return getBlockHashList(fileHashes);

//}

void FileShareManager::receivedSearchResultFiles(QVariantMap searchReplyMessage) {

    // First make sure that the search reply corresponds to the current search!
    QString searchKeywords = searchReplyMessage.value("SearchReply").toString();
    if (currentSearchKeywords != searchKeywords) return;

    //QList<QByteArray> *fileHashList = getFileHashList(searchReplyMessage);
    QVariantList fileHashList = searchReplyMessage.value("MatchIDs").toList();
    QVariantList fileNamesList = searchReplyMessage.value("MatchNames").toList();
    QString destination = searchReplyMessage.value("Origin").toString();

    for (int i = 0; i < fileHashList.length(); i++) {

        // Get the last part of filename after slash from the absolute path
        QStringList slashDelimFileName = fileNamesList.at(i).toString().split("/");
        QString fileName = slashDelimFileName.last();

        QByteArray fileHash = fileHashList.at(i).toByteArray();

        QPair<QString, QByteArray> destinationHashPair(destination, fileHash);
        //QPair<QString, QByteArray> destinationHashPair(destination, fileHashList->at(i));

        searchResultFiles->insert(fileName, destinationHashPair);
    }

}

void FileShareManager::startNewSearch(QString newSearchKeywords, quint32 initialSearchBudget) {

    // Set current search keywords to the string provided
    this->currentSearchKeywords = newSearchKeywords;

    // initialize current search budget
    this->currentSearchBudget = initialSearchBudget;

    // We need to clear the former search results list!
    searchResultFiles->clear();
}

QString FileShareManager::getDestinationForDownload(QString fileName) {

    return searchResultFiles->value(fileName).first;
}

QByteArray FileShareManager::getFileHashForDownload(QString fileName) {

    return searchResultFiles->value(fileName).second;
}

int FileShareManager::getNumberOfSearchHits() {

    return searchResultFiles->keys().length();
}

QString FileShareManager::getCurrentSearchKeywords() {

    return this->currentSearchKeywords;
}

quint32 FileShareManager::getIncrementedSearchBudget() {

    currentSearchBudget *= 2;
    return currentSearchBudget;
}
