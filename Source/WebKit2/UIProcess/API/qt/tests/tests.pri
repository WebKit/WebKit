TEMPLATE = app

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

INCLUDEPATH += $$PWD
SOURCES +=  ../util.cpp

QT += testlib webkit
contains(DEFINES, HAVE_QTQUICK=1) {
    QT += qml quick quick-private
    HEADERS += ../bytearraytestdata.h \
               ../util.h

    SOURCES += ../bytearraytestdata.cpp
}
WEBKIT += wtf # For platform macros

DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD\\\" \
           QWP_PATH=\\\"$${ROOT_BUILD_DIR}/bin\\\"
