TEMPLATE = lib
TARGET = TestNetscapePlugIn

VPATH = ../../unix/TestNetscapePlugin ../../TestNetscapePlugIn.subproj
include(../../../../WebKit.pri)

DESTDIR = $$OUTPUT_DIR/lib/plugins

mac {
    CONFIG += plugin
    CONFIG += plugin_bundle
    QMAKE_INFO_PLIST = ../../TestNetscapePlugIn.subproj/Info.plist
    QMAKE_PLUGIN_BUNDLE_NAME = $$TARGET
    QMAKE_BUNDLE_LOCATION += "Contents/MacOS"

    !build_pass:CONFIG += build_all
    debug_and_release:TARGET = $$qtLibraryTarget($$TARGET)
}

INCLUDEPATH += ../../../../JavaScriptCore \
               ../../unix/TestNetscapePlugin/ForwardingHeaders \
               ../../unix/TestNetscapePlugin/ForwardingHeaders/WebKit \
               ../../../../WebCore \
               ../../../../WebCore/bridge \
               ../../TestNetscapePlugIn.subproj

SOURCES = PluginObject.cpp \
          TestObject.cpp

mac {
    SOURCES += ../../TestNetscapePlugIn.subproj/main.cpp
    LIBS += -framework Carbon
} else {
    SOURCES += ../../unix/TestNetscapePlugin/TestNetscapePlugin.cpp
}
