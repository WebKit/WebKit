TEMPLATE = app
TARGET = tst_qwebview
include(../../../../WebKit.pri)
SOURCES  += tst_qwebview.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
