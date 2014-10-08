set(ForwardingHeadersForWebKitTestRunner_NAME WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/cairo/TestInvocationCairo.cpp

    ${WEBKIT_TESTRUNNER_DIR}/gtk/TestControllerGtk.cpp
    ${WEBKIT_TESTRUNNER_DIR}/gtk/PlatformWebViewGtk.cpp
    ${WEBKIT_TESTRUNNER_DIR}/gtk/EventSenderProxyGtk.cpp

    ${WEBKIT_TESTRUNNER_DIR}/gtk/EventSenderProxyGtk.cpp
    ${WEBKIT_TESTRUNNER_DIR}/gtk/PlatformWebViewGtk.cpp
    ${WEBKIT_TESTRUNNER_DIR}/gtk/TestControllerGtk.cpp
    ${WEBKIT_TESTRUNNER_DIR}/gtk/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_DIR}
    ${WTF_DIR}/wtf/gobject
    ${ATK_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_LIBRARIES
    ${ATK_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${GLIB_LIBRARIES}
    WTF
    WebCorePlatformGTK
)

set(WebKitTestRunnerInjectedBundle_LIBRARIES
    ${ATK_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GTK3_LIBRARIES}
    WebCoreTestSupport
    WebKit2
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityControllerAtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityNotificationHandlerAtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityUIElementAtk.cpp

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/gtk/ActivateFontsGtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/gtk/InjectedBundleGtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/gtk/InjectedBundleUtilities.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/gtk/TestRunnerGtk.cpp
)

add_definitions(
    -DFONTS_CONF_DIR="${TOOLS_DIR}/WebKitTestRunner/gtk/fonts"
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)

file(GLOB_RECURSE WebKitTestRunner_HEADERS
    *.h
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKitTestRunner-forwarding-headers.stamp
    DEPENDS ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl
            ${WebKitTestRunner_SOURCES}
            ${WebKitTestRunner_HEADERS}
            ${WebKitTestRunnerInjectedBundle_SOURCES}
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_TESTRUNNER_DIR} ${FORWARDING_HEADERS_DIR} gtk
    COMMAND touch ${CMAKE_BINARY_DIR}/WebKitTestRunner-forwarding-headers.stamp
)
add_custom_target(WebKitTestRunner-forwarding-headers
    DEPENDS ${CMAKE_BINARY_DIR}/WebKitTestRunner-forwarding-headers.stamp
)
