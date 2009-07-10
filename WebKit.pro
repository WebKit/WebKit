TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
        WebCore \
        JavaScriptCore/jsc.pro \
        WebKit/qt/QtLauncher \
        WebKit/qt/tests

!win32:!symbian: SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro

include(WebKit/qt/docs/docs.pri)
