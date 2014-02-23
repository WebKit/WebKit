list(APPEND DumpRenderTree_SOURCES
    atk/AccessibilityCallbacksAtk.cpp
    atk/AccessibilityControllerAtk.cpp
    atk/AccessibilityNotificationHandlerAtk.cpp
    atk/AccessibilityUIElementAtk.cpp

    cairo/PixelDumpSupportCairo.cpp

    gtk/AccessibilityControllerGtk.cpp
    gtk/DumpRenderTree.cpp
    gtk/EditingCallbacks.cpp
    gtk/EventSender.cpp
    gtk/GCControllerGtk.cpp
    gtk/PixelDumpSupportGtk.cpp
    gtk/SelfScrollingWebKitWebView.cpp
    gtk/TestRunnerGtk.cpp
    gtk/TextInputController.cpp
    gtk/WorkQueueItemGtk.cpp
)

list(APPEND DumpRenderTree_LIBRARIES
    WebCorePlatformGTK
    ${ATK_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GTK_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_WEBKITGTK_DIR}
    ${WEBKIT_DIR}/gtk
    ${WEBCORE_DIR}/platform/gtk
    ${TOOLS_DIR}/DumpRenderTree/atk
    ${TOOLS_DIR}/DumpRenderTree/cairo
    ${TOOLS_DIR}/DumpRenderTree/gtk
    ${ATK_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

add_definitions(
    -DTEST_PLUGIN_DIR="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    -DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
    -DTOP_LEVEL_DIR="${CMAKE_SOURCE_DIR}"
)
