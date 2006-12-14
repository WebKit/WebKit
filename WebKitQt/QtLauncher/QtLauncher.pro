TEMPLATE = app
SOURCES += main.cpp

include(../../WebKit.pri)

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib
