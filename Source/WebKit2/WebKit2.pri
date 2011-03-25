# WebKit2 - Qt4 build info

SOURCE_DIR = $$replace(PWD, /WebKit2, "")

# Use a config-specific target to prevent parallel builds file clashes on Mac
mac: CONFIG(debug, debug|release): WEBKIT2_TARGET = webkit2d
else: WEBKIT2_TARGET = webkit2

# Output in WebKit2/<config>
CONFIG(debug, debug|release) : WEBKIT2_DESTDIR = debug
else: WEBKIT2_DESTDIR = release

CONFIG(standalone_package) {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = $$PWD/generated
} else {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = generated
}

WEBKIT2_INCLUDEPATH = \
    $$SOURCE_DIR/WebKit2 \
    $$SOURCE_DIR/WebKit2/Platform \
    $$SOURCE_DIR/WebKit2/Platform/CoreIPC \
    $$SOURCE_DIR/WebKit2/Platform/qt \
    $$SOURCE_DIR/WebKit2/Shared \
    $$SOURCE_DIR/WebKit2/Shared/API/c \
    $$SOURCE_DIR/WebKit2/Shared/CoreIPCSupport \
    $$SOURCE_DIR/WebKit2/Shared/Plugins \
    $$SOURCE_DIR/WebKit2/Shared/Plugins/Netscape \
    $$SOURCE_DIR/WebKit2/Shared/qt \
    $$SOURCE_DIR/WebKit2/UIProcess \
    $$SOURCE_DIR/WebKit2/UIProcess/API/C \
    $$SOURCE_DIR/WebKit2/UIProcess/API/cpp \
    $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/qt \
    $$SOURCE_DIR/WebKit2/UIProcess/API/qt \
    $$SOURCE_DIR/WebKit2/UIProcess/Authentication \
    $$SOURCE_DIR/WebKit2/UIProcess/Downloads \
    $$SOURCE_DIR/WebKit2/UIProcess/Launcher \
    $$SOURCE_DIR/WebKit2/UIProcess/Plugins \
    $$SOURCE_DIR/WebKit2/UIProcess/qt \
    $$SOURCE_DIR/WebKit2/WebProcess \
    $$SOURCE_DIR/WebKit2/WebProcess/ApplicationCache \
    $$SOURCE_DIR/WebKit2/WebProcess/Authentication \
    $$SOURCE_DIR/WebKit2/WebProcess/Cookies \
    $$SOURCE_DIR/WebKit2/WebProcess/Cookies/qt \
    $$SOURCE_DIR/WebKit2/WebProcess/Downloads \
    $$SOURCE_DIR/WebKit2/WebProcess/Downloads/qt \
    $$SOURCE_DIR/WebKit2/WebProcess/FullScreen \
    $$SOURCE_DIR/WebKit2/WebProcess/Geolocation \
    $$SOURCE_DIR/WebKit2/WebProcess/IconDatabase \
    $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle \
    $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/DOM \
    $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c \
    $$SOURCE_DIR/WebKit2/WebProcess/KeyValueStorage \
    $$SOURCE_DIR/WebKit2/WebProcess/MediaCache \
    $$SOURCE_DIR/WebKit2/WebProcess/Plugins \
    $$SOURCE_DIR/WebKit2/WebProcess/Plugins/Netscape \
    $$SOURCE_DIR/WebKit2/WebProcess/ResourceCache \
    $$SOURCE_DIR/WebKit2/WebProcess/WebCoreSupport \
    $$SOURCE_DIR/WebKit2/WebProcess/WebCoreSupport/qt \
    $$SOURCE_DIR/WebKit2/WebProcess/WebPage \
    $$SOURCE_DIR/WebKit2/WebProcess/qt \
    $$SOURCE_DIR/WebKit2/PluginProcess

# On Symbian PREPEND_INCLUDEPATH is the best way to make sure that WebKit headers
# are included before platform headers.

symbian {
    PREPEND_INCLUDEPATH = $$WEBKIT2_INCLUDEPATH $$WEBKIT2_GENERATED_SOURCES_DIR $$PREPEND_INCLUDEPATH
} else {
    INCLUDEPATH = $$WEBKIT2_INCLUDEPATH $$WEBKIT2_GENERATED_SOURCES_DIR $$INCLUDEPATH
}

defineTest(prependWebKit2Lib) {
    pathToWebKit2Output = $$ARGS/$$WEBKIT2_DESTDIR

    win32-msvc*|wince* {
        LIBS = -l$$WEBKIT2_TARGET $$LIBS
        LIBS = -L$$pathToWebKit2Output $$LIBS
        POST_TARGETDEPS += $${pathToWebKit2Output}$${QMAKE_DIR_SEP}$${WEBKIT2_TARGET}.lib
    } else:symbian {
        LIBS = -l$${WEBKIT2_TARGET}.lib $$LIBS
        QMAKE_LIBDIR += $$pathToWebKit2Output
        POST_TARGETDEPS += $${pathToWebKit2Output}$${QMAKE_DIR_SEP}$${WEBKIT2_TARGET}.lib
    } else {
        QMAKE_LIBDIR = $$pathToWebKit2Output $$QMAKE_LIBDIR
        LIBS = -l$$WEBKIT2_TARGET $$LIBS
        POST_TARGETDEPS += $${pathToWebKit2Output}$${QMAKE_DIR_SEP}lib$${WEBKIT2_TARGET}.a
    }

    # The following line is to prevent qmake from adding webkit2 to libQtWebKit's prl dependencies.
    CONFIG -= explicitlib

    export(QMAKE_LIBDIR)
    export(POST_TARGETDEPS)
    export(CONFIG)
    export(LIBS)

    return(true)
}
