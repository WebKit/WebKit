include(../tests.pri)

CONFIG += qtwebkit-private
CONFIG += warn_on testcase

QT -= testlib
QT += qmltest

# FIXME: When webkit-private works let's use it.
load(javascriptcore)
load(webcore)
load(webkit2)

# QML files tested are the ones in WebKit source repository.
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD\\\"\"
DEFINES += IMPORT_DIR=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}imports\\\"\"

OTHER_FILES += \
    DesktopBehavior/DesktopWebView.qml \
    DesktopBehavior/tst_linkHovered.qml \
    DesktopBehavior/tst_loadHtml.qml \
    DesktopBehavior/tst_messaging.qml \
    DesktopBehavior/tst_navigationRequested.qml \
    WebView/tst_download.qml \
    WebView/tst_geopermission.qml \
    WebView/tst_javaScriptDialogs.qml \
    WebView/tst_loadFail.qml \
    WebView/tst_loadHtml.qml \
    WebView/tst_loadProgress.qml \
    WebView/tst_loadProgressSignal.qml \
    WebView/tst_preferences.qml \
    WebView/tst_properties.qml
