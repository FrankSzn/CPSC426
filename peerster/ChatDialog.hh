#ifndef CHAT_DIALOG_HH
#define CHAT_DIALOG_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include "NetSocket.hh"


/* Custom QTextEdit that emits a signal when return is pressed */
class QTextEditSingleLine : public QTextEdit {

	Q_OBJECT

public:
    QTextEditSingleLine(QWidget *parent) : QTextEdit(parent) {}

protected:
	virtual void keyPressEvent(QKeyEvent *e);

signals:
	void textEditReturnPressed();

};

class QPrivateTextEdit : public QTextEdit {
    Q_OBJECT

public:
    QPrivateTextEdit(QString dest) {destination = dest;}

private:
    QString destination;

protected:
    virtual void keyPressEvent(QKeyEvent *e);

signals:
    void privateTextEditReturnPressed(QString destination, QString message);
};


/* Main class definition for the ChatDialog window */
class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog(NetSocket *socket);

public slots:
	void gotReturnPressed();
	void displayReceivedMessage(QString message);
	void addNewPeer();
    void updateOriginsList(QList<QString> origins);
    void startPrivateMessageSession(QListWidgetItem* selectedItem);
    void sendPrivateMessage(QString destination, QString message);
    void startSharingFiles();
    void startDownloadingFile();
    void startSearchingForFile();
    void displaySearchResults(QStringList searchResultFiles);
    void startSearchedFileDownload(QListWidgetItem* selectedItem);
    void selectImage1();
    void selectImage2();
    void startImageMatching();
    void displayMatchingResult(QString);

private:
	QTextEdit *textview;
	QTextEditSingleLine *textline;
	QLineEdit *addPeerTextLine;
    QListWidget *originsList;
    QLineEdit *downloadTargetLine;
    QLineEdit *downloadFileIdLine;
    QLineEdit *searchKeywordsLine;
    QPushButton *shareLocalFile;
    QPushButton *downloadFile;
    QPushButton *searchFile;
    QListWidget *searchedFilesList;
    QTextEdit *imageMatchOutput;
    QPushButton *image1;
    QPushButton *image2;
    QPushButton *matchImages;
    QString selectedImage1;
    QString selectedImage2;

	NetSocket *netSocket;

    QString selectImage();
};

#endif  //CHAT_DIALOG_HH
