# -------------------------------------------------------------------
# Project file for the DumpRenderTree binary (DRT)
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

TARGET = DumpRenderTree
DESTDIR = $$ROOT_BUILD_DIR/bin

WEBKIT += wtf webcore
!v8: WEBKIT += javascriptcore

INCLUDEPATH += \
    $$PWD/ \
    $$PWD/.. \
    $${ROOT_WEBKIT_DIR}/Source/WebKit/qt/WebCoreSupport \
    $${ROOT_WEBKIT_DIR}/Source/WTF

QT = core gui network testlib webkit widgets printsupport
macx: QT += xml

contains(DEFINES, HAVE_FONTCONFIG=1): PKGCONFIG += fontconfig

HEADERS += \
    $$PWD/../WorkQueue.h \
    $$PWD/../DumpRenderTree.h \
    DumpRenderTreeQt.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    WorkQueueItemQt.h \
    LayoutTestControllerQt.h \
    GCControllerQt.h \
    QtInitializeTestFonts.h \
    testplugin.h

SOURCES += \
    $$PWD/../WorkQueue.cpp \
    $$PWD/../DumpRenderTreeCommon.cpp \
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
    INCLUDEPATH += $$QT.core.sources/../3rdparty/ce-compat $$WCECOMPAT/include
    LIBS += $$WCECOMPAT/lib/wcecompat.lib
}

DEFINES -= USE_SYSTEM_MALLOC=0
DEFINES += USE_SYSTEM_MALLOC=1

RESOURCES = DumpRenderTree.qrc
