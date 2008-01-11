TEMPLATE = subdirs
CONFIG += ordered
!gtk-port:CONFIG += qt-port
qt-port:!win32-*:SUBDIRS += WebKit/qt/Plugins
SUBDIRS += \
        WebCore \
        JavaScriptCore/kjs/testkjs.pro

qt-port {
    SUBDIRS += WebKit/qt/QtLauncher

    !win32-*: SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro
}

gtk-port:SUBDIRS += \
        WebKitTools/GtkLauncher \
        WebKitTools/DumpRenderTree/gtk/DumpRenderTree.pro
