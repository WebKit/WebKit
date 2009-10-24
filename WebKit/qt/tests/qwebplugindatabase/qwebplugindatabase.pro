TEMPLATE = app
TARGET = tst_qwebplugindatabase
include(../../../../WebKit.pri)
SOURCES  += tst_qwebplugindatabase.cpp
QT += testlib network
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.UID3 = 0xA000E540
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}
