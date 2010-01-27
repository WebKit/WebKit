TARGET = DumpRenderTree
CONFIG  -= app_bundle
CONFIG += uitools

mac:!static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
    CONFIG -= debug
    CONFIG += release
}

BASEDIR = $$PWD/../

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../..
INCLUDEPATH += ../../../JavaScriptCore
INCLUDEPATH += ../../../JavaScriptCore/ForwardingHeaders
INCLUDEPATH += $$BASEDIR
DESTDIR = ../../../bin

!win32 {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

QT = core gui network testlib
macx: QT += xml

HEADERS = $$BASEDIR/WorkQueue.h \
    DumpRenderTreeQt.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    WorkQueueItemQt.h \
    LayoutTestControllerQt.h \
    GCControllerQt.h \
    testplugin.h
SOURCES = ../../../JavaScriptCore/wtf/Assertions.cpp \
    $$BASEDIR/WorkQueue.cpp \
    DumpRenderTreeQt.cpp \
    EventSenderQt.cpp \
    TextInputControllerQt.cpp \
    WorkQueueItemQt.cpp \
    LayoutTestControllerQt.cpp \
    GCControllerQt.cpp \
    testplugin.cpp \
    main.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

DEFINES+=USE_SYSTEM_MALLOC
