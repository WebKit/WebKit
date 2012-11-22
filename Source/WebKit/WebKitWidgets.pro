# -------------------------------------------------------------------
# Target file for the WebKitWidgets static library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = WebKitWidgets

include(WebKitWidgets.pri)

WEBKIT += wtf javascriptcore webcore webkit1
QT += gui

CONFIG += staticlib

SOURCES += \
    $$PWD/qt/Api/qgraphicswebview.cpp \
    $$PWD/qt/Api/qwebframe.cpp \
    $$PWD/qt/Api/qwebpage.cpp \
    $$PWD/qt/Api/qwebview.cpp \
    $$PWD/qt/Api/qwebplugindatabase.cpp \
    $$PWD/qt/Api/qwebinspector.cpp \
    $$PWD/qt/Api/qwebkitversion.cpp \
    $$PWD/qt/WebCoreSupport/QtFallbackWebPopup.cpp \
    $$PWD/qt/WebCoreSupport/QtWebComboBox.cpp \
    $$PWD/qt/WebCoreSupport/QWebUndoCommand.cpp \
    $$PWD/qt/WebCoreSupport/DefaultFullScreenVideoHandler.cpp \
    $$PWD/qt/WebCoreSupport/InitWebKitQt.cpp \
    $$PWD/qt/WebCoreSupport/InspectorClientWebPage.cpp \
    $$PWD/qt/WebCoreSupport/PageClientQt.cpp \
    $$PWD/qt/WebCoreSupport/QStyleFacadeImp.cpp \
    $$PWD/qt/WebCoreSupport/QGraphicsWidgetPluginImpl.cpp \
    $$PWD/qt/WebCoreSupport/QWidgetPluginImpl.cpp

HEADERS += \
    $$PWD/qt/Api/qgraphicswebview.h \
    $$PWD/qt/Api/qwebframe.h \
    $$PWD/qt/Api/qwebframe_p.h \
    $$PWD/qt/Api/qwebkitglobal.h \
    $$PWD/qt/Api/qwebkitplatformplugin.h \
    $$PWD/qt/Api/qwebpage.h \
    $$PWD/qt/Api/qwebpage_p.h \
    $$PWD/qt/Api/qwebview.h \
    $$PWD/qt/Api/qwebinspector.h \
    $$PWD/qt/Api/qwebkitversion.h \
    $$PWD/qt/Api/qwebplugindatabase_p.h \
    $$PWD/qt/WebCoreSupport/InitWebKitQt.h \
    $$PWD/qt/WebCoreSupport/InspectorClientWebPage.h \
    $$PWD/qt/WebCoreSupport/DefaultFullScreenVideoHandler.h \
    $$PWD/qt/WebCoreSupport/QtFallbackWebPopup.h \
    $$PWD/qt/WebCoreSupport/QtWebComboBox.h \
    $$PWD/qt/WebCoreSupport/QWebUndoCommand.h \
    $$PWD/qt/WebCoreSupport/PageClientQt.h \
    $$PWD/qt/WebCoreSupport/QGraphicsWidgetPluginImpl.h \
    $$PWD/qt/WebCoreSupport/QStyleFacadeImp.h \
    $$PWD/qt/WebCoreSupport/QWidgetPluginImpl.h

contains(QT_CONFIG, accessibility) {
    SOURCES += $$PWD/qt/Api/qwebviewaccessible.cpp
    HEADERS += $$PWD/qt/Api/qwebviewaccessible_p.h 
}

INCLUDEPATH += \
    $$PWD/qt/Api \
    $$PWD/qt/WebCoreSupport

enable?(VIDEO) {
    !use?(QTKIT):!use?(GSTREAMER):use?(QT_MULTIMEDIA) {
        HEADERS += $$PWD/qt/WebCoreSupport/FullScreenVideoWidget.h
        SOURCES += $$PWD/qt/WebCoreSupport/FullScreenVideoWidget.cpp
    }
}

use?(3D_GRAPHICS): WEBKIT += angle
