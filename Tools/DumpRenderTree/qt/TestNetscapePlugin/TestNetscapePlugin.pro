# -------------------------------------------------------------------
# Project file for the NPAPI test plugin
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = TestNetscapePlugIn

CONFIG += plugin

SOURCES += \
    PluginObject.cpp \
    PluginTest.cpp \
    TestObject.cpp \
    main.cpp \
    Tests/DocumentOpenInDestroyStream.cpp \
    Tests/EvaluateJSAfterRemovingPluginElement.cpp \
    Tests/FormValue.cpp \
    Tests/GetURLNotifyWithURLThatFailsToLoad.cpp \
    Tests/GetURLWithJavaScriptURL.cpp \
    Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp \
    Tests/GetUserAgentWithNullNPPFromNPPNew.cpp \
    Tests/NPDeallocateCalledBeforeNPShutdown.cpp \
    Tests/NPPSetWindowCalledDuringDestruction.cpp \
    Tests/NPRuntimeObjectFromDestroyedPlugin.cpp \
    Tests/NPRuntimeRemoveProperty.cpp \
    Tests/NullNPPGetValuePointer.cpp \
    Tests/PassDifferentNPPStruct.cpp \
    Tests/PluginScriptableNPObjectInvokeDefault.cpp \
    Tests/PrivateBrowsing.cpp

load(webcore)

VPATH = ../../unix/TestNetscapePlugin ../../TestNetscapePlugIn


INCLUDEPATH += \
    ../../unix/TestNetscapePlugin/ForwardingHeaders \
    ../../unix/TestNetscapePlugin/ForwardingHeaders/WebKit \
    ../../TestNetscapePlugIn

DESTDIR = $${ROOT_BUILD_DIR}/lib/plugins

mac {
    CONFIG += plugin_bundle
    QMAKE_INFO_PLIST = ../../TestNetscapePlugIn/mac/Info.plist
    QMAKE_PLUGIN_BUNDLE_NAME = $$TARGET
    QMAKE_BUNDLE_LOCATION += "Contents/MacOS"

    !build_pass:CONFIG += build_all

    OBJECTIVE_SOURCES += PluginObjectMac.mm
    LIBS += -framework Carbon -framework Cocoa -framework QuartzCore
}

!win32:!embedded:!mac {
    LIBS += -lX11
    DEFINES += XP_UNIX
}
