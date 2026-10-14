QT += core gui widgets serialbus
CONFIG += c++17
TEMPLATE = app
TARGET = CanMonitor

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    canconnection.cpp \
    canframemodel.cpp \
    filterproxymodel.cpp \
    framedelegate.cpp \
    hexutil.cpp \
    isotpreassembler.cpp \
    settingsdialog.cpp \
    trafficstatistics.cpp \
    udsclient.cpp \
    udsdecoder.cpp

HEADERS += \
    mainwindow.h \
    canconnection.h \
    canframemodel.h \
    filterproxymodel.h \
    framedelegate.h \
    hexutil.h \
    isotpreassembler.h \
    settingsdialog.h \
    trafficstatistics.h \
    udsclient.h \
    udsdecoder.h

FORMS += ui/mainwindow.ui
