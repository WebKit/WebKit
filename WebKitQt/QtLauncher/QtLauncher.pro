TEMPLATE = app
SOURCES += main.cpp
CONFIG -= app_bundle

include(../../WebKit.pri)

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
