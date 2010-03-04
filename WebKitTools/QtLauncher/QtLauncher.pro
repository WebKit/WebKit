TEMPLATE = app

SOURCES += \
    locationedit.cpp \
    main.cpp \
    mainwindow.cpp \
    urlloader.cpp \
    utils.cpp \
    webpage.cpp \
    webview.cpp \

HEADERS += \
    locationedit.h \
    mainwindow.h \
    urlloader.h \
    utils.h \
    webinspector.h \
    webpage.h \
    webview.h \

CONFIG -= app_bundle
CONFIG += uitools

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..
include(../../WebKit.pri)

DESTDIR = $$OUTPUT_DIR/bin

QT += network
macx:QT+=xml
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.UID3 = 0xA000E543
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}
