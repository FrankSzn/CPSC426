#include <QVBoxLayout>
#include <QDebug>
#include <QLabel>

#include "ChatDialog.hh"

ChatDialog::ChatDialog(NetSocket *socket)
{
    netSocket = socket;

    QString windowTitle = "Peerster@";
    windowTitle = windowTitle.append(socket->hostIdentifier);
    setWindowTitle(windowTitle);

    QLabel *chatLabel = new QLabel("Chat", this);
    chatLabel->setAlignment(Qt::AlignCenter);

    // Read-only text box where we display messages from everyone.
    // This widget expands both horizontally and vertically.
    textview = new QTextEdit(this);
    textview->setReadOnly(true);

    // Small text-entry box the user can enter messages.
    // This widget normally expands only horizontally,
    // leaving extra vertical space for the textview widget.
    //
    // You might change this into a read/write QTextEdit,
    // so that the user can easily enter multi-line messages.
    textline = new QTextEditSingleLine(this);
    QFontMetrics fontMetrics(textline->font());
    int rowHeight = fontMetrics.lineSpacing();
    textline->setFixedHeight(3 * rowHeight);
    // Set defualt focus on textline so that user can enter text immediately
    textline->setFocus();

    // Widget for adding a new peer connection
    // add peers by providing hostname:port OR ipaddr:port
    addPeerTextLine = new QLineEdit(this);
    addPeerTextLine->setPlaceholderText("Add a Buddy");

    // Widget that displays a list of origins to which
    // private message can be sent
    originsList = new QListWidget(this);
    QLabel *originsListLabel = new QLabel("Origins", this);
    originsListLabel->setAlignment(Qt::AlignCenter);

    // Lay out the widgets to appear in the main window.
    // For Qt widget and layout concepts see:
    // http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(chatLabel);
    leftLayout->addWidget(textview);
    leftLayout->addWidget(textline);
    leftLayout->addWidget(addPeerTextLine);

    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(originsListLabel);
    rightLayout->addWidget(originsList);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addLayout(leftLayout, 2);
    topLayout->addLayout(rightLayout, 1);

    QLabel *fileSharingLabel = new QLabel("File Sharing:", this);
    fileSharingLabel->setAlignment(Qt::AlignLeft);

    // For entering target node ID to get download from
    downloadTargetLine = new QLineEdit(this);
    downloadTargetLine->setPlaceholderText("Download target node id");

    // For entering hash of the file needed to download
    downloadFileIdLine = new QLineEdit(this);
    downloadFileIdLine->setPlaceholderText("Download file hash");

    // setup button to download a file
    downloadFile = new QPushButton("Download File", this);

    // setup button to share local files
    shareLocalFile = new QPushButton("Share Local File", this);

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(downloadFile, 1);
    buttonsLayout->addWidget(shareLocalFile, 1);

    QVBoxLayout *chatShareLayout = new QVBoxLayout();
    chatShareLayout->addLayout(topLayout);
    chatShareLayout->addWidget(fileSharingLabel);
    chatShareLayout->addWidget(downloadTargetLine);
    chatShareLayout->addWidget(downloadFileIdLine);
    chatShareLayout->addLayout(buttonsLayout);

    QLabel *fileSearchLabel = new QLabel("File Search:", this);
    fileSearchLabel->setAlignment(Qt::AlignCenter);

    // For displaying search results that can be double-clicked to start a download
    searchedFilesList = new QListWidget(this);

    // For entering search keywords for file to be downloaded
    searchKeywordsLine = new QLineEdit(this);
    searchKeywordsLine->setPlaceholderText("Search for file keywords");

    // setup button for searching files with entered keywords
    searchFile = new QPushButton("Search for File", this);

    QVBoxLayout *fileSearchLayout = new QVBoxLayout();
    fileSearchLayout->addWidget(fileSearchLabel);
    fileSearchLayout->addWidget(searchedFilesList);
    fileSearchLayout->addWidget(searchKeywordsLine);
    fileSearchLayout->addWidget(searchFile, Qt::AlignCenter);

    QHBoxLayout *labsLayout = new QHBoxLayout();
    labsLayout->addLayout(chatShareLayout, 3);
    labsLayout->addLayout(fileSearchLayout, 1);

    // Setup for image matching kit UI
    QLabel *imageMatchingLabel = new QLabel("Distributed Image Matching:", this);
    imageMatchingLabel->setAlignment(Qt::AlignLeft);
    imageMatchOutput = new QTextEdit(this);
    imageMatchOutput->setReadOnly(true);
    image1 = new QPushButton("Select Image 1", this);
    image2 = new QPushButton("Select Image 2", this);
    matchImages = new QPushButton("Match Images", this);
    selectedImage1 = "";
    selectedImage2 = "";

    QVBoxLayout *imageMatchButtons = new QVBoxLayout();
    imageMatchButtons->addWidget(image1);
    imageMatchButtons->addWidget(image2);
    imageMatchButtons->addWidget(matchImages);

    QHBoxLayout *imageMatchLayout = new QHBoxLayout();
    imageMatchLayout->addWidget(imageMatchOutput, 2);
    imageMatchLayout->addLayout(imageMatchButtons, 1);

    QVBoxLayout *projectLayout = new QVBoxLayout();
    projectLayout->addWidget(imageMatchingLabel);
    projectLayout->addLayout(imageMatchLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(labsLayout);
    mainLayout->addLayout(projectLayout);

    setLayout(mainLayout);

    // Register a callback on the textline's returnPressed signal
    // so that we can send the message entered by the user.
    connect(textline, SIGNAL(textEditReturnPressed()),
            this, SLOT(gotReturnPressed()));

    // Catches signal from netsocket to display received rumor message
    connect(netSocket, SIGNAL(receivedMessage(QString)),
            this, SLOT(displayReceivedMessage(QString)));

    // Register a callback on addPeerTextLine to add new Peers
    connect(addPeerTextLine, SIGNAL(returnPressed()),
            this, SLOT(addNewPeer()));

    // Register callback on originsListChanged to update origins available
    connect(netSocket->router, SIGNAL(originsListChanged(QList<QString>)),
            this, SLOT(updateOriginsList(QList<QString>)));

    // Register onClick listener for QPushButton to start sharing local files
    connect(shareLocalFile, SIGNAL(clicked()),
            this, SLOT(startSharingFiles()));

    // Register onClick listener for QPushButton to start downloading a file
    connect(downloadFile, SIGNAL(clicked()),
            this, SLOT(startDownloadingFile()));

    // Register onClick listener for QPushButton to start searching for a file
    connect(searchFile, SIGNAL(clicked()),
            this, SLOT(startSearchingForFile()));

    // Register onDoubleClick listener on Origin list items for starting private message session
    connect(originsList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this,SLOT(startPrivateMessageSession(QListWidgetItem*)));

    // Catches signal from netsocket to display received search result files
    connect(netSocket, SIGNAL(receivedSearchResults(QStringList)),
            this, SLOT(displaySearchResults(QStringList)));

    // Register onDoubleClick listener on searched files list items for starting file download
    connect(searchedFilesList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this,SLOT(startSearchedFileDownload(QListWidgetItem*)));

    // Register onClick listener for selecting image 1 to match
    connect(image1, SIGNAL(clicked()),
            this, SLOT(selectImage1()));

    // Register onClick listener for selecting image 2 to match
    connect(image2, SIGNAL(clicked()),
            this, SLOT(selectImage2()));

    // Register onClick listener for starting the distributed image matching process
    connect(matchImages, SIGNAL(clicked()),
            this, SLOT(startImageMatching()));

    // Catches signal from image processor to display image processing result
    connect(netSocket->imageProcessor, SIGNAL(completedMatching(QString)),
            this, SLOT(displayMatchingResult(QString)));
}

void ChatDialog::gotReturnPressed()
{
    // Initially, just echo the string locally.
    // Insert some networking code here...
    qDebug() << "FIX: send message to other peers: " << textline->toPlainText();
    textview->append("<Me>: " + textline->toPlainText());

    netSocket->sendNewRumorMessage(textline->toPlainText());

    // Clear the textline to get ready for the next input message.
    textline->clear();
}

void ChatDialog::addNewPeer() {

    QString peerHostName = addPeerTextLine->text();
    netSocket->addNewNeighbor(peerHostName);

    addPeerTextLine->clear();
}

void ChatDialog::displayReceivedMessage(QString message) {
    textview->append(message);
}

/* Override keyPressEvent function in QTextEdit to emit signal when return is pressed */
void QTextEditSingleLine::keyPressEvent(QKeyEvent *e) {

    if((e->key() == Qt::Key_Enter) || (e->key() == Qt::Key_Return)) {
        e->ignore();
        emit textEditReturnPressed();
    } else {
        QTextEdit::keyPressEvent(e);
    }
}

/* Override keyPressEvent function in QTextEdit to emit signal when return is pressed */
void QPrivateTextEdit::keyPressEvent(QKeyEvent *e) {

    if((e->key() == Qt::Key_Enter) || (e->key() == Qt::Key_Return)) {
        e->ignore();
        emit privateTextEditReturnPressed(this->destination, this->toPlainText());
        this->close();
    } else {
        QTextEdit::keyPressEvent(e);
    }
}

void ChatDialog::updateOriginsList(QList<QString> origins) {

    //qDebug() << "Updating origins list";

    originsList->clear();
    QStringList *originsStringList = new QStringList(origins);
    originsList->addItems(*originsStringList);

}

void ChatDialog::startPrivateMessageSession(QListWidgetItem* selectedItem) {

    QString originSelected = selectedItem->text();
    qDebug() << "Origin selected: " + originSelected;
    QPrivateTextEdit *privateMessageDialog = new QPrivateTextEdit(originSelected);
    QFontMetrics fontMetrics(privateMessageDialog->font());
    int rowHeight = fontMetrics.lineSpacing();
    privateMessageDialog->setFixedHeight(8 * rowHeight);
    privateMessageDialog->setFixedWidth(16 * rowHeight);
    privateMessageDialog->move(this->geometry().center().x(),this->geometry().center().y());
    privateMessageDialog->show();

    // register callback onReturnPressed to send the private message
    connect(privateMessageDialog, SIGNAL(privateTextEditReturnPressed(QString,QString)),
            this, SLOT(sendPrivateMessage(QString,QString)));
}

void ChatDialog::sendPrivateMessage(QString destination, QString message) {

    qDebug() << "Destination: " + destination + " , Message: " + message;
    netSocket->sendNewPrivateMessage(destination, message);
}

void ChatDialog::startSharingFiles() {

    QFileDialog fileDialog(this);
    fileDialog.setDirectory(QDir::homePath());
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList filesToShare;
    if (fileDialog.exec()) {
        filesToShare = fileDialog.selectedFiles();
    }

    // start the file sharing process if at least one file is selected
    if (filesToShare.size() > 0) {
        netSocket->fileShareManager->shareFiles(filesToShare);
    }
}

void ChatDialog::startDownloadingFile() {

    QString downloadTarget = downloadTargetLine->text();
    QString downloadFileHash = downloadFileIdLine->text();

    if (!downloadTarget.isEmpty() && !downloadFileHash.isEmpty()) {

        netSocket->startFileDownload(downloadTarget, downloadFileHash);

        downloadTargetLine->clear();
        downloadFileIdLine->clear();
    }
}

void ChatDialog::startSearchingForFile() {

    QString searchKeywords = searchKeywordsLine->text();
    netSocket->startNewFileSearch(searchKeywords);
    searchKeywordsLine->clear();
}

void ChatDialog::displaySearchResults(QStringList searchResultFiles) {

    searchedFilesList->clear();
    searchedFilesList->addItems(searchResultFiles);
}

void ChatDialog::startSearchedFileDownload(QListWidgetItem *selectedItem) {

    QString selectedFile = selectedItem->text();
    netSocket->startFileDownload(selectedFile);
}

void ChatDialog::selectImage1() {
    QString selected = selectImage();
    if (selected != NULL) {
        imageMatchOutput->append("Image 1: " + selected);
        selectedImage1 = selected;
    }
}

void ChatDialog::selectImage2() {
    QString selected = selectImage();
    if (selected != NULL) {
        imageMatchOutput->append("Image 2: " + selected);
        selectedImage2 = selected;
    }
}

QString ChatDialog::selectImage() {

    QFileDialog fileDialog(this);
    fileDialog.setDirectory(QDir::homePath());
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    fileDialog.setNameFilter(tr("Images (*.png *.xpm *.jpg)"));
    QStringList selected;
    if (fileDialog.exec()) {
        selected = fileDialog.selectedFiles();
    }

    // successfully selected image
    if (selected.size() > 0) {
        return selected.at(0);
    }

    return NULL;
}

void ChatDialog::startImageMatching() {

    if (selectedImage1 != "" && selectedImage2 != "") {

        imageMatchOutput->append("Starting image matching...");
        netSocket->imageProcessor->startProcessing(selectedImage1, selectedImage2);

        // Need to reset these variables to EMPTY
        selectedImage1 = "";
        selectedImage2 = "";

    } else if (selectedImage1 == "" && selectedImage2 == ""){
        imageMatchOutput->append("Please select image 1 and 2");
    } else if (selectedImage1 == ""){
        imageMatchOutput->append("Please select image 1");
    } else if (selectedImage2 == "") {
        imageMatchOutput->append("Please select image 2");
    }
}

void ChatDialog::displayMatchingResult(QString result) {
    imageMatchOutput->append(result);
}
