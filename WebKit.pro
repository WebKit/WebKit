TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
        WebCore \
        JavaScriptCore/kjs/jsc.pro \
        WebKit/qt/QtLauncher

!win32-*: SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro

