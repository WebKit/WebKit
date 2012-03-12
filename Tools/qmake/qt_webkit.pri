# These variables define the library version, which is based on the original
# Qt library version. It is not related to the release-version of QtWebKit.
QT.webkit.MAJOR_VERSION = 5
QT.webkit.MINOR_VERSION = 0
QT.webkit.PATCH_VERSION = 0
QT.webkit.VERSION = 5.0.0

QT.webkit.name = QtWebKit
QT.webkit.bins = $$QT_MODULE_BIN_BASE
QT.webkit.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtWebKit
QT.webkit.imports = $$QT_MODULE_IMPORT_BASE
QT.webkit.private_includes = $$QT_MODULE_INCLUDE_BASE/$$QT.webkit.name/$$QT.webkit.VERSION
QT.webkit.sources = $$QT_MODULE_BASE
QT.webkit.libs = $$QT_MODULE_LIB_BASE
QT.webkit.depends = core gui opengl network xmlpatterns script
QT.webkit.module_pri = $$_FILE_

!contains(QT_CONFIG, modular)|contains(QT_ELIGIBLE_MODULES, webkit) {
    QT_CONFIG += webkit
} else {
    warning("Attempted to include $$QT.webkit.name in the build, but it was not enabled in configure.")
}

# This is the old syntax for the WebKit version defines.
# We keep them around in case someone was using them.
QT_WEBKIT_VERSION = $$QT.webkit.VERSION
QT_WEBKIT_MAJOR_VERSION = $$QT.webkit.MAJOR_VERSION
QT_WEBKIT_MINOR_VERSION = $$QT.webkit.MINOR_VERSION
QT_WEBKIT_PATCH_VERSION = $$QT.webkit.PATCH_VERSION

unix:!mac {
    install_prefix = $$find(_FILE_, ^$$[QT_INSTALL_PREFIX])
    module_prefix = $$find(QMAKE_EXTRA_MODULE_FORWARDS, ^$$dirname(QT_MODULE_BIN_BASE))
    isEmpty(install_prefix):!isEmpty(module_prefix) {
        # We are loading the qt_webkit.pri forwarding file from the
        # local webkit build directory, so set up rpath to point to
        # to the local build. This ensures that any of the WebKit tools
        # such as WebKitTestRunner and QtTestBrowser will link to the
        # non-installed QtWebKit library. The same applies to third-
        # party applications that set QMAKE_EXTRA_MODULE_FORWARDS.
        QMAKE_LFLAGS *= "$${QMAKE_LFLAGS_RPATH}$${QT_MODULE_LIB_BASE}"
    }
    unset(install_prefix)
    unset(module_prefix)
}
