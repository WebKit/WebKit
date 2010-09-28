TARGET = DumpRenderTree
CONFIG  -= app_bundle
CONFIG += uitools

BASEDIR = $$PWD/../
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..

include(../../../WebKit.pri)
INCLUDEPATH += ../../..
INCLUDEPATH += ../../../JavaScriptCore
INCLUDEPATH += ../../../JavaScriptCore/ForwardingHeaders
INCLUDEPATH += $$BASEDIR
DESTDIR = ../../../bin

unix:!mac {
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

wince*: {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat $$WCECOMPAT/include
    LIBS += $$WCECOMPAT/lib/wcecompat.lib
}

DEFINES+=USE_SYSTEM_MALLOC
