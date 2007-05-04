TARGET = DumpRenderTree

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../../JavaScriptCore/kjs


QT = core gui

HEADERS = DumpRenderTree.h jsobjects.h
SOURCES = DumpRenderTree.cpp main.cpp jsobjects.cpp

