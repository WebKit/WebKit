# -------------------------------------------------------------------
# Target file for the WebKit1 static library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = WebKit1

include(WebKit1.pri)

WEBKIT += wtf javascriptcore webcore
QT += gui

CONFIG += staticlib

SOURCES += \
    $$PWD/qt/Api/qwebframe.cpp \
    $$PWD/qt/Api/qgraphicswebview.cpp \
    $$PWD/qt/Api/qwebpage.cpp \
    $$PWD/qt/Api/qwebview.cpp \
    $$PWD/qt/Api/qwebelement.cpp \
    $$PWD/qt/Api/qwebhistory.cpp \
    $$PWD/qt/Api/qwebsettings.cpp \
    $$PWD/qt/Api/qwebhistoryinterface.cpp \
    $$PWD/qt/Api/qwebplugindatabase.cpp \
    $$PWD/qt/Api/qwebpluginfactory.cpp \
    $$PWD/qt/Api/qwebsecurityorigin.cpp \
    $$PWD/qt/Api/qwebscriptworld.cpp \
    $$PWD/qt/Api/qwebdatabase.cpp \
    $$PWD/qt/Api/qwebinspector.cpp \
    $$PWD/qt/Api/qwebkitversion.cpp \
    $$PWD/qt/Api/qhttpheader.cpp \
    $$PWD/qt/WebCoreSupport/QtFallbackWebPopup.cpp \
    $$PWD/qt/WebCoreSupport/QtWebComboBox.cpp \
    $$PWD/qt/WebCoreSupport/ChromeClientQt.cpp \
    $$PWD/qt/WebCoreSupport/ContextMenuClientQt.cpp \
    $$PWD/qt/WebCoreSupport/DragClientQt.cpp \
    $$PWD/qt/WebCoreSupport/DumpRenderTreeSupportQt.cpp \
    $$PWD/qt/WebCoreSupport/EditorClientQt.cpp \
    $$PWD/qt/WebCoreSupport/UndoStepQt.cpp \
    $$PWD/qt/WebCoreSupport/FrameLoaderClientQt.cpp \
    $$PWD/qt/WebCoreSupport/FrameNetworkingContextQt.cpp \
    $$PWD/qt/WebCoreSupport/GeolocationPermissionClientQt.cpp \
    $$PWD/qt/WebCoreSupport/InitWebCoreQt.cpp \
    $$PWD/qt/WebCoreSupport/InspectorClientQt.cpp \
    $$PWD/qt/WebCoreSupport/InspectorServerQt.cpp \
    $$PWD/qt/WebCoreSupport/NotificationPresenterClientQt.cpp \
    $$PWD/qt/WebCoreSupport/PageClientQt.cpp \
    $$PWD/qt/WebCoreSupport/PopupMenuQt.cpp \
    $$PWD/qt/WebCoreSupport/QtPlatformPlugin.cpp \
    $$PWD/qt/WebCoreSupport/RenderThemeQStyle.cpp \
    $$PWD/qt/WebCoreSupport/ScrollbarThemeQStyle.cpp \
    $$PWD/qt/WebCoreSupport/SearchPopupMenuQt.cpp \
    $$PWD/qt/WebCoreSupport/TextCheckerClientQt.cpp \
    $$PWD/qt/WebCoreSupport/PlatformStrategiesQt.cpp \
    $$PWD/qt/WebCoreSupport/WebEventConversion.cpp

