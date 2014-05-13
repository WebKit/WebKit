add_definitions(-DWEBKIT_EXEC_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/JavaScriptCore/ForwardingHeaders
    ${CMAKE_SOURCE_DIR}/Source/WebKit/gtk
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_WEBKITGTK_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

add_executable(GtkLauncher
    LauncherInspectorWindow.c
    LauncherInspectorWindow.h
    main.c
)

target_link_libraries(GtkLauncher
    ${JavaScriptCore_LIBRARY_NAME}
    WebKit
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GTK_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

