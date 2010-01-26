TEMPLATE = subdirs
CONFIG += ordered

include(WebKit.pri)

SUBDIRS += \
        WebCore \
        WebKitTools/QtLauncher \
        WebKit/qt/QGVLauncher \
        WebKit/qt/tests

!CONFIG(standalone_package) {
    SUBDIRS += JavaScriptCore/jsc.pro

    !symbian: SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro

    !win32:!symbian {
        SUBDIRS += WebKitTools/DumpRenderTree/qt/ImageDiff.pro
        SUBDIRS += WebKitTools/DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
    }

}

build-qtscript {
    SUBDIRS += \
        JavaScriptCore/qt/api/QtScript.pro \
        JavaScriptCore/qt/tests
}

include(WebKit/qt/docs/docs.pri)
