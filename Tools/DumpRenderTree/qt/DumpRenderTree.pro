TARGET = DumpRenderTree
CONFIG  -= app_bundle
!isEqual(QT_ARCH,sh4) {
    greaterThan(QT_MAJOR_VERSION, 4):isEmpty(QT.uitools.name) {
        message("QtUiTools library not found. QWidget plugin loading will be disabled")
        DEFINES += QT_NO_UITOOLS
    } else {
        CONFIG += uitools
    }
}

BASEDIR = $$PWD/../
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..

include(../../../Source/WebKit.pri)
INCLUDEPATH += ../../../Source
INCLUDEPATH += ../../../Source/JavaScriptCore
INCLUDEPATH += ../../../Source/JavaScriptCore/ForwardingHeaders
INCLUDEPATH += ../../../Source/WebKit/qt/WebCoreSupport
INCLUDEPATH += $$BASEDIR
DESTDIR = ../../bin

unix:!mac:!symbian:!embedded {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

QT = core gui network testlib
macx: QT += xml
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

HEADERS = $$BASEDIR/WorkQueue.h \
    DumpRenderTreeQt.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    WorkQueueItemQt.h \
    LayoutTestControllerQt.h \
    GCControllerQt.h \
    PlainTextControllerQt.h \
    testplugin.h
SOURCES = ../../../Source/JavaScriptCore/wtf/Assertions.cpp \
    $$BASEDIR/WorkQueue.cpp \
    DumpRenderTreeQt.cpp \
    EventSenderQt.cpp \
    TextInputControllerQt.cpp \
    PlainTextControllerQt.cpp \
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

DEFINES += USE_SYSTEM_MALLOC=1
DEFINES -= QT_ASCII_CAST_WARNINGS

RESOURCES = DumpRenderTree.qrc
