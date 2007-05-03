TEMPLATE = subdirs
CONFIG += ordered
!gdk-port:CONFIG += qt-port
SUBDIRS = \
        WebCore

qt-port:SUBDIRS += \
        WebKitQt/QtLauncher \
        WebKitTools/DumpRenderTree/DumpRenderTree.qtproj/DumpRenderTree.pro \
        JavaScriptCore/kjs/testkjs.pro
gdk-port:SUBDIRS += \
        WebKitTools/GdkLauncher
