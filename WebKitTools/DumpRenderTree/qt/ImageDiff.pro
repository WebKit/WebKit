TARGET = ImageDiff
CONFIG  -= app_bundle

include(../../../WebKit.pri)
INCLUDEPATH += ../../../JavaScriptCore
DESTDIR = ../../../bin

QT = core gui

SOURCES = ImageDiff.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}
