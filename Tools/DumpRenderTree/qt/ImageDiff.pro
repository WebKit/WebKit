TARGET = ImageDiff
CONFIG  -= app_bundle

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
include(../../../Source/WebKit.pri)
INCLUDEPATH += ../../../Source/JavaScriptCore
DESTDIR = $$OUTPUT_DIR/bin

QT = core gui

SOURCES = ImageDiff.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

DEFINES -= QT_ASCII_CAST_WARNINGS
