TEMPLATE = app
TARGET = tst_qwebpage
include(../../../../WebKit.pri)
SOURCES  += tst_qwebpage.cpp
RESOURCES  += tst_qwebpage.qrc
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
