#include "FileBlock.hh"

FileBlock::FileBlock(QString name, bool type, QByteArray content) {
    this->fileName = name;
    this->blockType = type;
    this->blockContent = content;
}

QString FileBlock::getFileName() {
    return this->fileName;
}

//int FileBlock::getBlockNumber() {
//    return this->blockNumber;
//}

//int FileBlock::getBlockSize() {
//    return this->blockSize;
//}

bool FileBlock::isFileChunk() {
    return (this->blockType == TYPE_FILE_CHUNK);
}

bool FileBlock::isFileHash() {
    return (this->blockType == TYPE_FILE_HASH);
}

QByteArray FileBlock::getFileBlockContent() {
    return this->blockContent;
}
