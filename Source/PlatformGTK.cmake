add_subdirectory(${WEBCORE_DIR}/platform/gtk/po)

# This allows exposing a 'gir' target which builds all GObject introspection files.
add_custom_target(gir ALL DEPENDS ${GObjectIntrospectionTargets})

set(DocumentationDependencies
    "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitenvironment.xml"
)

if (ENABLE_WEBKIT)
    list(APPEND DocumentationDependencies
        WebKit
        "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitgtk-docs.sgml"
        "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitgtk-sections.txt"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkitgtk/html/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/gtk-doc/html/webkitgtk
    )
endif ()

if (ENABLE_WEBKIT2)
    list(APPEND DocumentationDependencies
        WebKit2
        "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-docs.sgml"
        "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-sections.txt"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/webkit2gtk/html/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/gtk-doc/html/webkit2gtk
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

add_gtkdoc_generator("docs-build-no-html.stamp" "--skip-html")
add_custom_target(gtkdoc-no-html ALL
    DEPENDS "${CMAKE_BINARY_DIR}/docs-build-no-html.stamp"
)

add_gtkdoc_generator("docs-build.stamp" "")
add_custom_target(gtkdoc
    DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp"
)

add_custom_target(check
    COMMAND ${TOOLS_DIR}/Scripts/run-gtk-tests
    COMMAND ${TOOLS_DIR}/gtk/check-for-webkitdom-api-breaks
)

if (ENABLE_WEBKIT AND ENABLE_WEBKIT2)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
        DEPENDS ${TOOLS_DIR}/gtk/make-dist.py
        DEPENDS ${TOOLS_DIR}/gtk/manifest.txt
        DEPENDS WebKit
        DEPENDS WebKit2
        DEPENDS gtkdoc
        COMMAND ${TOOLS_DIR}/gtk/make-dist.py
                --source-dir=${CMAKE_SOURCE_DIR}
                --build-dir=${CMAKE_BINARY_DIR}
                --tarball-root=/webkitgtk-${PROJECT_VERSION}
                -o ${CMAKE_BINARY_DIR}/webkitgtk-${PROJECT_VERSION}.tar
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
endif ()
