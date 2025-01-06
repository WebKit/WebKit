add_custom_target(WebKitTestRunner-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WebKitTestRunner_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe
)
list(APPEND WebKitTestRunner_DEPENDENCIES WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    libwpe/EventSenderProxyClientLibWPE.cpp
    libwpe/EventSenderProxyLibWPE.cpp
    libwpe/PlatformWebViewClientLibWPE.cpp
    libwpe/PlatformWebViewLibWPE.cpp

    skia/TestInvocationSkia.cpp

    wpe/EventSenderProxyClientWPE.cpp
    wpe/PlatformWebViewClientWPE.cpp
    wpe/TestControllerWPE.cpp
    wpe/UIScriptControllerWPE.cpp
    wpe/main.cpp
)

list(APPEND WebKitTestRunner_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${WebKitTestRunner_DIR}/libwpe
    ${WebKitTestRunner_DIR}/wpe
    $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_DIR}
)

list(APPEND WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBXKBCOMMON_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_PRIVATE_LIBRARIES
    WebKit::WPEToolingBackends
    ${GLIB_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
)

if (ENABLE_WPE_PLATFORM)
    list(APPEND WebKitTestRunner_LIBRARIES
        WPEPlatform-${WPE_API_VERSION}
    )
endif ()

list(APPEND TestRunnerInjectedBundle_LIBRARIES
    ${GLIB_LIBRARIES}
)

list(APPEND TestRunnerInjectedBundle_SOURCES
    InjectedBundle/atspi/AccessibilityControllerAtspi.cpp
    InjectedBundle/atspi/AccessibilityNotificationHandler.cpp
    InjectedBundle/atspi/AccessibilityUIElementAtspi.cpp

    InjectedBundle/glib/ActivateFontsGlib.cpp

    InjectedBundle/wpe/InjectedBundleWPE.cpp
    InjectedBundle/wpe/TestRunnerWPE.cpp
)

list(APPEND TestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${GLIB_INCLUDE_DIRS}
    ${WebKitTestRunner_DIR}/InjectedBundle/atspi
    ${WebKitTestRunner_DIR}/InjectedBundle/glib
    ${WebKitTestRunner_DIR}/InjectedBundle/wpe
)

add_definitions(
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
