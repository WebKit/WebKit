TARGET = WebKitTestRunner
CONFIG -= app_bundle

BASEDIR = $$PWD/../
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
GENERATED_SOURCES_DIR = ../generated


include(../../../WebKit.pri)

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}

DEFINES += USE_SYSTEM_MALLOC

INCLUDEPATH += \
    $$BASEDIR \
    $$BASEDIR/../../JavaScriptCore \
    $$BASEDIR/../../WebKit2 \
    $$BASEDIR/../../WebKit2/Shared \
    $$BASEDIR/../../WebKit2/UIProcess/API/qt \
    $$BASEDIR/../../WebKit2/UIProcess/API/cpp/qt \
    $$GENERATED_SOURCES_DIR

INCLUDEPATH += \
    $$OUTPUT_DIR/include \


DESTDIR = $$OUTPUT_DIR/bin

unix:!mac {
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
    $$BASEDIR/TestController.cpp \
    $$BASEDIR/TestInvocation.cpp \

PREFIX_HEADER = $$BASEDIR/WebKitTestRunnerPrefix.h
QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

linux-* {
    # From Creator's src/rpath.pri:
    # Do the rpath by hand since it's not possible to use ORIGIN in QMAKE_RPATHDIR
    # this expands to $ORIGIN (after qmake and make), it does NOT read a qmake var.
    QMAKE_RPATHDIR = \$\$ORIGIN/../lib $$QMAKE_RPATHDIR
    MY_RPATH = $$join(QMAKE_RPATHDIR, ":")

    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$${MY_RPATH}\'
    QMAKE_RPATHDIR =
} else {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

include(../../../JavaScriptCore/JavaScriptCore.pri)
addJavaScriptCoreLib(../../../JavaScriptCore)
