TEMPLATE = app
SOURCES += main.cpp

BASE_DIR = $$PWD/../..

include(../../WebKit.pri)


QMAKE_RPATHDIR += $$OUTPUT_DIR/lib
