# -------------------------------------------------------------------
# This file contains shared rules used both when building WebKit2
# itself, and by targets that use WebKit2.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/WebKit2

WEBKIT2_GENERATED_SOURCES_DIR = $${ROOT_BUILD_DIR}/Source/WebKit2/$${GENERATED_SOURCES_DESTDIR}

INCLUDEPATH += \
    $$SOURCE_DIR \
    $$SOURCE_DIR/Platform \
    $$SOURCE_DIR/Platform/CoreIPC \
    $$SOURCE_DIR/Platform/qt \
    $$SOURCE_DIR/Shared \
    $$SOURCE_DIR/Shared/API/c \
    $$SOURCE_DIR/Shared/CoreIPCSupport \
    $$SOURCE_DIR/Shared/Plugins \
    $$SOURCE_DIR/Shared/Plugins/Netscape \
    $$SOURCE_DIR/Shared/qt \
    $$SOURCE_DIR/UIProcess \
    $$SOURCE_DIR/UIProcess/API/C \
    $$SOURCE_DIR/UIProcess/API/cpp \
    $$SOURCE_DIR/UIProcess/API/cpp/qt \
    $$SOURCE_DIR/UIProcess/API/qt \
    $$SOURCE_DIR/UIProcess/Authentication \
    $$SOURCE_DIR/UIProcess/Downloads \
    $$SOURCE_DIR/UIProcess/Launcher \
    $$SOURCE_DIR/UIProcess/Notifications \
    $$SOURCE_DIR/UIProcess/Plugins \
    $$SOURCE_DIR/UIProcess/qt \
    $$SOURCE_DIR/WebProcess \
    $$SOURCE_DIR/WebProcess/ApplicationCache \
    $$SOURCE_DIR/WebProcess/Authentication \
    $$SOURCE_DIR/WebProcess/Cookies \
    $$SOURCE_DIR/WebProcess/Cookies/qt \
    $$SOURCE_DIR/WebProcess/Downloads \
    $$SOURCE_DIR/WebProcess/Downloads/qt \
    $$SOURCE_DIR/WebProcess/FullScreen \
    $$SOURCE_DIR/WebProcess/Geolocation \
    $$SOURCE_DIR/WebProcess/IconDatabase \
    $$SOURCE_DIR/WebProcess/InjectedBundle \
    $$SOURCE_DIR/WebProcess/InjectedBundle/DOM \
    $$SOURCE_DIR/WebProcess/InjectedBundle/API/c \
    $$SOURCE_DIR/WebProcess/KeyValueStorage \
    $$SOURCE_DIR/WebProcess/MediaCache \
    $$SOURCE_DIR/WebProcess/Notifications \
    $$SOURCE_DIR/WebProcess/Plugins \
    $$SOURCE_DIR/WebProcess/Plugins/Netscape \
    $$SOURCE_DIR/WebProcess/ResourceCache \
    $$SOURCE_DIR/WebProcess/WebCoreSupport \
    $$SOURCE_DIR/WebProcess/WebCoreSupport/qt \
    $$SOURCE_DIR/WebProcess/WebPage \
    $$SOURCE_DIR/WebProcess/qt \
    $$SOURCE_DIR/PluginProcess

# The WebKit2 Qt APIs depend on qwebkitglobal.h, which lives in WebKit
INCLUDEPATH += $${ROOT_WEBKIT_DIR}/Source/WebKit/qt/Api

INCLUDEPATH += $$WEBKIT2_GENERATED_SOURCES_DIR
