TEMPLATE = app
TARGET = tst_qscriptengine
QT += testlib
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../tests.pri)

SOURCES += tst_qscriptengine.cpp

