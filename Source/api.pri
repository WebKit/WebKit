# -------------------------------------------------------------------
# Project file for the QtWebKit C++ APIs
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = QtWebKit

DESTDIR = $${ROOT_BUILD_DIR}/lib

runSyncQt() # Generate forwarding headers for the QtWebKit API

load(features)

include(WebKit/WebKit.pri)

CONFIG += wtf

!v8:CONFIG += javascriptcore

CONFIG += webcore

!no_webkit2 {
    CONFIG += webkit2
    QT += declarative

    # Ensure that changes to the WebKit2 API will trigger a qmake of this
    # file, which in turn runs syncqt to update the forwarding headers.
    QMAKE_INTERNAL_INCLUDED_FILES *= WebKit2/Target.pri
}

QT += network
haveQt(5): QT += widgets printsupport

win32*:!win32-msvc* {
    # Make sure OpenGL libs are after the webcore lib so MinGW can resolve symbols
    contains(DEFINES, ENABLE_WEBGL=1)|contains(CONFIG, texmap): LIBS += $$QMAKE_LIBS_OPENGL
}

!static: DEFINES += QT_MAKEDLL

SOURCES += \
    $$PWD/WebKit/qt/WebCoreSupport/QtFallbackWebPopup.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/QtWebComboBox.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/ChromeClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/ContextMenuClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/DragClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/DumpRenderTreeSupportQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/EditorClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/UndoStepQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/FrameLoaderClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/FrameNetworkingContextQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/GeolocationPermissionClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/InitWebCoreQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/InspectorClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/InspectorServerQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/NotificationPresenterClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/PageClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/PopupMenuQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/QtPlatformPlugin.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/SearchPopupMenuQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/TextCheckerClientQt.cpp \
    $$PWD/WebKit/qt/WebCoreSupport/PlatformStrategiesQt.cpp

HEADERS += \
    $$PWD/WebKit/qt/WebCoreSupport/InitWebCoreQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/InspectorServerQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/QtFallbackWebPopup.h \
    $$PWD/WebKit/qt/WebCoreSupport/QtWebComboBox.h \
    $$PWD/WebKit/qt/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/FrameNetworkingContextQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/GeolocationPermissionClientQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/NotificationPresenterClientQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/PageClientQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/QtPlatformPlugin.h \
    $$PWD/WebKit/qt/WebCoreSupport/PopupMenuQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/SearchPopupMenuQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/TextCheckerClientQt.h \
    $$PWD/WebKit/qt/WebCoreSupport/PlatformStrategiesQt.h

INCLUDEPATH += \
    $$PWD/WebKit/qt/Api \
    $$PWD/WebKit/qt/WebCoreSupport

contains(DEFINES, ENABLE_VIDEO=1) {
    !contains(DEFINES, WTF_USE_QTKIT=1):!contains(DEFINES, WTF_USE_GSTREAMER=1):contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/WebKit/qt/WebCoreSupport/FullScreenVideoWidget.h
        SOURCES += $$PWD/WebKit/qt/WebCoreSupport/FullScreenVideoWidget.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) | contains(DEFINES, WTF_USE_GSTREAMER=1) | contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/WebKit/qt/WebCoreSupport/FullScreenVideoQt.h
        SOURCES += $$PWD/WebKit/qt/WebCoreSupport/FullScreenVideoQt.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) {
        INCLUDEPATH += \
            $$PWD/WebCore/platform/qt/ \
            $$PWD/WebCore/platform/mac/ \
            $$PWD/../WebKitLibraries/

        DEFINES += NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES

        contains(CONFIG, "x86") {
            DEFINES+=NS_BUILD_32_LIKE_64
        }

        HEADERS += \
            $$PWD/WebKit/qt/WebCoreSupport/WebSystemInterface.h \
            $$PWD/WebKit/qt/WebCoreSupport/QTKitFullScreenVideoHandler.h

        OBJECTIVE_SOURCES += \
            $$PWD/WebKit/qt/WebCoreSupport/WebSystemInterface.mm \
            $$PWD/WebKit/qt/WebCoreSupport/QTKitFullScreenVideoHandler.mm

        LIBS += -framework Security -framework IOKit

        # We can know the Mac OS version by using the Darwin major version
        DARWIN_VERSION = $$split(QMAKE_HOST.version, ".")
        DARWIN_MAJOR_VERSION = $$first(DARWIN_VERSION)
        equals(DARWIN_MAJOR_VERSION, "11") {
            LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceLion.a
        } else:equals(DARWIN_MAJOR_VERSION, "10") {
            LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceSnowLeopard.a
        } else:equals(DARWIN_MAJOR_VERSION, "9") {
            LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceLeopard.a
        }
    }
}

