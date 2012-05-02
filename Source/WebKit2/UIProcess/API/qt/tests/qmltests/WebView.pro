include(../tests.pri)
SOURCES += tst_qmltests.cpp
TARGET = tst_qmltests_WebView
OBJECTS_DIR = obj_WebView/$$activeBuildConfig()

QT += webkit-private
CONFIG += warn_on testcase

QT -= testlib
QT += qmltest

# Test the QML files under WebView in the source repository.
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD$${QMAKE_DIR_SEP}WebView\\\"\"
DEFINES += IMPORT_DIR=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}imports\\\"\"

OTHER_FILES += \
    WebView/tst_favIconLoad.qml \
    WebView/tst_download.qml \
    WebView/tst_geopermission.qml \
    WebView/tst_itemSelector.qml \
    WebView/tst_javaScriptDialogs.qml \
    WebView/tst_loadFail.qml \
    WebView/tst_loadIgnore.qml \
    WebView/tst_loadHtml.qml \
    WebView/tst_loadProgress.qml \
    WebView/tst_loadProgressSignal.qml \
    WebView/tst_preferences.qml \
    WebView/tst_properties.qml \
    WebView/tst_titleChanged.qml \
    WebView/tst_applicationScheme.qml \
    WebView/tst_origin.qml
