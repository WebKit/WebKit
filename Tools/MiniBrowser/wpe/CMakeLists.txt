set(MiniBrowser_SOURCES
    main.cpp
)

set(MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DERIVED_SOURCES_WPE_API_DIR}
    ${FORWARDING_HEADERS_WPE_DIR}
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
)

set(MiniBrowser_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(MiniBrowser_PRIVATE_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    WebKit::WPEToolingBackends
    WebKit::WebKit
)

if (USE_ATK)
    list(APPEND MiniBrowser_SYSTEM_INCLUDE_DIRECTORIES
        ${ATK_INCLUDE_DIRS}
    )

    list(APPEND MiniBrowser_PRIVATE_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

if (ENABLE_WPE_PLATFORM)
    list(APPEND MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES
        ${WPEPlatform_DERIVED_SOURCES_DIR}
        ${WEBKIT_DIR}/WPEPlatform
    )

    list(APPEND MiniBrowser_PRIVATE_LIBRARIES
        WPEPlatform-${WPE_API_VERSION}
    )
endif ()

if (NOT USE_GSTREAMER_FULL)
  list(APPEND MiniBrowser_PRIVATE_INCLUDE_DIRECTORIES ${GSTREAMER_INCLUDE_DIRS})
  list(APPEND MiniBrowser_PRIVATE_LIBRARIES ${GSTREAMER_LIBRARIES})
endif ()

if (DEVELOPER_MODE)
    list(APPEND MiniBrowser_PRIVATE_DEFINITIONS WEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()

if (USE_SKIA)
    list(APPEND MiniBrowser_PRIVATE_LIBRARIES Skia)
    if (NOT USE_SYSTEM_MALLOC)
        #
        # Skia uses WTF::fastMalloc only when built inside libWPEWebKit.
        # Add the malloc allocator here to make the linker skil the symbols
        # from the static library which have references to WTF.
        #
        list(APPEND MiniBrowser_SOURCES
            ${THIRDPARTY_DIR}/skia/src/ports/SkMemory_malloc.cpp
        )
    endif ()

    if (NOT DEFINED CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
        check_cxx_compiler_flag(-Wno-unused-parameter CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
    endif ()
    if (CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
        set_property(
            SOURCE ${THIRDPARTY_DIR}/skia/src/ports/SkMemory_malloc.cpp
            APPEND PROPERTY COMPILE_OPTIONS -Wno-unused-parameter
        )
    endif ()
endif ()

WEBKIT_EXECUTABLE_DECLARE(MiniBrowser)
WEBKIT_EXECUTABLE(MiniBrowser)

install(TARGETS MiniBrowser DESTINATION "${LIBEXEC_INSTALL_DIR}")

if (ENABLE_WPE_QT_API)
    if (USE_QT6)
        add_subdirectory(qt6)
    else ()
        add_subdirectory(qt5)
    endif ()
endif ()
