# -------------------------------------------------------------------
# Project file for the DumpRenderTree binary (DRT)
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

TARGET = DumpRenderTree
DESTDIR = $$ROOT_BUILD_DIR/bin

load(features)

WEBKIT += wtf webcore
!v8: WEBKIT += javascriptcore

INCLUDEPATH += \
    $$PWD/.. \
    $${ROOT_WEBKIT_DIR}/Source/WebKit/qt/WebCoreSupport \
    $${ROOT_WEBKIT_DIR}/Source/WTF

QT = core gui network testlib webkit
macx: QT += xml
haveQt(5): QT += widgets printsupport

contains(DEFINES, HAVE_FONTCONFIG=1): PKGCONFIG += fontconfig

HEADERS += \
    $$PWD/../WorkQueue.h \
    DumpRenderTreeQt.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    WorkQueueItemQt.h \
    LayoutTestControllerQt.h \
    GCControllerQt.h \
    QtInitializeTestFonts.h \
    testplugin.h

SOURCES += \
    $${ROOT_WEBKIT_DIR}/Source/WTF/wtf/Assertions.cpp \
    $$PWD/../WorkQueue.cpp \
    DumpRenderTreeQt.cpp \
    EventSenderQt.cpp \
    TextInputControllerQt.cpp \
    WorkQueueItemQt.cpp \
    LayoutTestControllerQt.cpp \
    GCControllerQt.cpp \
    QtInitializeTestFonts.cpp \
    testplugin.cpp \
    main.cpp

wince*: {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat $$WCECOMPAT/include
    LIBS += $$WCECOMPAT/lib/wcecompat.lib
}

DEFINES -= USE_SYSTEM_MALLOC=0
DEFINES += USE_SYSTEM_MALLOC=1

mac: LIB_SUFFIX=.dylib
win: LIB_SUFFIX=.dll
unix:!mac: LIB_SUFFIX=.so
DEFINES += TEST_PLATFORM_PLUGIN_PATH=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}lib$${QMAKE_DIR_SEP}libtestplatform$${LIB_SUFFIX}\\\"\"

RESOURCES = DumpRenderTree.qrc
