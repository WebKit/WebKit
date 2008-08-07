TEMPLATE = app
TARGET = tst_qwebframe
include(../../../../WebKit.pri)
SOURCES  += tst_qwebframe.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
