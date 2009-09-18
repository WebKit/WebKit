TEMPLATE = app
TARGET = tst_loading
include(../../../../../WebKit.pri)
SOURCES += tst_loading.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian:TARGET.UID3 = 0xA000E541
