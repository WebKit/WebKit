TARGET = DumpRenderTree

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../../JavaScriptCore/kjs


QT = core gui

HEADERS = DumpRenderTree.h jsobjects.h
SOURCES = DumpRenderTree.cpp main.cpp jsobjects.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}
