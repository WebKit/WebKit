TEMPLATE = app
TARGET = MiniBrowser

SOURCES += \
    BrowserWindow.cpp \
    main.cpp \
    MiniBrowserApplication.cpp \
    UrlLoader.cpp \
    utils.cpp \

HEADERS += \
    BrowserWindow.h \
    MiniBrowserApplication.h \
    UrlLoader.h \
    utils.h \

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
include(../../../Source/WebKit.pri)

INCLUDEPATH += \
    $$PWD/../../../Source/WebKit2/ \
    $$PWD/../../../Source/WebKit2/UIProcess/API/cpp \
    $$PWD/../../../Source/WebKit2/UIProcess/API/C \
    $$PWD/../../../Source/WebKit2/UIProcess/API/qt \
    $$OUTPUT_DIR/include


DESTDIR = $$OUTPUT_DIR/bin
CONFIG -= app_bundle

QT += network declarative
macx:QT+=xml

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

DEFINES -= QT_ASCII_CAST_WARNINGS

# Use the MiniBrowser.qrc file from the sources.
RESOURCES += MiniBrowser.qrc
