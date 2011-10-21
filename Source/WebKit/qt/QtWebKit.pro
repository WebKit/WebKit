# QtWebKit - qmake build info
CONFIG += building-libs
CONFIG += depend_includepath

TARGET = QtWebKit
TEMPLATE = lib

DEFINES += BUILDING_WEBKIT

webkit2: QT += declarative

RESOURCES += \
    $$PWD/../../WebCore/WebCore.qrc

CONFIG(debug, debug|release) : CONFIG_DIR = debug
else: CONFIG_DIR = release

SOURCE_DIR = $$replace(PWD, /WebKit/qt, "")

include($$PWD/Api/headers.pri)
include($$SOURCE_DIR/WebKit.pri)
include($$SOURCE_DIR/JavaScriptCore/JavaScriptCore.pri)
webkit2 {
    include($$SOURCE_DIR/WebKit2/WebKit2.pri)
    include($$SOURCE_DIR/WebKit2/WebKit2API.pri)
}
include($$SOURCE_DIR/WebCore/WebCore.pri)

v8 {
    linux-* {
        QMAKE_LIBDIR += $${V8_LIB_DIR}
        LIBS = -lv8 $$LIBS
    }
}

!v8:prependJavaScriptCoreLib(../../JavaScriptCore)
prependWebCoreLib(../../WebCore)
webkit2:prependWebKit2Lib(../../WebKit2)

# This is needed for syncqt when it parses the dependencies on module's main pro file so
# the generated includes are containing the dependencies.
# It used to be in WebCore.pro but now that this is the main pro file it has to be here.
QT += network
qt5: QT += widgets printsupport

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..

contains(QT_CONFIG, embedded):CONFIG += embedded

win32*:!win32-msvc* {
    # Make sure OpenGL libs are after the webcore lib so MinGW can resolve symbols
    contains(DEFINES, ENABLE_WEBGL=1)|contains(CONFIG, texmap): LIBS += $$QMAKE_LIBS_OPENGL
}

include_webinspector: RESOURCES += $$SOURCE_DIR/WebCore/inspector/front-end/WebKit.qrc $$WC_GENERATED_SOURCES_DIR/InspectorBackendStub.qrc

# Extract sources to build from the generator definitions
defineTest(addExtraCompiler) {
    isEqual($${1}.wkAddOutputToSources, false): return(true)

    outputRule = $$eval($${1}.output)
    input = $$eval($${1}.input)
    input = $$eval($$input)

    for(file,input) {
        base = $$basename(file)
        base ~= s/\\..+//
        newfile=$$replace(outputRule,\\$\\{QMAKE_FILE_BASE\\},$$base)
        SOURCES += $$newfile
    }
    SOURCES += $$eval($${1}.wkExtraSources)
    export(SOURCES)

    return(true)
}

include($$SOURCE_DIR/WebCore/CodeGenerators.pri)

CONFIG(release) {
    contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
    unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
}


DESTDIR = $$OUTPUT_DIR/lib

moduleFile=$$PWD/qt_webkit_version.pri
isEmpty(QT_BUILD_TREE):include($$moduleFile)
VERSION = $${QT_WEBKIT_MAJOR_VERSION}.$${QT_WEBKIT_MINOR_VERSION}.$${QT_WEBKIT_PATCH_VERSION}

!static: DEFINES += QT_MAKEDLL

