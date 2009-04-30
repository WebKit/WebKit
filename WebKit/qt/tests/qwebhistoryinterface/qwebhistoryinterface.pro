TEMPLATE = app
TARGET = tst_qwebhistoryinterface
include(../../../../WebKit.pri)
SOURCES  += tst_qwebhistoryinterface.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
