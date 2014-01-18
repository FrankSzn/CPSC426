#include "ImageProcessor.hh"
#include <math.h>

#include <QDebug>

ImageProcessor::ImageProcessor(QList<Peer*> *neighborsList) {
    this->neighborsList = neighborsList;
}

void ImageProcessor::startProcessing(QString image1, QString image2) {

    firstImage = new QImage(image1);
    secondImage = new QImage(image2);

    int h = firstImage->height();
    int w = firstImage->width();
    int hsecond = secondImage->height();
    int wsecond = secondImage->width();

    // Dimensions check for the two images to be matched
    if ( w != wsecond || h != hsecond ) {
        emit completedMatching("Error, pictures must have identical dimensions!");
        return;
    }

    // Flatten the two images to 1D vectors
    QVector<uint> *flatImage1 = flattenImage(firstImage);
    QVector<uint> *flatImage2 = flattenImage(secondImage);

    // Chop up the images into 2kB portions and store them as pairs (image1 chunk, image2 chunk) in a table
    imagePortions = chopUpImages(flatImage1, flatImage2);

    //qDebug() << "size of 1D image =" << flatImage1->size();
    //qDebug() << "#image portions =" << imagePortions->size();

    // Allocate space for storing the results for each chunk with DEFAULT_RESULT_VALUE
    resultsPortions = new QVector<double>(imagePortions->size(), DEFAULT_RESULT_VALUE);

    // Assign image chunks to each peer in neighbors list and start the peer sessions
    portionsPerPeer = ceil(imagePortions->size() / (neighborsList->size() * 1.0));
    peerSessionsList = new QList<PeerSession *>();
    chunkIdxToSessionMap = new QHash<int, PeerSession*>();
    for (int peerIdx = 0, portionIdx = 0; peerIdx < neighborsList->size(); peerIdx++) {
        QList<int> *assignedIdx = new QList<int>();
        int idx = portionIdx;
        for(; portionIdx < imagePortions->size() && portionIdx < idx + portionsPerPeer; portionIdx++) {
            assignedIdx->append(portionIdx);
        }
        PeerSession *peerSession = new PeerSession(neighborsList->at(peerIdx), assignedIdx);
        connect(peerSession, SIGNAL(sendDataChunk(int,Peer*)),
                this, SLOT(sendData(int,Peer*)));
        for (int i = idx; i < portionIdx; i++) {
            chunkIdxToSessionMap->insert(i,peerSession);
        }
        peerSession->startSession();
        peerSessionsList->append(peerSession);
    }

    firstTimeout = true;

    //    qDebug() << "#neighbors = " << neighborsList->size();
    //    qDebug() << "Portions per peer = " << portionsPerPeer;


    //    double totaldiff = 0.0 ; //holds the number of different pixels
    //    for (int i = 0; i < imagePortions->size(); i++) {
    //        QPair<QVector<uint>*, QVector<uint>*>* portion = imagePortions->at(i);
    //        for (int j = 0; j < portion->first->size(); j++) {
    //            uint pixelFirst = portion->first->at(j);
    //            int rFirst = qRed(pixelFirst);
    //            int gFirst = qGreen(pixelFirst);
    //            int bFirst = qBlue(pixelFirst);
    //            uint pixelSecond = portion->second->at(j);
    //            int rSecond = qRed(pixelSecond);
    //            int gSecond = qGreen(pixelSecond);
    //            int bSecond = qBlue(pixelSecond);
    //            totaldiff += std::abs(rFirst - rSecond) / 255.0 ;
    //            totaldiff += std::abs(gFirst - gSecond)  / 255.0 ;
    //            totaldiff += std::abs(bFirst -bSecond) / 255.0 ;
    //        }
    //    }

    //    double similarity = (100 - (totaldiff * 100)  / (w * h * 3));
    //    QString result = "Correct Similarity between the images = " + QString::number(similarity) + "%";
    //    emit completedMatching(result);


    //    totaldiff = 0.0 ; //holds the number of different pixels

    //    for (int y = 0 ; y < h ; y++) {
    //        uint *firstLine = (uint*) firstImage->scanLine(y);
    //        uint *secondLine = (uint*)secondImage->scanLine(y);
    //        for (int x = 0 ; x < w ; x++) {
    //            uint pixelFirst = firstLine[x];
    //            int rFirst = qRed(pixelFirst);
    //            int gFirst = qGreen(pixelFirst);
    //            int bFirst = qBlue(pixelFirst);
    //            uint pixelSecond = secondLine[x];
    //            int rSecond = qRed(pixelSecond);
    //            int gSecond = qGreen(pixelSecond);
    //            int bSecond = qBlue(pixelSecond);
    //            totaldiff += std::abs(rFirst - rSecond) / 255.0 ;
    //            totaldiff += std::abs(gFirst - gSecond)  / 255.0 ;
    //            totaldiff += std::abs(bFirst -bSecond) / 255.0 ;
    //        }
    //    }
}

// Flatten the given image into a 1D vector of uint
QVector<uint> *ImageProcessor::flattenImage(QImage *image) {

    QVector<uint> *flatImage = new QVector<uint>();
    int h = image->height();
    int w = image->width();

    for (int y = 0; y < h; y++) {
        uint *line = (uint*) image->scanLine(y);
        for (int x = 0; x < w; x++) {
            uint pixel = line[x];
            flatImage->append(pixel);
        }
    }

    return flatImage;
}

