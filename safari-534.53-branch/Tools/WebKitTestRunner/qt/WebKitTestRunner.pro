TARGET = WebKitTestRunner
CONFIG -= app_bundle

BASEDIR = $$PWD/../
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
GENERATED_SOURCES_DIR = ../generated


include(../../../Source/WebKit.pri)

DEFINES += USE_SYSTEM_MALLOC=1
DEFINES -= QT_ASCII_CAST_WARNINGS

INCLUDEPATH += \
    $$BASEDIR \
    $$BASEDIR/../../Source/JavaScriptCore \
    $$BASEDIR/../../Source/WebKit2 \
    $$BASEDIR/../../Source/WebKit2/Shared \
    $$BASEDIR/../../Source/WebKit2/UIProcess/API/qt \
    $$BASEDIR/../../Source/WebKit2/UIProcess/API/cpp/qt \
    $$GENERATED_SOURCES_DIR

INCLUDEPATH += \
    $$OUTPUT_DIR/include \


DESTDIR = $$OUTPUT_DIR/bin

unix:!mac:!symbian:!embedded {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

QT = core gui network

HEADERS = \
    $$BASEDIR/PlatformWebView.h \
    $$BASEDIR/StringFunctions.h \
    $$BASEDIR/TestController.h \
    $$BASEDIR/TestInvocation.h

SOURCES = \
    main.cpp \
    PlatformWebViewQt.cpp \
    TestControllerQt.cpp \
    TestInvocationQt.cpp \
    $$BASEDIR/TestController.cpp \
    $$BASEDIR/TestInvocation.cpp \

PREFIX_HEADER = $$BASEDIR/WebKitTestRunnerPrefix.h
*-g++*:QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

linux-* {
    # From Creator's src/rpath.pri:
    # Do the rpath by hand since it's not possible to use ORIGIN in QMAKE_RPATHDIR
    # this expands to $ORIGIN (after qmake and make), it does NOT read a qmake var.
    QMAKE_RPATHDIR = \$\$ORIGIN/../lib $$QMAKE_RPATHDIR
    MY_RPATH = $$join(QMAKE_RPATHDIR, ":")

    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$${MY_RPATH}\' -Wl,--no-undefined
    QMAKE_RPATHDIR =
} else {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

include(../../../Source/JavaScriptCore/JavaScriptCore.pri)
prependJavaScriptCoreLib(../../JavaScriptCore)
