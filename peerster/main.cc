#include <QApplication>

#include "ChatDialog.hh"
#include "NetSocket.hh"
#include <QtCrypto>

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

    // Initialize QCA toolkit
    QCA::Initializer qcainit;

    // Get the command line arguments and add any host:port provided
    QStringList argsList = QCoreApplication::arguments();
    // argsList.at(0) = ./peerster

    bool noForward = false;
    if (argsList.size() > 1 && argsList.at(1) == "noforward") {
        noForward = true;
    }

    // Create a UDP network socket
    NetSocket *sock = new NetSocket(noForward);
    if (!(sock->bind()))
		exit(1);

	// Create an initial chat dialog window
    ChatDialog dialog(sock);
	dialog.show();


	for (int i = 1; i < argsList.size(); i++) {
        sock->addNewNeighbor(argsList.at(i));
	}

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

