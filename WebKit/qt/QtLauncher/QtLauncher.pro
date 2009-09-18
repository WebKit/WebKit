TEMPLATE = app
SOURCES += main.cpp
CONFIG -= app_bundle
CONFIG += uitools
DESTDIR = ../../../bin

include(../../../WebKit.pri)

QT += network
macx:QT+=xml
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian:TARGET.UID3 = 0xA000E543
