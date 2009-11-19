TEMPLATE = app
TARGET = tst_qwebinspector
include(../../../../WebKit.pri)
SOURCES  += tst_qwebinspector.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
