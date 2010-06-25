TEMPLATE = app
TARGET = tst_bench_qscriptvalue
QT += testlib

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../benchmarks.pri)

SOURCES += tst_qscriptvalue.cpp

