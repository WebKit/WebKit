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

    !win32:!symbian {
        SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro
        SUBDIRS += WebKitTools/DumpRenderTree/qt/ImageDiff.pro
        SUBDIRS += WebKitTools/DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
    }

}

include(WebKit/qt/docs/docs.pri)
