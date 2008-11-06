TEMPLATE = app
TARGET = jsc
DESTDIR = ..
SOURCES = jsc.cpp
QT -= gui
INCLUDEPATH += $$PWD/.. \
    $$PWD \
    $$PWD/../bindings \
    $$PWD/../bindings/c \
    $$PWD/../wtf \
    $$PWD/../VM
CONFIG -= app_bundle
DEFINES += BUILDING_QT__
CONFIG += building-libs

CONFIG(release) {
    DEFINES += NDEBUG USE_SYSTEM_MALLOC
}

include($$PWD/../../WebKit.pri)

CONFIG += link_pkgconfig

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/../..
include($$OUTPUT_DIR/config.pri)
OBJECTS_DIR = tmp
OBJECTS_DIR_WTR = $$OBJECTS_DIR/
win32-*: OBJECTS_DIR_WTR ~= s|/|\|
include($$PWD/../JavaScriptCore.pri)

lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}
