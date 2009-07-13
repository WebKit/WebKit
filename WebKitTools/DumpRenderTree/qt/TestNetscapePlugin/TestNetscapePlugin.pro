TEMPLATE = lib
TARGET = TestNetscapePlugin
VPATH = ../../gtk/TestNetscapePlugin ../../TestNetscapePlugIn.subproj
include(../../../../WebKit.pri)
DESTDIR = $$OUTPUT_DIR/lib/plugins
INCLUDEPATH += ../../../../JavaScriptCore \
               ../../gtk/TestNetscapePlugin/ForwardingHeaders \
               ../../gtk/TestNetscapePlugin/ForwardingHeaders/WebKit \
               ../../../../WebCore \
               ../../../../WebCore/bridge \
               ../../TestNetscapePlugIn.subproj
SOURCES = TestNetscapePlugin.cpp \
          PluginObject.cpp \
          TestObject.cpp \
