TEMPLATE = app
TARGET = tst_qmltests
CONFIG += warn_on testcase
SOURCES += tst_qmltests.cpp

QT += declarative qmltest

# QML files tested are the ones in WebKit source repository.
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD\\\"\"

message($$PWD)

OTHER_FILES += \
    DesktopWebView/tst_properties.qml \
    TouchWebView/tst_properties.qml

