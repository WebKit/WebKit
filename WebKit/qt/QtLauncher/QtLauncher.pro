TEMPLATE = app
SOURCES += main.cpp
CONFIG -= app_bundle
CONFIG += uitools
DESTDIR = ../../../bin

include(../../../WebKit.pri)

macx:QT+=xml network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
