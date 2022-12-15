add_custom_target(WebKitTestRunner-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WebKitTestRunner_DIR} --include-path ${TOOLS_DIR}/TestRunnerShared --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
)
list(APPEND WebKitTestRunner_DEPENDENCIES WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    gtk/EventSenderProxyGtk.cpp
    gtk/PlatformWebViewGtk.cpp
    gtk/TestControllerGtk.cpp
    gtk/UIScriptControllerGtk.cpp
    gtk/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_DIR}
)

list(APPEND WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_LIBRARIES
    ${GLIB_LIBRARIES}
    Cairo::Cairo
    GTK::GTK
)

list(APPEND WebKitTestRunnerInjectedBundle_LIBRARIES
    ${GLIB_LIBRARIES}
    Fontconfig::Fontconfig
    GTK::GTK
)

list(APPEND WebKitTestRunner_FRAMEWORKS
    WebKit
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    InjectedBundle/atspi/AccessibilityControllerAtspi.cpp
    InjectedBundle/atspi/AccessibilityNotificationHandler.cpp
    InjectedBundle/atspi/AccessibilityUIElementAtspi.cpp

    InjectedBundle/gtk/ActivateFontsGtk.cpp
    InjectedBundle/gtk/InjectedBundleGtk.cpp
    InjectedBundle/gtk/InjectedBundleUtilities.cpp
    InjectedBundle/gtk/TestRunnerGtk.cpp
)

list(APPEND WebKitTestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${WebKitTestRunner_DIR}/InjectedBundle/atspi
    ${WebKitTestRunner_DIR}/InjectedBundle/gtk
)

add_definitions(
    -DFONTS_CONF_DIR="${TOOLS_DIR}/WebKitTestRunner/gtk/fonts"
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
