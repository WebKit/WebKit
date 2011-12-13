# -------------------------------------------------------------------
# Project file for the DumpRenderTree binary (DRT)
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

TARGET = DumpRenderTree
DESTDIR = $$ROOT_BUILD_DIR/bin

CONFIG += uitools

load(wtf)
load(webcore)

CONFIG += qtwebkit

INCLUDEPATH += \
    $$PWD/.. \
    $${ROOT_WEBKIT_DIR}/Source/WebKit/qt/WebCoreSupport

!embedded: PKGCONFIG += fontconfig

QT = core gui network testlib
macx: QT += xml
haveQt(5): QT += widgets printsupport

HEADERS += \
    $$PWD/../WorkQueue.h \
    DumpRenderTreeQt.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    WorkQueueItemQt.h \
    LayoutTestControllerQt.h \
    GCControllerQt.h \
    PlainTextControllerQt.h \
    testplugin.h

SOURCES += \
    $${ROOT_WEBKIT_DIR}/Source/JavaScriptCore/wtf/Assertions.cpp \
    $$PWD/../WorkQueue.cpp \
    DumpRenderTreeQt.cpp \
    EventSenderQt.cpp \
    TextInputControllerQt.cpp \
    PlainTextControllerQt.cpp \
    WorkQueueItemQt.cpp \
    LayoutTestControllerQt.cpp \
    GCControllerQt.cpp \
    testplugin.cpp \
    main.cpp

wince*: {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat $$WCECOMPAT/include
    LIBS += $$WCECOMPAT/lib/wcecompat.lib
}

DEFINES -= USE_SYSTEM_MALLOC=0
DEFINES += USE_SYSTEM_MALLOC=1

RESOURCES = DumpRenderTree.qrc
