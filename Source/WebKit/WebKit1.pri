# -------------------------------------------------------------------
# This file contains shared rules used both when building WebKit1
# itself, and by targets that use WebKit1.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/WebKit

INCLUDEPATH += \
    $$SOURCE_DIR/qt/Api \
    $$SOURCE_DIR/qt/WebCoreSupport \
    $$ROOT_WEBKIT_DIR/Source/WTF/wtf/qt

contains(DEFINES, ENABLE_VIDEO=1):contains(DEFINES, WTF_USE_QTKIT=1) {
    LIBS += -framework Security -framework IOKit

    # We can know the Mac OS version by using the Darwin major version
    DARWIN_VERSION = $$split(QMAKE_HOST.version, ".")
    DARWIN_MAJOR_VERSION = $$first(DARWIN_VERSION)
    equals(DARWIN_MAJOR_VERSION, "12") {
        LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceMountainLion.a
    } else:equals(DARWIN_MAJOR_VERSION, "11") {
        LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceLion.a
    } else:equals(DARWIN_MAJOR_VERSION, "10") {
        LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceSnowLeopard.a
    } else:equals(DARWIN_MAJOR_VERSION, "9") {
        LIBS += $${ROOT_WEBKIT_DIR}/WebKitLibraries/libWebKitSystemInterfaceLeopard.a
    }
}

contains(DEFINES, ENABLE_DEVICE_ORIENTATION=1)|contains(DEFINES, ENABLE_ORIENTATION_EVENTS=1) {
    haveQt(5) {
        QT += sensors
    } else {
        CONFIG *= mobility
        MOBILITY *= sensors
    }
}

contains(DEFINES, ENABLE_GEOLOCATION=1):haveQt(5): QT += location

contains(CONFIG, texmap): DEFINES += WTF_USE_TEXTURE_MAPPER=1

plugin_backend_xlib: PKGCONFIG += x11

QT += network
haveQt(5): QT += widgets printsupport quick

contains(DEFINES, WTF_USE_TEXTURE_MAPPER_GL=1)|contains(DEFINES, ENABLE_WEBGL=1) {
    QT *= opengl
    # Make sure OpenGL libs are after the webcore lib so MinGW can resolve symbols
    win32*:!win32-msvc*: LIBS += $$QMAKE_LIBS_OPENGL
}


