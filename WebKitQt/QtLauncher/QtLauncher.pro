TEMPLATE = app
SOURCES += main.cpp
CONFIG -= app_bundle
DESTDIR = ../../bin

include(../../WebKit.pri)

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
