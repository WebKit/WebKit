TEMPLATE = subdirs
CONFIG += ordered
!gdk-port:CONFIG += qt-port
qt-port:!win32-*:SUBDIRS += WebKitQt/Plugins
SUBDIRS += \
        JavaScriptCore/pcre/dftables.pro \
        WebCore \
        JavaScriptCore/kjs/testkjs.pro

qt-port {
    SUBDIRS += WebKitQt/QtLauncher

    !win32-*: SUBDIRS += WebKitTools/DumpRenderTree/DumpRenderTree.qtproj/DumpRenderTree.pro
}

gdk-port:SUBDIRS += \
        WebKitTools/GdkLauncher
