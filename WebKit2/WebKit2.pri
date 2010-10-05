# WebKit2 - Qt4 build info

# Use a config-specific target to prevent parallel builds file clashes on Mac
mac: CONFIG(debug, debug|release): WEBKIT2_TARGET = webkit2d
else: WEBKIT2_TARGET = webkit2

# Output in WebKit2/<config>
CONFIG(debug, debug|release) : WEBKIT2_DESTDIR = debug
else: WEBKIT2_DESTDIR = release

defineTest(addWebKit2Lib) {
    pathToWebKit2Output = $$ARGS/$$WEBKIT2_DESTDIR

    QMAKE_LIBDIR += $$pathToWebKit2Output

    # Make symbols visible
    QMAKE_LFLAGS += -Wl,-whole-archive -l$$WEBKIT2_TARGET -Wl,-no-whole-archive

    POST_TARGETDEPS += $${pathToWebKit2Output}$${QMAKE_DIR_SEP}lib$${WEBKIT2_TARGET}.a

    # The following line is to prevent qmake from adding webkit2 to libQtWebKit's prl dependencies.
    CONFIG -= explicitlib

    export(QMAKE_LIBDIR)
    export(QMAKE_LFLAGS)
    export(POST_TARGETDEPS)
    export(CONFIG)

    return(true)
}
