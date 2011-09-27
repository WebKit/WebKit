TARGET = ImageDiff
CONFIG  -= app_bundle

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
include(../../../Source/WebKit.pri)
INCLUDEPATH += ../../../Source/JavaScriptCore
DESTDIR = $$OUTPUT_DIR/bin

QT = core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

SOURCES = ImageDiff.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

DEFINES -= QT_ASCII_CAST_WARNINGS
