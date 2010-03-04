TEMPLATE = app
TARGET = tst_qscriptvalue
QT += testlib
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../tests.pri)

SOURCES += \
    tst_qscriptvalue.cpp \
    tst_qscriptvalue_generated.cpp

HEADERS += \
    tst_qscriptvalue.h
