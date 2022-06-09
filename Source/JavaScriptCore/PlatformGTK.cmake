include(GLib.cmake)
include(inspector/remote/GLib.cmake)

set(JavaScriptCore_OUTPUT_NAME javascriptcoregtk-${WEBKITGTK_API_VERSION})

configure_file(javascriptcoregtk.pc.in ${JavaScriptCore_PKGCONFIG_FILE} @ONLY)

if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
    add_custom_target(JavaScriptCore-build-revision
        ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${JavaScriptCore_PKGCONFIG_FILE}
        DEPENDS ${JavaScriptCore_PKGCONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
    list(APPEND JavaScriptCore_DEPENDENCIES
        JavaScriptCore-build-revision
    )
endif ()

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

    if (${INTROSPECTION_HAVE_SOURCES_TOP_DIRS})
        set(GIR_SOURCES_TOP_DIRS "--sources-top-dirs=${CMAKE_BINARY_DIR}")
    endif ()

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        DEPENDS JavaScriptCore
        COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations LDFLAGS=
            ${LOADER_LIBRARY_PATH_VAR}="${INTROSPECTION_ADDITIONAL_LIBRARY_PATH}"
            ${INTROSPECTION_SCANNER}
            --quiet
            --warn-all
            --warn-error
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
            ${GIR_SOURCES_TOP_DIRS}
            --c-include="jsc/jsc.h"
            -DJSC_COMPILATION
            -I${JAVASCRIPTCORE_DIR}/API/glib
            -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
            -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
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
    "[jsc-glib-${WEBKITGTK_API_DOC_VERSION}]\n"
    "pkgconfig_file=${JavaScriptCore_PKGCONFIG_FILE}\n"
    "decorator=JSC_API\n"
    "deprecation_guard=JSC_DISABLE_DEPRECATED\n"
    "namespace=jsc\n"
    "cflags=-I${JAVASCRIPTCORE_DIR}/API/glib\n"
    "       -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}\n"
    "       -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}\n"
    "doc_dir=${JAVASCRIPTCORE_DIR}/API/glib/docs\n"
    "source_dirs=${JAVASCRIPTCORE_DIR}/API/glib\n"
    "headers=${JavaScriptCore_INSTALLED_HEADERS}\n"
    "main_sgml_file=jsc-glib-docs.sgml\n"
)
