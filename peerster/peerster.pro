######################################################################
# Automatically generated by qmake (2.01a) Mon Aug 27 15:02:08 2012
######################################################################

TEMPLATE = app
TARGET = peerster
DEPENDPATH += .
INCLUDEPATH += .
QT += network
CONFIG += crypto
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Input
HEADERS += main.hh \
    Peer.hh \
    Router.hh \
    FileBlock.hh \
    SharedFile.hh \
    OngoingDownload.hh \
    ImageProcessor.hh \
    PeerSession.hh
HEADERS += ChatDialog.hh
HEADERS += NetSocket.hh
HEADERS += MessageManager.hh
HEADERS += FileShareManager.hh

SOURCES += main.cc \
    Peer.cc \
    Router.cc \
    FileBlock.cc \
    SharedFile.cc \
    OngoingDownload.cc \
    ImageProcessor.cc \
    PeerSession.cc
SOURCES += ChatDialog.cc
SOURCES += NetSocket.cc
SOURCES += MessageManager.cc
SOURCES += FileShareManager.cc
