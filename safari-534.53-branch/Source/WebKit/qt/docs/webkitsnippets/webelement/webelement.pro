TEMPLATE = app
CONFIG -= app_bundle
CONFIG(QTDIR_build) {
    QT       += webkit
}
SOURCES   = main.cpp
include(../../../../../WebKit.pri)
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
