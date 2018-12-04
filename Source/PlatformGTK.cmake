include(GtkDoc)
include(WebKitDist)

add_subdirectory(${WEBCORE_DIR}/platform/gtk/po)

# This allows exposing a 'gir' target which builds all GObject introspection files.
if (ENABLE_INTROSPECTION)
    add_custom_target(gir ALL DEPENDS ${GObjectIntrospectionTargets})
endif ()

list(APPEND DocumentationDependencies
    WebKit
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/gtk/docs/webkit2gtk-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/gtk/docs/webkit2gtk-${WEBKITGTK_API_VERSION}-sections.txt"
)

if (ENABLE_GTKDOC)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkit2gtk-${WEBKITGTK_API_VERSION}/html/webkit2gtk-${WEBKITGTK_API_VERSION}
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkitdomgtk-${WEBKITGTK_API_VERSION}/html/webkitdomgtk-${WEBKITGTK_API_VERSION}
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/jsc-glib-${WEBKITGTK_API_VERSION}/html/jsc-glib-${WEBKITGTK_API_VERSION}
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html"
    )
endif ()

ADD_GTKDOC_GENERATOR("docs-build.stamp" "--gtk")
if (ENABLE_GTKDOC)
    add_custom_target(gtkdoc ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")
elseif (NOT ENABLED_COMPILER_SANITIZERS AND NOT CMAKE_CROSSCOMPILING AND NOT APPLE)
    add_custom_target(gtkdoc DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")

    # Add a default build step which check that documentation does not have any warnings
    # or errors. This is useful to prevent breaking documentation inadvertently during
    # the course of development.
    if (DEVELOPER_MODE)
        ADD_GTKDOC_GENERATOR("docs-build-no-html.stamp" "--gtk;--skip-html")
        add_custom_target(gtkdoc-no-html ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build-no-html.stamp")
    endif ()
endif ()

add_custom_target(check
    COMMAND ${TOOLS_DIR}/Scripts/run-gtk-tests
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM
)

if (DEVELOPER_MODE)
    add_custom_target(Documentation DEPENDS gtkdoc)
    WEBKIT_DECLARE_DIST_TARGETS(GTK webkitgtk ${TOOLS_DIR}/gtk/manifest.txt.in)
endif ()
