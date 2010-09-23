TEMPLATE = app
TARGET = jsc
DESTDIR = .
SOURCES = jsc.cpp
QT -= gui
CONFIG -= app_bundle
CONFIG += building-libs
win32-*: CONFIG += console
win32-msvc*: CONFIG += exceptions_off stl_off

isEmpty(OUTPUT_DIR): OUTPUT_DIR= ..
include($$PWD/../WebKit.pri)

CONFIG += link_pkgconfig

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}
OBJECTS_DIR_WTR = $$OBJECTS_DIR$${QMAKE_DIR_SEP}
include($$PWD/JavaScriptCore.pri)
addJavaScriptCoreLib(.)

symbian {
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}

mac {
    LIBS_PRIVATE += -framework AppKit
}

wince* {
    LIBS += mmtimer.lib
}

# Prevent warnings about difference in visibility on Mac OS X
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
