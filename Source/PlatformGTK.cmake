add_subdirectory(${WEBCORE_DIR}/platform/gtk/po)

# This allows exposing a 'gir' target which builds all GObject introspection files.
if (ENABLE_INTROSPECTION)
    add_custom_target(gir ALL DEPENDS ${GObjectIntrospectionTargets})
endif ()

list(APPEND DocumentationDependencies
    GObjectDOMBindings
    WebKit2
    "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-sections.txt"
)

if (ENABLE_GTKDOC)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkit2gtk/html/
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html/webkit2gtk-${WEBKITGTK_API_VERSION}"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkitdomgtk/html/
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html/webkitdomgtk-${WEBKITGTK_API_VERSION}"
    )
endif ()

macro(ADD_GTKDOC_GENERATOR _stamp_name _extra_args)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/${_stamp_name}"
        DEPENDS ${DocumentationDependencies}
        COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=${CMAKE_C_FLAGS} ${CMAKE_SOURCE_DIR}/Tools/gtk/generate-gtkdoc ${_extra_args}
        COMMAND touch ${_stamp_name}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endmacro()

add_gtkdoc_generator("docs-build.stamp" "")
if (ENABLE_GTKDOC)
    add_custom_target(gtkdoc ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")
else ()
    add_custom_target(gtkdoc DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")

    # Add a default build step which check that documentation does not have any warnings
    # or errors. This is useful to prevent breaking documentation inadvertently during
    # the course of development.
    if (DEVELOPER_MODE)
        add_gtkdoc_generator("docs-build-no-html.stamp" "--skip-html")
        add_custom_target(gtkdoc-no-html ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build-no-html.stamp")
    endif ()
endif ()

add_custom_target(check
    COMMAND ${TOOLS_DIR}/Scripts/run-gtk-tests
    COMMAND ${TOOLS_DIR}/gtk/check-for-webkitdom-api-breaks
)


add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
    DEPENDS ${TOOLS_DIR}/gtk/make-dist.py
    DEPENDS ${TOOLS_DIR}/gtk/manifest.txt
    DEPENDS WebKit2
    DEPENDS gtkdoc
    COMMAND ${TOOLS_DIR}/gtk/make-dist.py
            --source-dir=${CMAKE_SOURCE_DIR}
            --build-dir=${CMAKE_BINARY_DIR}
            --version=${PROJECT_VERSION}
            ${TOOLS_DIR}/gtk/manifest.txt
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar.xz
    DEPENDS ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
    COMMAND xz -f ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
)

add_custom_target(dist
    DEPENDS ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar.xz
)

add_custom_target(distcheck
    DEPENDS ${TOOLS_DIR}/gtk/make-dist.py
    DEPENDS ${TOOLS_DIR}/gtk/manifest.txt
    DEPENDS WebKit2
    DEPENDS gtkdoc
    COMMAND ${TOOLS_DIR}/gtk/make-dist.py
            --check
            --source-dir=${CMAKE_SOURCE_DIR}
            --build-dir=${CMAKE_BINARY_DIR}
            --version=/webkitgtk-${PROJECT_VERSION}
            ${TOOLS_DIR}/gtk/manifest.txt
    COMMAND xz -f ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
)
