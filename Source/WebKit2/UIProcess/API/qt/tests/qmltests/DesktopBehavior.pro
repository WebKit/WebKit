include(../tests.pri)
SOURCES += tst_qmltests.cpp
TARGET = tst_qmltests_DesktopBehavior
OBJECTS_DIR = obj_DesktopBehavior/$$activeBuildConfig()

CONFIG += qtwebkit-private
CONFIG += warn_on testcase

QT -= testlib
QT += qmltest

DEFINES += DISABLE_FLICKABLE_VIEWPORT=1
# Test the QML files under DesktopBehavior in the source repository.
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD$${QMAKE_DIR_SEP}DesktopBehavior\\\"\"
DEFINES += IMPORT_DIR=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}imports\\\"\"

OTHER_FILES += \
    DesktopBehavior/DesktopWebView.qml \
    DesktopBehavior/tst_linkHovered.qml \
    DesktopBehavior/tst_loadHtml.qml \
    DesktopBehavior/tst_messaging.qml \
    DesktopBehavior/tst_navigationRequested.qml