HEADERS += \
    $$PWD/qt/Api/qwebframe.h \
    $$PWD/qt/Api/qwebframe_p.h \
    $$PWD/qt/Api/qgraphicswebview.h \
    $$PWD/qt/Api/qwebkitglobal.h \
    $$PWD/qt/Api/qwebkitplatformplugin.h \
    $$PWD/qt/Api/qwebpage.h \
    $$PWD/qt/Api/qwebview.h \
    $$PWD/qt/Api/qwebsettings.h \
    $$PWD/qt/Api/qwebhistoryinterface.h \
    $$PWD/qt/Api/qwebdatabase.h \
    $$PWD/qt/Api/qwebsecurityorigin.h \
    $$PWD/qt/Api/qwebelement.h \
    $$PWD/qt/Api/qwebelement_p.h \
    $$PWD/qt/Api/qwebpluginfactory.h \
    $$PWD/qt/Api/qwebhistory.h \
    $$PWD/qt/Api/qwebinspector.h \
    $$PWD/qt/Api/qwebkitversion.h \
    $$PWD/qt/Api/qwebplugindatabase_p.h \
    $$PWD/qt/Api/qhttpheader_p.h \
    $$PWD/qt/WebCoreSupport/InitWebCoreQt.h \
    $$PWD/qt/WebCoreSupport/InspectorServerQt.h \
    $$PWD/qt/WebCoreSupport/QtFallbackWebPopup.h \
    $$PWD/qt/WebCoreSupport/QtWebComboBox.h \
    $$PWD/qt/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/qt/WebCoreSupport/FrameNetworkingContextQt.h \
    $$PWD/qt/WebCoreSupport/GeolocationPermissionClientQt.h \
    $$PWD/qt/WebCoreSupport/NotificationPresenterClientQt.h \
    $$PWD/qt/WebCoreSupport/PageClientQt.h \
    $$PWD/qt/WebCoreSupport/PopupMenuQt.h \
    $$PWD/qt/WebCoreSupport/QtPlatformPlugin.h \
    $$PWD/qt/WebCoreSupport/RenderThemeQStyle.h \
    $$PWD/qt/WebCoreSupport/ScrollbarThemeQStyle.h \
    $$PWD/qt/WebCoreSupport/SearchPopupMenuQt.h \
    $$PWD/qt/WebCoreSupport/TextCheckerClientQt.h \
    $$PWD/qt/WebCoreSupport/PlatformStrategiesQt.h \
    $$PWD/qt/WebCoreSupport/WebEventConversion.h

haveQt(5): contains(QT_CONFIG,accessibility) {
    SOURCES += $$PWD/qt/Api/qwebviewaccessible.cpp
    HEADERS += $$PWD/qt/Api/qwebviewaccessible_p.h 
}

INCLUDEPATH += \
    $$PWD/qt/Api \
    $$PWD/qt/WebCoreSupport

contains(DEFINES, ENABLE_VIDEO=1) {
    !contains(DEFINES, WTF_USE_QTKIT=1):!contains(DEFINES, WTF_USE_GSTREAMER=1):contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/qt/WebCoreSupport/FullScreenVideoWidget.h
        SOURCES += $$PWD/qt/WebCoreSupport/FullScreenVideoWidget.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) | contains(DEFINES, WTF_USE_GSTREAMER=1) | contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/qt/WebCoreSupport/FullScreenVideoQt.h
        SOURCES += $$PWD/qt/WebCoreSupport/FullScreenVideoQt.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) {
        INCLUDEPATH += \
            $$PWD/../WebCore/platform/qt/ \
            $$PWD/../WebCore/platform/mac/ \
            $$PWD/../../WebKitLibraries/

        DEFINES += NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES

        contains(CONFIG, "x86") {
            DEFINES+=NS_BUILD_32_LIKE_64
        }

        HEADERS += \
            $$PWD/qt/WebCoreSupport/WebSystemInterface.h \
            $$PWD/qt/WebCoreSupport/QTKitFullScreenVideoHandler.h

        OBJECTIVE_SOURCES += \
            $$PWD/qt/WebCoreSupport/WebSystemInterface.mm \
            $$PWD/qt/WebCoreSupport/QTKitFullScreenVideoHandler.mm
    }
}

contains(DEFINES, ENABLE_ICONDATABASE=1) {
    HEADERS += \
        $$PWD/../WebCore/loader/icon/IconDatabaseClient.h \
        $$PWD/qt/WebCoreSupport/IconDatabaseClientQt.h

    SOURCES += \
        $$PWD/qt/WebCoreSupport/IconDatabaseClientQt.cpp
}

contains(DEFINES, ENABLE_GEOLOCATION=1) {
     HEADERS += \
        $$PWD/qt/WebCoreSupport/GeolocationClientQt.h
     SOURCES += \
        $$PWD/qt/WebCoreSupport/GeolocationClientQt.cpp
}


