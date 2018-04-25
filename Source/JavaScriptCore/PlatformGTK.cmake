include(GLib.cmake)

set(JavaScriptCore_OUTPUT_NAME javascriptcoregtk-${WEBKITGTK_API_VERSION})

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}"
    "${JAVASCRIPTCORE_DIR}/inspector/remote/glib"
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/glib/RemoteInspectorServer.h
    inspector/remote/glib/RemoteInspectorUtils.h
)

configure_file(javascriptcoregtk.pc.in ${JavaScriptCore_PKGCONFIG_FILE} @ONLY)

install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)

install(FILES ${JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/JavaScriptCore"
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/jsc"
)

if (ENABLE_INTROSPECTION)
    install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
            DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
    )
    install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
            DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
    )
endif ()

add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
add_definitions(-DJSC_COMPILATION)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)
list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

if (ENABLE_INTROSPECTION)
    # Add ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} to LD_LIBRARY_PATH or DYLD_LIBRARY_PATH
    if (APPLE)
        set(LOADER_LIBRARY_PATH_VAR "DYLD_LIBRARY_PATH")
        set(PREV_LOADER_LIBRARY_PATH "$ENV{DYLD_LIBRARY_PATH}")
    else ()
        set(LOADER_LIBRARY_PATH_VAR "LD_LIBRARY_PATH")
        set(PREV_LOADER_LIBRARY_PATH "$ENV{LD_LIBRARY_PATH}")
    endif ()
    string(COMPARE EQUAL "${PREV_LOADER_LIBRARY_PATH}" "" ld_library_path_does_not_exist)
    if (ld_library_path_does_not_exist)
        set(INTROSPECTION_ADDITIONAL_LIBRARY_PATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    else ()
        set(INTROSPECTION_ADDITIONAL_LIBRARY_PATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}:${PREV_LOADER_LIBRARY_PATH}")
    endif ()

    # Add required -L flags from ${CMAKE_SHARED_LINKER_FLAGS} for g-ir-scanner
    string(REGEX MATCHALL "-L[^ ]*" INTROSPECTION_ADDITIONAL_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        DEPENDS JavaScriptCore
        COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations LDFLAGS=
            ${LOADER_LIBRARY_PATH_VAR}="${INTROSPECTION_ADDITIONAL_LIBRARY_PATH}"
            ${INTROSPECTION_SCANNER}
            --quiet
            --warn-all
            --symbol-prefix=jsc
            --identifier-prefix=JSC
            --namespace=JavaScriptCore
            --nsversion=${WEBKITGTK_API_VERSION}
            --include=GObject-2.0
            --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
            -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            ${INTROSPECTION_ADDITIONAL_LINKER_FLAGS}
            --no-libtool
            --pkg=gobject-2.0
            --pkg-export=javascriptcoregtk-${WEBKITGTK_API_VERSION}
            --output=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
            --c-include="jsc/jsc.h"
            -DJSC_COMPILATION
            -I${CMAKE_SOURCE_DIR}/Source
            -I${JAVASCRIPTCORE_DIR}
            -I${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}
            -I${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib
            ${JavaScriptCore_INSTALLED_HEADERS}
            ${JAVASCRIPTCORE_DIR}/API/glib/*.cpp
    )

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
        DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        COMMAND ${INTROSPECTION_COMPILER} --includedir=${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir -o ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
        VERBATIM
    )

    ADD_TYPELIB(${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib)
endif ()

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-jsc-glib.cfg
    "[jsc-glib-${WEBKITGTK_API_VERSION}]\n"
    "pkgconfig_file=${JavaScriptCore_PKGCONFIG_FILE}\n"
    "decorator=JSC_API\n"
    "deprecation_guard=JSC_DISABLE_DEPRECATED\n"
    "namespace=jsc\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${JAVASCRIPTCORE_DIR}/API/glib\n"
    "       -I${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib\n"
    "doc_dir=${JAVASCRIPTCORE_DIR}/API/glib/docs\n"
    "source_dirs=${JAVASCRIPTCORE_DIR}/API/glib\n"
    "headers=${JavaScriptCore_INSTALLED_HEADERS}\n"
    "main_sgml_file=jsc-glib-docs.sgml\n"
)
