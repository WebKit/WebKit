TEMPLATE = app
CONFIG -= app_bundle

VPATH += $$_PRO_FILE_PWD_
# Add the tst_ prefix, In QTDIR_build it's done by qttest_p4.prf
CONFIG(QTDIR_build) { load(qttest_p4) }
ELSE { TARGET = tst_$$TARGET }

SOURCES += $${TARGET}.cpp \
           ../util.cpp
INCLUDEPATH += $$PWD

include(../../../../../WebKit.pri)
QT += testlib declarative widgets

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD\\\" \
           QWP_PATH=\\\"$$OUTPUT_DIR/bin\\\"
