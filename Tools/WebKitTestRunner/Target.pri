# -------------------------------------------------------------------
# Project file for WebKitTestRunner binary (WTR)
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app
TARGET = WebKitTestRunner

HEADERS += \
    $${ROOT_WEBKIT_DIR}/Tools/DumpRenderTree/qt/QtInitializeTestFonts.h \
    EventSenderProxy.h \
    PlatformWebView.h \
    StringFunctions.h \
    TestController.h \
    TestInvocation.h

SOURCES += \
    $${ROOT_WEBKIT_DIR}/Tools/DumpRenderTree/qt/QtInitializeTestFonts.cpp \
    qt/main.cpp \
    qt/EventSenderProxyQt.cpp \
    qt/PlatformWebViewQt.cpp \
    qt/TestControllerQt.cpp \
    qt/TestInvocationQt.cpp \
    TestController.cpp \
    TestInvocation.cpp

DESTDIR = $${ROOT_BUILD_DIR}/bin

QT = core gui gui-private widgets network testlib quick quick-private webkit

WEBKIT += wtf javascriptcore webkit2

DEFINES += USE_SYSTEM_MALLOC=1

contains(DEFINES, HAVE_FONTCONFIG=1): PKGCONFIG += fontconfig

INCLUDEPATH += \
    $${ROOT_WEBKIT_DIR}/Tools/DumpRenderTree/qt

PREFIX_HEADER = WebKitTestRunnerPrefix.h
*-g++*:QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"
*-clang*:QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

RESOURCES = qt/WebKitTestRunner.qrc
