include(WebKitDist)

add_subdirectory(${WEBCORE_DIR}/platform/gtk/po)

add_custom_target(check
    COMMAND ${TOOLS_DIR}/Scripts/run-gtk-tests
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM
)

if (DEVELOPER_MODE)
    WEBKIT_DECLARE_DIST_TARGETS(GTK webkitgtk ${TOOLS_DIR}/gtk/manifest.txt.in)
endif ()
