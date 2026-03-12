add_custom_target(WebKitTestRunner-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WebKitTestRunner_DIR} --include-path ${TOOLS_DIR}/TestRunnerShared --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
)
list(APPEND WebKitTestRunner_DEPENDENCIES WebKitTestRunner-forwarding-headers)

list(APPEND WebKitTestRunner_SOURCES
    gtk/EventSenderProxyGtk.cpp
    gtk/PlatformWebViewGtk.cpp
    gtk/TestControllerGtk.cpp
    gtk/UIScriptControllerGtk.cpp
    gtk/main.cpp

    skia/TestInvocationSkia.cpp
)

list(APPEND WebKitTestRunner_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_DIR}
)

list(APPEND WebKitTestRunner_LIBRARIES
    GTK::GTK
)

list(APPEND TestRunnerInjectedBundle_LIBRARIES
    GLib::GLib
)

list(APPEND TestRunnerInjectedBundle_SOURCES
    InjectedBundle/atspi/AccessibilityControllerAtspi.cpp
    InjectedBundle/atspi/AccessibilityNotificationHandler.cpp
    InjectedBundle/atspi/AccessibilityUIElementAtspi.cpp

    InjectedBundle/glib/ActivateFontsGlib.cpp

    InjectedBundle/gtk/InjectedBundleGtk.cpp
    InjectedBundle/gtk/TestRunnerGtk.cpp
)

list(APPEND TestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Source
    ${WebKitTestRunner_DIR}/InjectedBundle/atspi
    ${WebKitTestRunner_DIR}/InjectedBundle/glib
    ${WebKitTestRunner_DIR}/InjectedBundle/gtk
)

add_definitions(
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
