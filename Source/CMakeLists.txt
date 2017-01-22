# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
if (NOT USE_SYSTEM_MALLOC)
    add_subdirectory(bmalloc)
endif ()

add_subdirectory(WTF)

add_subdirectory(JavaScriptCore)

if (WIN32 AND ENABLE_GRAPHICS_CONTEXT_3D)
    add_subdirectory(ThirdParty/ANGLE)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(ThirdParty/gtest)
endif ()

if (USE_WOFF2)
    add_subdirectory(ThirdParty/brotli)
    add_subdirectory(ThirdParty/woff2)
endif ()

if (ENABLE_WEBCORE)
    add_subdirectory(WebCore)
endif ()

if (ENABLE_WEBKIT)
    add_subdirectory(WebKit)
endif ()

if (ENABLE_WEBKIT2)
    add_subdirectory(WebKit2)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# -----------------------------------------------------------------------------
# Set compiler flags for all targets
# -----------------------------------------------------------------------------
if (NOT USE_SYSTEM_MALLOC)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(bmalloc ${ADDITIONAL_COMPILER_FLAGS})
endif ()
WEBKIT_SET_EXTRA_COMPILER_FLAGS(WTF ${ADDITIONAL_COMPILER_FLAGS})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(JavaScriptCore ${ADDITIONAL_COMPILER_FLAGS})

if (ENABLE_WEBCORE)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(PAL ${ADDITIONAL_COMPILER_FLAGS})
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCoreTestSupport ${ADDITIONAL_COMPILER_FLAGS})
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCore ${ADDITIONAL_COMPILER_FLAGS})
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCoreDerivedSources ${ADDITIONAL_COMPILER_FLAGS})
endif ()

if (ENABLE_WEBKIT)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebKit ${ADDITIONAL_COMPILER_FLAGS})
endif ()

if (ENABLE_WEBKIT2)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebKit2 ${ADDITIONAL_COMPILER_FLAGS})
endif ()
