TEMPLATE = app
TARGET = tst_qwebelement
include(../../../../WebKit.pri)
SOURCES  += tst_qwebelement.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
