TEMPLATE = app
TARGET = jsc
DESTDIR = ..
SOURCES = Shell.cpp
QT -= gui
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
INCLUDEPATH += $$PWD/.. \
    $$PWD \
    $$PWD/../bindings \
    $$PWD/../bindings/c \
    $$PWD/../wtf \
    $$PWD/../VM
CONFIG -= app_bundle
DEFINES += BUILDING_QT__

CONFIG += link_pkgconfig

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/../..
include($$OUTPUT_DIR/config.pri)
OBJECTS_DIR = tmp
OBJECTS_DIR_WTR = $$OBJECTS_DIR/
win32-*: OBJECTS_DIR_WTR ~= s|/|\|
include($$PWD/../JavaScriptCore.pri)

# Hack!  Fix this.
SOURCES -= API/JSBase.cpp \
    API/JSCallbackConstructor.cpp \
    API/JSCallbackFunction.cpp \
    API/JSCallbackObject.cpp \
    API/JSClassRef.cpp \
    API/JSContextRef.cpp \
    API/JSObjectRef.cpp \
    API/JSStringRef.cpp \
    API/JSValueRef.cpp

lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}
