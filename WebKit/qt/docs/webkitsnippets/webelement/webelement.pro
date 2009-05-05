TEMPLATE = app
CONFIG -= app_bundle
SOURCES   = main.cpp
include(../../../../../WebKit.pri)
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
