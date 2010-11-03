TEMPLATE = lib
TARGET = TestNetscapePlugIn

VPATH = ../../unix/TestNetscapePlugin ../../TestNetscapePlugIn
isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../../../../WebKit.pri)

DESTDIR = $$OUTPUT_DIR/lib/plugins

mac {
    CONFIG += plugin
    CONFIG += plugin_bundle
    QMAKE_INFO_PLIST = ../../TestNetscapePlugIn/mac/Info.plist
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
               ../../TestNetscapePlugIn

SOURCES = PluginObject.cpp \
          PluginTest.cpp \
          TestObject.cpp \
          Tests/DocumentOpenInDestroyStream.cpp \
          Tests/EvaluateJSAfterRemovingPluginElement.cpp \
          Tests/NPRuntimeObjectFromDestroyedPlugin.cpp \
          Tests/NPRuntimeRemoveProperty.cpp \
          Tests/NullNPPGetValuePointer.cpp \
          Tests/PassDifferentNPPStruct.cpp \
          Tests/PluginScriptableNPObjectInvokeDefault.cpp

mac {
    SOURCES += ../../TestNetscapePlugIn/main.cpp
    OBJECTIVE_SOURCES += PluginObjectMac.mm
    LIBS += -framework Carbon -framework Cocoa -framework QuartzCore
} else {
    SOURCES += ../../unix/TestNetscapePlugin/TestNetscapePlugin.cpp
}
