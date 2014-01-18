#ifndef FILEBLOCK_HH
#define FILEBLOCK_HH

#include <QObject>
#include <QString>

#define TYPE_FILE_CHUNK (0)
#define TYPE_FILE_HASH (1)

class FileBlock : public QObject
{

    Q_OBJECT

public:
    FileBlock(QString name, bool type, QByteArray content);

    QString getFileName();
    //int getBlockNumber();
    //int getBlockSize();
    QByteArray getFileBlockContent();
    bool isFileChunk();
    bool isFileHash();

private:
    QString fileName;
    bool blockType;
    //int blockNumber;
    //int blockSize;
    QByteArray blockContent;
};

#endif // FILEBLOCK_HH
