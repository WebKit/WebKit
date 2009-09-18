TEMPLATE = app
TARGET = tst_painting
include(../../../../../WebKit.pri)
SOURCES += tst_painting.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian:TARGET.UID3 = 0xA000E542
