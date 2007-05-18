TEMPLATE = subdirs
CONFIG += ordered
!gdk-port:CONFIG += qt-port
SUBDIRS = \
        WebCore \
        JavaScriptCore/kjs/testkjs.pro

qt-port:SUBDIRS += \
        WebKitQt/QtLauncher \
        WebKitTools/DumpRenderTree/DumpRenderTree.qtproj/DumpRenderTree.pro
gdk-port:SUBDIRS += \
        WebKitTools/GdkLauncher
