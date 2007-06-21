TEMPLATE = app
TARGET = testkjs
DESTDIR = ..
SOURCES = testkjs.cpp
QT -= gui
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
INCLUDEPATH += $$PWD/.. $$PWD $$PWD/../bindings $$PWD/../bindings/c $$PWD/../wtf
qt-port:DEFINES += BUILDING_QT__
#qt-port:LIBS += -L$$OUTPUT_DIR/lib -lQtWebKit
gdk-port {
    QMAKE_CXXFLAGS += $$system(icu-config --cppflags)
    LIBS += $$system(icu-config --ldflags)
}
QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/../..
include($$OUTPUT_DIR/config.pri)

include($$PWD/../JavaScriptCore.pri)

# Hack!  Fix this.
SOURCES -= API/JSValueRef.cpp
SOURCES -= API/JSCallbackObject.cpp
