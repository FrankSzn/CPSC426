#include "OngoingDownload.hh"

#include <QDebug>
#include <QFile>
#include <QDir>

OngoingDownload::OngoingDownload(QString saveFileDir, QString fileName, int fileSize, QList<QByteArray> *blocks) {

    this->saveFileDir = saveFileDir;
    this->fileName = fileName;
    this->fileSize = fileSize;
    this->pendingBlockRequests = blocks;
    this->data = new QList<QByteArray>();
}

QByteArray OngoingDownload::getNextBlockToRequest() {

    // Returns the first block from pendingBlockRequests
    return pendingBlockRequests->at(0);
}

bool OngoingDownload::blockBelongsToFile(QByteArray blockHash) {

    // Checks if the block given is at the top of the pendingBlockRequests stack
    return (blockHash == pendingBlockRequests->at(0));
}

int OngoingDownload::getNumberOfPendingBlocks() {
    return pendingBlockRequests->length();
}

void OngoingDownload::receivedBlock(QVariantMap blockReplyMessage) {

    // Confirm that this is the block we needed
    QByteArray blockHash = blockReplyMessage.value("BlockReply").toByteArray();
    if (blockHash == pendingBlockRequests->at(0)) {

        // Pop out the block from the pendingBlockRequests since it has been received
        pendingBlockRequests->removeFirst();

        // Write data to data array .. append only
        QByteArray blockData = blockReplyMessage.value("Data").toByteArray();
        data->append(blockData);
    }

}

void OngoingDownload::dumpDataToFile() {

    QString downloadFolder = saveFileDir + "/" + "Peerster_Downloads";
    QDir().mkpath(downloadFolder);
//    if(!QDir(saveFileDir).exists()) {
//        // Create the download folder if it does not exist
//        QDir().mkdir(downloadFolder);
//    }

    QString downloadFileFullPath = downloadFolder + "/" + fileName;
    qDebug() << "Downloading file complete. Dumping to file" << downloadFileFullPath;

    QFile file(downloadFileFullPath);
    file.open(QIODevice::WriteOnly);

    for (int i = 0; i < data->length(); i++) {

        int len = data->at(i).length();
        const char *buffer = data->at(i).data();
        file.write(buffer, len);
    }

    file.close();

}
