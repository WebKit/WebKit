if (ENABLE_API_TESTS)
    if (ENABLE_WEBKIT)
        add_subdirectory(${WEBKIT_DIR}/gtk/tests)
    endif ()
    if (ENABLE_WEBKIT2)
        add_subdirectory(${WEBKIT2_DIR}/UIProcess/API/gtk/tests)
    endif ()
endif ()

set(DocumentationDependencies
    "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitenvironment.xml"
)

if (ENABLE_WEBKIT)
    list(APPEND DocumentationDependencies
        WebKit
        "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitgtk-docs.sgml"
        "${CMAKE_SOURCE_DIR}/Source/WebKit/gtk/docs/webkitgtk-sections.txt"
    )
endif ()

if (ENABLE_WEBKIT2)
    list(APPEND DocumentationDependencies
        WebKit2
        "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-docs.sgml"
        "${CMAKE_SOURCE_DIR}/Source/WebKit2/UIProcess/API/gtk/docs/webkit2gtk-sections.txt"
    )
endif ()

add_custom_command(
    OUTPUT docs-build.stamp
    DEPENDS ${DocumentationDependencies}
    COMMAND CC="${CMAKE_C_COMPILER}" ${CMAKE_SOURCE_DIR}/Tools/gtk/generate-gtkdoc
    COMMAND touch docs-build.stamp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_custom_target(fake-docs-target ALL
    DEPENDS docs-build.stamp
)

if (ENABLE_WEBKIT)
    add_dependencies(fake-docs-target WebKit)
endif ()

if (ENABLE_WEBKIT2)
    add_dependencies(fake-docs-target WebKit2)
endif ()
