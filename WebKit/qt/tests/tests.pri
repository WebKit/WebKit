TEMPLATE = app
CONFIG -= app_bundle

VPATH += $$_PRO_FILE_PWD_
!CONFIG(QTDIR_build):TARGET = tst_$$TARGET
SOURCES += $${TARGET}.cpp
INCLUDEPATH += \
    $$PWD \
    $$PWD/../Api

exists($$_PRO_FILE_PWD_/$${TARGET}.qrc):RESOURCES += $$_PRO_FILE_PWD_/$${TARGET}.qrc

include(../../../WebKit.pri)
QT += testlib network

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}

# This define is used by some tests to look up resources in the source tree
!symbian: DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"

