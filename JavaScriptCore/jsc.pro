TEMPLATE = app
TARGET = jsc
DESTDIR = .
SOURCES = jsc.cpp
QT -= gui
CONFIG -= app_bundle
CONFIG += building-libs

include($$PWD/../WebKit.pri)

CONFIG += link_pkgconfig

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/..
include($$OUTPUT_DIR/config.pri)
CONFIG(debug, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}
OBJECTS_DIR_WTR = $$OBJECTS_DIR$${QMAKE_DIR_SEP}
include($$PWD/JavaScriptCore.pri)

lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}

*-g++*:QMAKE_CXXFLAGS_RELEASE -= -O2
*-g++*:QMAKE_CXXFLAGS_RELEASE += -O3
