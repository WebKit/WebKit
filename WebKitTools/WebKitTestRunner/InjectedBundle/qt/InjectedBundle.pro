TEMPLATE = lib

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..

SOURCES += \
    ../InjectedBundle.cpp \
    ../InjectedBundle.h \
    ../InjectedBundleMain.cpp \
    ../InjectedBundlePage.cpp \
    ../InjectedBundlePage.h \
    ../EventSendingController.cpp \
    ../EventSendingController.h \
    ../GCController.cpp \
    ../GCController.h \
    ../LayoutTestController.cpp \
    ../LayoutTestController.h \
    ../Bindings/JSWrapper.cpp \
    ActivateFontsQt.cpp \
    LayoutTestControllerQt.cpp \
    $$OUTPUT_DIR/WebKitTools/WebKitTestRunner/generated/JSEventSendingController.cpp \
    $$OUTPUT_DIR/WebKitTools/WebKitTestRunner/generated/JSGCController.cpp \
    $$OUTPUT_DIR/WebKitTools/WebKitTestRunner/generated/JSLayoutTestController.cpp \

HEADERS += \
    ../ActivateFonts.h \
    ../EventSendingController.h \
    ../GCController.h \
    ../InjectedBundle.h \
    ../InjectedBundlePage.h \
    ../LayoutTestController.h \

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}

include(../../../../WebKit.pri)
include(../../../../JavaScriptCore/JavaScriptCore.pri)
addJavaScriptCoreLib(../../../../JavaScriptCore)
include(../../../../WebKit2/WebKit2.pri)
addWebKit2Lib(../../../../WebKit2)

INCLUDEPATH += \
    $$PWD \
    $$PWD/.. \
    $$PWD/../.. \
    $$PWD/../Bindings \
    $$PWD/../../../../JavaScriptCore \
    $$PWD/../../../../JavaScriptCore/wtf \
    $$PWD/../../../../WebKit2 \
    $$PWD/../../../../WebKit2/Shared \
    $$OUTPUT_DIR/WebKitTools/WebKitTestRunner/generated

INCLUDEPATH += \
    $$OUTPUT_DIR/include \
    $$OUTPUT_DIR/WebCore/generated

PREFIX_HEADER = $$PWD/../../WebKitTestRunnerPrefix.h
QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

unix:!mac {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

TARGET = WTRInjectedBundle
DESTDIR = $$OUTPUT_DIR/lib
!CONFIG(standalone_package): CONFIG -= app_bundle
