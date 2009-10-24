TEMPLATE = app
TARGET = tst_qgraphicswebview
include(../../../../WebKit.pri)
SOURCES  += tst_qgraphicswebview.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}
