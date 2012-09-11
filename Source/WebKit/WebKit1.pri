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

enable?(VIDEO):use?(QTKIT) {
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

enable?(DEVICE_ORIENTATION)|enable?(ORIENTATION_EVENTS) {
    QT += sensors
}

enable?(GEOLOCATION): QT += location

contains(CONFIG, texmap): DEFINES += WTF_USE_TEXTURE_MAPPER=1

use?(PLUGIN_BACKEND_XLIB): PKGCONFIG += x11

QT += network widgets
have?(QTQUICK): QT += quick
have?(QTPRINTSUPPORT): QT += printsupport

use?(TEXTURE_MAPPER_GL)|enable?(WEBGL) {
    QT *= opengl
    # Make sure OpenGL libs are after the webcore lib so MinGW can resolve symbols
    win32*:!win32-msvc*: LIBS += $$QMAKE_LIBS_OPENGL
}


