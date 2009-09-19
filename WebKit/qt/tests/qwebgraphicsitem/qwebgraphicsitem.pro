TEMPLATE = app
TARGET = tst_qwebgraphicsitem
include(../../../../WebKit.pri)
SOURCES  += tst_qwebgraphicsitem.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
