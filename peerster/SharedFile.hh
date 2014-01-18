#ifndef SHAREDFILE_HH
#define SHAREDFILE_HH

#include <QObject>
#include <QString>

class SharedFile : public QObject {

    Q_OBJECT

public:
    SharedFile();
    SharedFile(QString name, int size, QByteArray hash);
    QString getFileName();
    int getFileSize();
    QByteArray getFileHash();

private:
    QString fileName;
    int fileSize;
    QByteArray fileHash;
};

#endif // SHAREDFILE_HH
