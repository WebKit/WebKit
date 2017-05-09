find_package(LibGBM REQUIRED)
find_package(WPEBackend-mesa REQUIRED)

add_custom_target(WebKitTestRunner-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_TESTRUNNER_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe
)

set(ForwardingHeadersForWebKitTestRunner_NAME WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/cairo/TestInvocationCairo.cpp

    ${WEBKIT_TESTRUNNER_DIR}/wpe/EventSenderProxyWPE.cpp
    ${WEBKIT_TESTRUNNER_DIR}/wpe/HeadlessViewBackend.cpp
    ${WEBKIT_TESTRUNNER_DIR}/wpe/PlatformWebViewWPE.cpp
    ${WEBKIT_TESTRUNNER_DIR}/wpe/TestControllerWPE.cpp
    ${WEBKIT_TESTRUNNER_DIR}/wpe/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle/wpe
    ${FORWARDING_HEADERS_DIR}
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBGBM_INCLUDE_DIRS}
    ${WPE_MESA_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBGBM_LIBRARIES}
    ${WPE_MESA_LIBRARIES}
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/wpe/AccessibilityControllerWPE.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/wpe/AccessibilityUIElementWPE.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/wpe/ActivateFontsWPE.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/wpe/InjectedBundleWPE.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/wpe/TestRunnerWPE.cpp
)

add_definitions(
    -DFONTS_CONF_DIR="${TOOLS_DIR}/WebKitTestRunner/gtk/fonts"
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