SOURCES += \
    $$PWD/Api/qwebframe.cpp \
    $$PWD/Api/qgraphicswebview.cpp \
    $$PWD/Api/qwebpage.cpp \
    $$PWD/Api/qwebview.cpp \
    $$PWD/Api/qwebelement.cpp \
    $$PWD/Api/qwebhistory.cpp \
    $$PWD/Api/qwebsettings.cpp \
    $$PWD/Api/qwebhistoryinterface.cpp \
    $$PWD/Api/qwebplugindatabase.cpp \
    $$PWD/Api/qwebpluginfactory.cpp \
    $$PWD/Api/qwebsecurityorigin.cpp \
    $$PWD/Api/qwebscriptworld.cpp \
    $$PWD/Api/qwebdatabase.cpp \
    $$PWD/Api/qwebinspector.cpp \
    $$PWD/Api/qwebkitversion.cpp \
    \
    $$PWD/WebCoreSupport/QtFallbackWebPopup.cpp \
    $$PWD/WebCoreSupport/QtWebComboBox.cpp \
    $$PWD/WebCoreSupport/ChromeClientQt.cpp \
    $$PWD/WebCoreSupport/ContextMenuClientQt.cpp \
    $$PWD/WebCoreSupport/DragClientQt.cpp \
    $$PWD/WebCoreSupport/DumpRenderTreeSupportQt.cpp \
    $$PWD/WebCoreSupport/EditorClientQt.cpp \
    $$PWD/WebCoreSupport/EditCommandQt.cpp \
    $$PWD/WebCoreSupport/FrameLoaderClientQt.cpp \
    $$PWD/WebCoreSupport/FrameNetworkingContextQt.cpp \
    $$PWD/WebCoreSupport/GeolocationPermissionClientQt.cpp \
    $$PWD/WebCoreSupport/InspectorClientQt.cpp \
    $$PWD/WebCoreSupport/InspectorServerQt.cpp \
    $$PWD/WebCoreSupport/NotificationPresenterClientQt.cpp \
    $$PWD/WebCoreSupport/PageClientQt.cpp \
    $$PWD/WebCoreSupport/PopupMenuQt.cpp \
    $$PWD/WebCoreSupport/QtPlatformPlugin.cpp \
    $$PWD/WebCoreSupport/SearchPopupMenuQt.cpp \
    $$PWD/WebCoreSupport/TextCheckerClientQt.cpp \
    $$PWD/WebCoreSupport/WebPlatformStrategies.cpp

HEADERS += \
    $$WEBKIT_API_HEADERS \
    $$PWD/Api/qwebplugindatabase_p.h \
    \
    $$PWD/WebCoreSupport/InspectorServerQt.h \
    $$PWD/WebCoreSupport/QtFallbackWebPopup.h \
    $$PWD/WebCoreSupport/QtWebComboBox.h \
    $$PWD/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/WebCoreSupport/FrameNetworkingContextQt.h \
    $$PWD/WebCoreSupport/GeolocationPermissionClientQt.h \
    $$PWD/WebCoreSupport/NotificationPresenterClientQt.h \
    $$PWD/WebCoreSupport/PageClientQt.h \
    $$PWD/WebCoreSupport/QtPlatformPlugin.h \
    $$PWD/WebCoreSupport/PopupMenuQt.h \
    $$PWD/WebCoreSupport/SearchPopupMenuQt.h \
    $$PWD/WebCoreSupport/TextCheckerClientQt.h \
    $$PWD/WebCoreSupport/WebPlatformStrategies.h

webkit2 {
    HEADERS += $$WEBKIT2_API_HEADERS
    SOURCES += $$WEBKIT2_API_SOURCES
}

contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
    unix {
        maemo5 {
            HEADERS += $$PWD/WebCoreSupport/QtMaemoWebPopup.h
            SOURCES += $$PWD/WebCoreSupport/QtMaemoWebPopup.cpp
        }
    }
}

