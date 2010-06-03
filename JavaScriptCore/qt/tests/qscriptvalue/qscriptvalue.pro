TEMPLATE = app
TARGET = tst_qscriptvalue
QT += testlib
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../tests.pri)

SOURCES += \
    tst_qscriptvalue.cpp \
    tst_qscriptvalue_generated_init.cpp \
    tst_qscriptvalue_generated_comparison.cpp \
    tst_qscriptvalue_generated_istype.cpp \
    tst_qscriptvalue_generated_totype.cpp \

HEADERS += \
    tst_qscriptvalue.h
