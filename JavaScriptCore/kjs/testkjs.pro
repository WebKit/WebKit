TEMPLATE = app
TARGET = testkjs
DESTDIR = ..
SOURCES = testkjs.cpp
QT -= gui
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
INCLUDEPATH += $$PWD/.. $$PWD $$PWD/../bindings $$PWD/../bindings/c $$PWD/../wtf
CONFIG -= app_bundle
qt-port:DEFINES += BUILDING_QT__
#qt-port:LIBS += -L$$OUTPUT_DIR/lib -lQtWebKit
gtk-port {
    QMAKE_CXXFLAGS += $$system(icu-config --cppflags)
    LIBS += $$system(icu-config --ldflags)
}
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

