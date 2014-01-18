#ifndef IMAGEPROCESSOR_HH
#define IMAGEPROCESSOR_HH

#include <QObject>
#include <QImage>
#include <cstdlib>
#include <QColor>
#include <iostream>
#include <QPair>
#include <QTimer>

#include "Peer.hh"
#include "PeerSession.hh"

/* This class handles distributed image matching session.
 * The assumption is that there can only be one on-going image processing session
 * at a time.
 * Contains methods for both the master and the workers.
 * For masters, it starts processing by dividing up the image processing task among
 * its peers and waiting till result responses are received from all the workers.
 * For workers, it receives the data, processes it and returns the result.
 */

#define IMAGE_PORTION_SIZE (2048) // 2 kB
#define DEFAULT_RESULT_VALUE (-1)   // Result value always >= 0
#define PROGRESS_TIMEOUT (3000) // 3 seconds

class ImageProcessor : public QObject
{
    Q_OBJECT

public:
    ImageProcessor(QList<Peer*> *neighborsList);

    void startProcessing(QString image1, QString image2);
    QVector<uint> *flattenImage(QImage *image);
    QList<QPair<QVector<uint>*, QVector<uint>*>*> *chopUpImages(QVector<uint> *flattened1, QVector<uint> *flattened2);
    double receivedImageChunk(QList<uint> image1, QList<uint> image2);
    void receivedResult(int idx, double result);
    bool allResultsReceived();
    double getResultsTotalDiff();
    PeerSession *getSessionToReassignPeer();
    QTimer *progressTimer;

private:
    QList<Peer*> *neighborsList;
    QImage *firstImage;
    QImage *secondImage;
    QList<QPair<QVector<uint>*, QVector<uint>* >* > *imagePortions;
    QVector<double> *resultsPortions;
    int portionsPerPeer;
    QList<PeerSession *> *peerSessionsList;
    QHash<int, PeerSession*> *chunkIdxToSessionMap;
    bool firstTimeout;

signals:
    void completedMatching(QString result);
    void sendImageChunk(QPair<QVector<uint>*,QVector<uint>*>* imageChunk, int idx, Peer* peer);

public slots:
    void sendData(int idx, Peer *peer);
    void onProgressTimerTimeout();

};

#endif // IMAGEPROCESSOR_HH
