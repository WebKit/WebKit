LIST(APPEND WebKitTestRunner_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
)

ADD_CUSTOM_TARGET(forwarding-headersEflForWebKitTestRunner
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_TESTRUNNER_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)
SET(ForwardingHeadersForWebKitTestRunner_NAME forwarding-headersEflForWebKitTestRunner)

ADD_CUSTOM_TARGET(forwarding-headersSoupForWebKitTestRunner
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_TESTRUNNER_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)
SET(ForwardingNetworkHeadersForWebKitTestRunner_NAME forwarding-headersSoupForWebKitTestRunner)

LIST(APPEND WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/cairo/TestInvocationCairo.cpp

    ${WEBKIT_TESTRUNNER_DIR}/efl/EventSenderProxyEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/PlatformWebViewEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/TestControllerEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/main.cpp
)

LIST(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${TOOLS_DIR}/DumpRenderTree/efl/
    ${WEBKIT2_DIR}/UIProcess/API/efl
    "${WTF_DIR}/wtf/gobject"

    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

LIST(APPEND WebKitTestRunner_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${WTF_LIBRARY_NAME}
)

LIST(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${TOOLS_DIR}/DumpRenderTree/efl/FontManagement.cpp

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/ActivateFontsEfl.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/InjectedBundleEfl.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/TestRunnerEfl.cpp
)

# FIXME: DOWNLOADED_FONTS_DIR should not hardcode the directory
# structure. See <https://bugs.webkit.org/show_bug.cgi?id=81475>.
ADD_DEFINITIONS(-DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
                -DDOWNLOADED_FONTS_DIR="${CMAKE_SOURCE_DIR}/WebKitBuild/Dependencies/Source/webkitgtk-test-fonts-0.0.3"
                -DTHEME_DIR="${THEME_BINARY_DIR}")