// Chop up the two flattened images into a list of < image_chunk_1, image_chunk_2 > pairs
QList<QPair<QVector<uint>*, QVector<uint>*>*> *ImageProcessor::
chopUpImages(QVector<uint> *flattened1, QVector<uint> *flattened2) {

    QList<QPair<QVector<uint>*, QVector<uint>*>*> *imageChunks = new QList<QPair<QVector<uint>*, QVector<uint>*>*>();

    for (int idx = 0; idx < flattened1->size(); ) {
        QVector<uint> *chunk1 = new QVector<uint>();
        QVector<uint> *chunk2 = new QVector<uint>();
        for (int j = 0; j < IMAGE_PORTION_SIZE && idx < flattened1->size(); j++, idx++) {
            uint pixel1 = flattened1->at(idx);
            uint pixel2 = flattened2->at(idx);
            chunk1->append(pixel1);
            chunk2->append(pixel2);
        }
        QPair<QVector<uint>*, QVector<uint>*> *pair = new QPair<QVector<uint>*, QVector<uint>*>(chunk1, chunk2);
        imageChunks->append(pair);
    }

    return imageChunks;
}

// Emit signal to be caught by NetSocket that will then send the message containing the image chunks
void ImageProcessor::sendData(int idx, Peer *peer) {

    // Start a timer that will go off to indicate no progress (in case of no peers available)
    progressTimer = new QTimer(this);
    progressTimer->setSingleShot(true);
    connect(progressTimer,SIGNAL(timeout()),this,SLOT(onProgressTimerTimeout()));
    progressTimer->start(PROGRESS_TIMEOUT);

    emit sendImageChunk(imagePortions->at(idx), idx, peer);
}

// Computes the difference between the two image chunks passed in
double ImageProcessor::receivedImageChunk(QList<uint> image1, QList<uint> image2) {

    double totaldiff = 0.0 ; //holds the number of different pixels
    for (int i = 0; i < image1.length(); i++) {
        uint pixelFirst = image1.at(i);
        int rFirst = qRed(pixelFirst);
        int gFirst = qGreen(pixelFirst);
        int bFirst = qBlue(pixelFirst);
        uint pixelSecond = image2.at(i);
        int rSecond = qRed(pixelSecond);
        int gSecond = qGreen(pixelSecond);
        int bSecond = qBlue(pixelSecond);
        totaldiff += std::abs(rFirst - rSecond) / 255.0 ;
        totaldiff += std::abs(gFirst - gSecond)  / 255.0 ;
        totaldiff += std::abs(bFirst -bSecond) / 255.0 ;
    }

    return totaldiff;
}

void ImageProcessor::receivedResult(int idx, double result) {

    //qDebug() << "Result for idx =" << idx << "is" << result;

    // First update our results array that stores result per each image portion
    resultsPortions->replace(idx,result);

    // Emit the signal to indicate that processing is finished if all result portions received
    if (allResultsReceived()) {
        // stop the progress timer
        progressTimer->stop();
        firstTimeout = false;

        // emit signal to indicate matching complete
        double totaldiff = getResultsTotalDiff();
        int width = firstImage->width();
        int height = firstImage->height();
        double similarity = (100 - (totaldiff * 100)  / (width * height * 3));
        QString result = "Similarity between the images = " + QString::number(similarity) + "%";
        emit completedMatching(result);
        return;
    }

    // If sender session has another chunk to send, send the next chunk
    PeerSession *senderSession = chunkIdxToSessionMap->value(idx);
    bool sentNextChunk = senderSession->receivedResultForSession();
    if (!sentNextChunk) {
        // Sender session has already sent all the chunks it was assigned
        // Assign more work to this peer by finding a slow session
        PeerSession *sessionToReassignPeer = getSessionToReassignPeer();
        if (sessionToReassignPeer != NULL) {
            sessionToReassignPeer->reassignPeer(senderSession->getPeer());
        }
    }
}

bool ImageProcessor::allResultsReceived() {

    for (int i = 0; i < resultsPortions->size(); i++) {
        if (resultsPortions->at(i) == DEFAULT_RESULT_VALUE) {
            return false;
        }
    }
    return true;
}

double ImageProcessor::getResultsTotalDiff() {

    double totaldiff= 0.0;
    for (int i = 0; i < resultsPortions->size(); i++) {
        totaldiff += resultsPortions->at(i);
    }
    return totaldiff;
}

// Will find a peer session that is less than halfway done so that
// it can be reassigned to a faster peer who completed his load
PeerSession *ImageProcessor::getSessionToReassignPeer() {
    int threshold = portionsPerPeer/2;
    for (int i = 0; i < peerSessionsList->size(); i++) {
        if (peerSessionsList->at(i)->getLoadStillToProcess() > threshold) {
            return peerSessionsList->at(i);
        }
    }
    return NULL;
}

// A slot that emits a signal to indicate that no progress made by timeout
// implying that no peers were available
void ImageProcessor::onProgressTimerTimeout() {
    if (firstTimeout) {
        emit completedMatching("No peers available at this time. Please try again later.");
        firstTimeout = false;
    }
}
