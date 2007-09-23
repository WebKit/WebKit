TEMPLATE = app
SOURCES += main.cpp
CONFIG -= app_bundle

BASE_DIR = $$PWD/../..

include(../../WebKit.pri)


QMAKE_RPATHDIR += $$OUTPUT_DIR/lib
