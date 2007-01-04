unix {
TARGET = DumpRenderTree

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../../JavaScriptCore/kjs


QT = core gui

HEADERS = DumpRenderTreeClient.h DumpRenderTree.h jsobjects.h
SOURCES = DumpRenderTreeClient.cpp DumpRenderTree.cpp main.cpp fontoverload.cpp jsobjects.cpp

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib
}
