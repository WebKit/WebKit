# -------------------------------------------------------------------
# Target file for the WebKit2 QML static library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = WebKit2QML

WEBKIT += wtf javascriptcore webcore webkit2

load(webkit_modules)

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/WebKit2

# Remove include paths that point to directories containing
# internal API, to prevent the accidental inclusion.
INCLUDEPATH -= \
    $$SOURCE_DIR \
    $$SOURCE_DIR/Platform \
    $$SOURCE_DIR/Platform/CoreIPC \
    $$SOURCE_DIR/Platform/qt \
    $$SOURCE_DIR/Shared \
    $$SOURCE_DIR/Shared/linux/SandboxProcess \
    $$SOURCE_DIR/Shared/Authentication \
    $$SOURCE_DIR/Shared/CoordinatedGraphics \
    $$SOURCE_DIR/Shared/CoreIPCSupport \
    $$SOURCE_DIR/Shared/Downloads \
    $$SOURCE_DIR/Shared/Downloads/qt \
    $$SOURCE_DIR/Shared/Network \
    $$SOURCE_DIR/Shared/Plugins \
    $$SOURCE_DIR/Shared/Plugins/Netscape \
    $$SOURCE_DIR/Shared/qt \
    $$SOURCE_DIR/UIProcess \
    $$SOURCE_DIR/UIProcess/API/qt \
    $$SOURCE_DIR/UIProcess/Authentication \
    $$SOURCE_DIR/UIProcess/CoordinatedGraphics \
    $$SOURCE_DIR/UIProcess/Downloads \
    $$SOURCE_DIR/UIProcess/InspectorServer \
    $$SOURCE_DIR/UIProcess/InspectorServer/qt \
    $$SOURCE_DIR/UIProcess/Launcher \
    $$SOURCE_DIR/UIProcess/Notifications \
    $$SOURCE_DIR/UIProcess/Plugins \
    $$SOURCE_DIR/UIProcess/Storage \
    $$SOURCE_DIR/UIProcess/qt \
    $$SOURCE_DIR/UIProcess/texmap \
    $$SOURCE_DIR/WebProcess \
    $$SOURCE_DIR/WebProcess/ApplicationCache \
    $$SOURCE_DIR/WebProcess/Battery \
    $$SOURCE_DIR/WebProcess/Cookies \
    $$SOURCE_DIR/WebProcess/Cookies/qt \
    $$SOURCE_DIR/WebProcess/FullScreen \
    $$SOURCE_DIR/WebProcess/Geolocation \
    $$SOURCE_DIR/WebProcess/IconDatabase \
    $$SOURCE_DIR/WebProcess/InjectedBundle \
    $$SOURCE_DIR/WebProcess/InjectedBundle/DOM \
    $$SOURCE_DIR/WebProcess/InjectedBundle/API/c \
    $$SOURCE_DIR/WebProcess/MediaCache \
    $$SOURCE_DIR/WebProcess/NetworkInfo \
    $$SOURCE_DIR/WebProcess/Notifications \
    $$SOURCE_DIR/WebProcess/Plugins \
    $$SOURCE_DIR/WebProcess/Plugins/Netscape \
    $$SOURCE_DIR/WebProcess/ResourceCache \
    $$SOURCE_DIR/WebProcess/Storage \
    $$SOURCE_DIR/WebProcess/WebCoreSupport \
    $$SOURCE_DIR/WebProcess/WebCoreSupport/qt \
    $$SOURCE_DIR/WebProcess/WebPage \
    $$SOURCE_DIR/WebProcess/WebPage/CoordinatedGraphics \
    $$SOURCE_DIR/WebProcess/qt \
    $$SOURCE_DIR/PluginProcess

CONFIG += staticlib

SOURCES += \
        UIProcess/API/qt/qwebnavigationhistory.cpp

HEADERS += \
        UIProcess/API/qt/qwebnavigationhistory_p.h \
        UIProcess/API/qt/qwebnavigationhistory_p_p.h

