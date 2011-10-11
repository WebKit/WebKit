TEMPLATE = lib

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..

isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = ../../../../Source/WebCore/generated

QT += declarative widgets

GENERATED_SOURCES_DIR = ../../generated

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
    ../TextInputController.cpp \
    ../TextInputController.h \
    ../Bindings/JSWrapper.cpp \
    ActivateFontsQt.cpp \
    InjectedBundleQt.cpp \
    LayoutTestControllerQt.cpp \
    $$GENERATED_SOURCES_DIR/JSEventSendingController.cpp \
    $$GENERATED_SOURCES_DIR/JSGCController.cpp \
    $$GENERATED_SOURCES_DIR/JSLayoutTestController.cpp \
    $$GENERATED_SOURCES_DIR/JSTextInputController.cpp \

HEADERS += \
    ../ActivateFonts.h \
    ../EventSendingController.h \
    ../GCController.h \
    ../InjectedBundle.h \
    ../InjectedBundlePage.h \
    ../LayoutTestController.h \
    ../TextInputController.h \

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}

include(../../../../Source/WebKit.pri)
include(../../../../Source/JavaScriptCore/JavaScriptCore.pri)
prependJavaScriptCoreLib(../../../JavaScriptCore)
include(../../../../Source/WebKit2/WebKit2.pri)
prependWebKit2Lib(../../../WebKit2)

INCLUDEPATH = \
    $$PWD \
    $$PWD/.. \
    $$PWD/../.. \
    $$PWD/../Bindings \
    $$PWD/../../../../Source \
    $$PWD/../../../../Source/JavaScriptCore \
    $$PWD/../../../../Source/JavaScriptCore/ForwardingHeaders \
    $$PWD/../../../../Source/JavaScriptCore/wtf/unicode \
    $$PWD/../../../../Source/WebCore \
    $$PWD/../../../../Source/WebCore/platform/text \
    $$PWD/../../../../Source/WebCore/testing/js \
    $$PWD/../../../../Source/WebKit2 \
    $$PWD/../../../../Source/WebKit2/Shared \
    $$OUTPUT_DIR/include/QtWebKit \
    $$OUTPUT_DIR/include \
    $$GENERATED_SOURCES_DIR \
    $$WC_GENERATED_SOURCES_DIR


PREFIX_HEADER = $$PWD/../../WebKitTestRunnerPrefix.h
*-g++*:QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

unix:!mac:!symbian:!embedded {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

TARGET = WTRInjectedBundle
DESTDIR = $$OUTPUT_DIR/lib
CONFIG -= app_bundle
linux-* {
    QMAKE_LFLAGS += -Wl,--no-undefined
}
