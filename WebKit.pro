# -------------------------------------------------------------------
# Root project file, used to load WebKit in Qt Creator and for
# building QtWebKit.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

!webkit_configured {
    CONFIG += production_build
    include(Tools/qmake/configure.pri)
    the_config = $$CONFIG
    the_config -= $$BASE_CONFIG $$find(CONFIG, "^(done_)?config_")
    cache(CONFIG, add, the_config)
    the_defines = $$DEFINES
    the_defines -= $$BASE_DEFINES
    cache(DEFINES, add, the_defines)

    # We inherit the build type from Qt, unless it was specified on the qmake command
    # line. Note that the perl build script defaults to forcing a release build.
    contains(the_config, debug|release) {
        contains(the_config, debug) {
            contains(the_config, release) {
                !debug_and_release:cache(CONFIG, add, $$list(debug_and_release))
            } else {
                release:cache(CONFIG, del, $$list(release))
                debug_and_release:cache(CONFIG, del, $$list(debug_and_release))
            }
        } else { # release
            debug:cache(CONFIG, del, $$list(debug))
            debug_and_release:cache(CONFIG, del, $$list(debug_and_release))
        }
    } else {
        contains(QT_CONFIG, release, debug|release): \
            cache(CONFIG, add, $$list(release))
        else: \
            cache(CONFIG, add, $$list(debug))
        macx:!debug_and_release:cache(CONFIG, add, $$list(debug_and_release))
    }
}

TEMPLATE = subdirs
CONFIG += ordered

WTF.file = Source/WTF/WTF.pro
WTF.makefile = Makefile.WTF
SUBDIRS += WTF

JavaScriptCore.file = Source/JavaScriptCore/JavaScriptCore.pro
JavaScriptCore.makefile = Makefile.JavaScriptCore
SUBDIRS += JavaScriptCore

WebCore.file = Source/WebCore/WebCore.pro
WebCore.makefile = Makefile.WebCore
SUBDIRS += WebCore

!no_webkit1 {
    webkit1.file = Source/WebKit/WebKit1.pro
    webkit1.makefile = Makefile.WebKit1
    SUBDIRS += webkit1
}

!no_webkit2 {
    webkit2.file = Source/WebKit2/WebKit2.pro
    webkit2.makefile = Makefile.WebKit2
    SUBDIRS += webkit2
}

QtWebKit.file = Source/QtWebKit.pro
QtWebKit.makefile = Makefile.QtWebKit
SUBDIRS += QtWebKit

SUBDIRS += Tools
