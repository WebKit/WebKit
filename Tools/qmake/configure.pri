# -------------------------------------------------------------------
# This file is used by build-webkit to compute the various feature
# defines, which are then cached in .qmake.cache.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

!quick_check {
    load(configure)
    QMAKE_CONFIG_TESTS_DIR = $$PWD/config.tests

    CONFIG_TESTS = \
        fontconfig \
        gccdepends \
        glx \
        libpng \
        libjpeg \
        libwebp \
        libXcomposite \
        libXrender \
        libxml2 \
        libxslt \
        libzlib

    for(test, CONFIG_TESTS): qtCompileTest($$test)
}

load(features)
