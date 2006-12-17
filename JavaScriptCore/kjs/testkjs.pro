TEMPLATE = app
TARGET = testkjs
DESTDIR = ..
SOURCES = testkjs.cpp
QT -= gui
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
DEFINES += BUILDING_QT__
INCLUDEPATH += $$PWD/.. $$PWD $$PWD/../bindings $$PWD/../bindings/c $$PWD/../wtf
LIBS += -L$$OUTPUT_DIR/lib -lJavaScriptCore
QMAKE_RPATHDIR += $$OUTPUT_DIR/lib

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/../..
include($$OUTPUT_DIR/config.pri)

