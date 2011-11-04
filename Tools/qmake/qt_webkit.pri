QT.webkit.VERSION = 4.10.0
QT.webkit.MAJOR_VERSION = 4
QT.webkit.MINOR_VERSION = 10
QT.webkit.PATCH_VERSION = 0

QT.webkit.name = QtWebKit
QT.webkit.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtWebKit
QT.webkit.sources = $$QT_MODULE_BASE
QT.webkit.libs = $$QT_MODULE_LIB_BASE
QT.webkit.depends = core gui opengl network xmlpatterns script

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
