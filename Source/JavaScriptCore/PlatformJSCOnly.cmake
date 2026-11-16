if (ENABLE_JSC_GLIB_API)
    include(GLib.cmake)
    set(JavaScriptCore_OUTPUT_NAME javascriptcoreglib-${PROJECT_VERSION})
    configure_file(javascriptcoreglib.pc.in ${JavaScriptCore_PKGCONFIG_FILE} @ONLY)

    if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
        add_custom_target(JavaScriptCore-build-revision
            ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${JavaScriptCore_PKGCONFIG_FILE}
            DEPENDS ${JavaScriptCore_PKGCONFIG_FILE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
        list(APPEND JavaScriptCore_DEPENDENCIES
            JavaScriptCore-build-revision
        )
    endif ()

    install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoreglib-${PROJECT_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
    )

    install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
        DESTINATION "${JavaScriptCore_HEADER_INSTALL_DIR}/jsc"
    )
endif ()

if (ENABLE_REMOTE_INSPECTOR)
    if (USE_GLIB)
        include(inspector/remote/GLib.cmake)
    elseif (APPLE)
        include(inspector/remote/Cocoa.cmake)
    else ()
        include(inspector/remote/Socket.cmake)
    endif ()
endif ()

if (USE_LIBBACKTRACE)
    list(APPEND WTF_LIBRARIES
        LIBBACKTRACE::LIBBACKTRACE
    )
endif ()

if (WIN32)
    # WasmDebugServer uses winsock functions
    list(APPEND JavaScriptCore_LIBRARIES
        ws2_32
    )
endif ()
