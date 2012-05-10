TEMPLATE = app

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

HEADERS += ../bytearraytestdata.h \
           ../util.h

SOURCES += ../util.cpp \
           ../bytearraytestdata.cpp
INCLUDEPATH += $$PWD

QT += testlib qml quick quick-private webkit

DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD\\\" \
           QWP_PATH=\\\"$${ROOT_BUILD_DIR}/bin\\\"