contains(DEFINES, ENABLE_ICONDATABASE=1) {
    HEADERS += \
        $$PWD/WebCore/loader/icon/IconDatabaseClient.h \
        $$PWD/WebKit/qt/WebCoreSupport/IconDatabaseClientQt.h

    SOURCES += \
        $$PWD/WebKit/qt/WebCoreSupport/IconDatabaseClientQt.cpp
}

contains(DEFINES, ENABLE_DEVICE_ORIENTATION=1) || contains(DEFINES, ENABLE_ORIENTATION_EVENTS=1) {
    haveQt(5): QT += sensors
}

contains(DEFINES, ENABLE_DEVICE_ORIENTATION=1) {
    HEADERS += \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceMotionClientQt.h \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceMotionProviderQt.h \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceOrientationClientQt.h \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceOrientationProviderQt.h

    SOURCES += \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceMotionClientQt.cpp \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceMotionProviderQt.cpp \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceOrientationClientQt.cpp \
        $$PWD/WebKit/qt/WebCoreSupport/DeviceOrientationProviderQt.cpp
}

contains(DEFINES, ENABLE_GEOLOCATION=1) {
     haveQt(5): QT += location

     HEADERS += \
        $$PWD/WebKit/qt/WebCoreSupport/GeolocationClientQt.h
     SOURCES += \
        $$PWD/WebKit/qt/WebCoreSupport/GeolocationClientQt.cpp
}

contains(CONFIG, texmap) {
    DEFINES += WTF_USE_TEXTURE_MAPPER=1
}


# ------------- Install rules -------------

modulefile.files = $$QT.webkit.modulefile
mkspecs = $$[QMAKE_MKSPECS]
mkspecs = $$split(mkspecs, :)
modulefile.path = $$last(mkspecs)/modules
INSTALLS += modulefile

# Syncqt has already run at this point, so we can use headers.pri
# as a basis for our install-rules
HEADERS_PRI = $${ROOT_BUILD_DIR}/include/$$TARGET/headers.pri
!include($$HEADERS_PRI): error(Failed to resolve install headers)

headers.files = $$SYNCQT.HEADER_FILES $$SYNCQT.HEADER_CLASSES
!isEmpty(INSTALL_HEADERS): headers.path = $$INSTALL_HEADERS/$${TARGET}
else: headers.path = $$[QT_INSTALL_HEADERS]/$${TARGET}
INSTALLS += headers

!isEmpty(INSTALL_LIBS): target.path = $$INSTALL_LIBS
else: target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target

unix {
    CONFIG += create_pc create_prl
    QMAKE_PKGCONFIG_LIBDIR = $$target.path
    QMAKE_PKGCONFIG_INCDIR = $$headers.path
    QMAKE_PKGCONFIG_DESTDIR = pkgconfig
    lib_replace.match = $$re_escape($$DESTDIR)
    lib_replace.replace = $$[QT_INSTALL_LIBS]
    QMAKE_PKGCONFIG_INSTALL_REPLACE += lib_replace
}

mac {
    !static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
        !build_pass {
            message("Building QtWebKit as a framework, as that's how Qt was built. You can")
            message("override this by passing CONFIG+=webkit_no_framework to build-webkit.")
        } else {
            isEmpty(QT_SOURCE_TREE):debug_and_release:TARGET = $$qtLibraryTarget($$TARGET)
        }

        CONFIG += lib_bundle qt_no_framework_direct_includes qt_framework

        # For debug_and_release configs, only copy headers in release
        !debug_and_release|if(build_pass:CONFIG(release, debug|release)) {
            FRAMEWORK_HEADERS.version = Versions
            FRAMEWORK_HEADERS.files = $${headers.files}
            FRAMEWORK_HEADERS.path = Headers
            QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
        }
    }

    QMAKE_LFLAGS_SONAME = "$${QMAKE_LFLAGS_SONAME}$${DESTDIR}$${QMAKE_DIR_SEP}"
}

plugin_backend_xlib: PKGCONFIG += x11

linux-g++*: {
    PRE_TARGETDEPS += $$PWD/qtwebkit-export.map
    QMAKE_LFLAGS += -Wl,--version-script=$$PWD/qtwebkit-export.map

    !no_webkit2: {
        # -lrt is required for shm_open and shm_unlink.
        LIBS += -lrt
    }
}
