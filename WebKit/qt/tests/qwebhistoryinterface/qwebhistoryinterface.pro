TEMPLATE = app
TARGET = tst_qwebhistoryinterface
include(../../../../WebKit.pri)
SOURCES  += tst_qwebhistoryinterface.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.UID3 = 0xA000E53C
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}
