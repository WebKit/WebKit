add_custom_target(WebKitTestRunner-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WebKitTestRunner_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe
)
list(APPEND WebKitTestRunner_DEPENDENCIES WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    libwpe/EventSenderProxyLibWPE.cpp
    libwpe/PlatformWebViewLibWPE.cpp

    wpe/TestControllerWPE.cpp
    wpe/UIScriptControllerWPE.cpp
    wpe/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_DIR}
)

list(APPEND WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBXKBCOMMON_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_LIBRARIES
    ${GLIB_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
    ${WPEBACKEND_FDO_LIBRARIES}
    Cairo::Cairo
    WebKit::WPEToolingBackends
)

list(APPEND WebKitTestRunnerInjectedBundle_LIBRARIES
    $<TARGET_OBJECTS:JavaScriptCore>
    ${GLIB_LIBRARIES}
    Cairo::Cairo
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    InjectedBundle/atspi/AccessibilityControllerAtspi.cpp
    InjectedBundle/atspi/AccessibilityNotificationHandler.cpp
    InjectedBundle/atspi/AccessibilityUIElementAtspi.cpp

    InjectedBundle/wpe/ActivateFontsWPE.cpp
    InjectedBundle/wpe/InjectedBundleWPE.cpp
    InjectedBundle/wpe/TestRunnerWPE.cpp
)

list(APPEND WebKitTestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${WebKitTestRunner_DIR}/InjectedBundle/atspi
    ${WebKitTestRunner_DIR}/InjectedBundle/wpe
)

add_definitions(
    -DFONTS_CONF_DIR="${TOOLS_DIR}/WebKitTestRunner/gtk/fonts"
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