contains(DEFINES, ENABLE_VIDEO=1) {
    !contains(DEFINES, WTF_USE_QTKIT=1):!contains(DEFINES, WTF_USE_GSTREAMER=1):contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/WebCoreSupport/FullScreenVideoWidget.h
        SOURCES += $$PWD/WebCoreSupport/FullScreenVideoWidget.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) | contains(DEFINES, WTF_USE_GSTREAMER=1) | contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += $$PWD/WebCoreSupport/FullScreenVideoQt.h
        SOURCES += $$PWD/WebCoreSupport/FullScreenVideoQt.cpp
    }

    contains(DEFINES, WTF_USE_QTKIT=1) {
        INCLUDEPATH += $$SOURCE_DIR/WebCore/platform/qt/ \
                       $$SOURCE_DIR/WebCore/platform/mac/ \
                       $$SOURCE_DIR/../WebKitLibraries/

        DEFINES+=NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
        contains(CONFIG, "x86") {
            DEFINES+=NS_BUILD_32_LIKE_64
        }

        HEADERS += $$PWD/WebCoreSupport/WebSystemInterface.h \
                   $$PWD/WebCoreSupport/QTKitFullScreenVideoHandler.h

        OBJECTIVE_SOURCES += $$PWD/WebCoreSupport/WebSystemInterface.mm \
                   $$PWD/WebCoreSupport/QTKitFullScreenVideoHandler.mm

        LIBS+= -framework Security -framework IOKit
        # We can know the Mac OS version by using the Darwin major version
        DARWIN_VERSION = $$split(QMAKE_HOST.version, ".")
        DARWIN_MAJOR_VERSION = $$first(DARWIN_VERSION)
        equals(DARWIN_MAJOR_VERSION, "9") | contains(QMAKE_MAC_SDK, "/Developer/SDKs/MacOSX10.5.sdk") {
            LIBS += $$SOURCE_DIR/../WebKitLibraries/libWebKitSystemInterfaceLeopard.a
        } else: equals(DARWIN_MAJOR_VERSION, "10") | contains(QMAKE_MAC_SDK, "/Developer/SDKs/MacOSX10.6.sdk") {
            LIBS += $$SOURCE_DIR/../WebKitLibraries/libWebKitSystemInterfaceSnowLeopard.a
        } else: equals(DARWIN_MAJOR_VERSION, "11") | contains(QMAKE_MAC_SDK, "/Developer/SDKs/MacOSX10.7.sdk") {
            LIBS += $$SOURCE_DIR/../WebKitLibraries/libWebKitSystemInterfaceLion.a
        }
    }
}

contains(DEFINES, ENABLE_ICONDATABASE=1) {
    HEADERS += \
        $$SOURCE_DIR/WebCore/loader/icon/IconDatabaseClient.h \
        $$PWD/WebCoreSupport/IconDatabaseClientQt.h

    SOURCES += \
        $$PWD/WebCoreSupport/IconDatabaseClientQt.cpp
}

contains(DEFINES, ENABLE_DEVICE_ORIENTATION=1) {
    HEADERS += \
        $$PWD/WebCoreSupport/DeviceMotionClientQt.h \
        $$PWD/WebCoreSupport/DeviceMotionProviderQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationClientQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationClientMockQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationProviderQt.h

    SOURCES += \
        $$PWD/WebCoreSupport/DeviceMotionClientQt.cpp \
        $$PWD/WebCoreSupport/DeviceMotionProviderQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationClientQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationClientMockQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationProviderQt.cpp
}

contains(DEFINES, ENABLE_GEOLOCATION=1) {
     qt5: QT += location

     HEADERS += \
        $$PWD/WebCoreSupport/GeolocationClientQt.h
     SOURCES += \
        $$PWD/WebCoreSupport/GeolocationClientQt.cpp
}

contains(CONFIG, texmap) {
    DEFINES += WTF_USE_TEXTURE_MAPPER=1
}

modfile.files = $$moduleFile
modfile.path = $$[QMAKE_MKSPECS]/modules

INSTALLS += modfile

exists($$OUTPUT_DIR/include/QtWebKit/classheaders.pri): include($$OUTPUT_DIR/include/QtWebKit/classheaders.pri)
WEBKIT_INSTALL_HEADERS = $$WEBKIT_API_HEADERS $$WEBKIT_CLASS_HEADERS

headers.files = $$WEBKIT_INSTALL_HEADERS

!isEmpty(INSTALL_HEADERS): headers.path = $$INSTALL_HEADERS/QtWebKit
else: headers.path = $$[QT_INSTALL_HEADERS]/QtWebKit

!isEmpty(INSTALL_LIBS): target.path = $$INSTALL_LIBS
else: target.path = $$[QT_INSTALL_LIBS]

INSTALLS += target headers

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

            CONFIG += build_all
        } else {
            isEmpty(QT_SOURCE_TREE):debug_and_release:TARGET = $$qtLibraryTarget($$TARGET)
        }

        CONFIG += lib_bundle qt_no_framework_direct_includes qt_framework
        FRAMEWORK_HEADERS.version = Versions
        FRAMEWORK_HEADERS.files = $${headers.files}
        FRAMEWORK_HEADERS.path = Headers
        QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
    }

    QMAKE_LFLAGS_SONAME = "$${QMAKE_LFLAGS_SONAME}$${DESTDIR}$${QMAKE_DIR_SEP}"
}

