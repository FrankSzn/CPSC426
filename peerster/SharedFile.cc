#include "SharedFile.hh"

SharedFile::SharedFile(QString name, int size, QByteArray hash) {

    this->fileName = name;
    this->fileSize = size;
    this->fileHash = hash;
}

QString SharedFile::getFileName() {
    return this->fileName;
}

int SharedFile::getFileSize() {
    return this->fileSize;
}

QByteArray SharedFile::getFileHash() {
    return this->fileHash;
}
