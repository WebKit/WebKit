# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
ADD_SUBDIRECTORY(JavaScriptCore)

IF (ENABLE_WEBCORE)
    ADD_SUBDIRECTORY(WebCore)
ENDIF ()

IF (ENABLE_WEBKIT)
    ADD_SUBDIRECTORY(WebKit)
ENDIF ()

IF (ENABLE_WEBKIT2)
    ADD_SUBDIRECTORY(WebKit2)
ENDIF ()

# -----------------------------------------------------------------------------
# Set compiler flags for all targets
# -----------------------------------------------------------------------------
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WTF_LIBRARY_NAME})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${JavaScriptCore_LIBRARY_NAME})

IF (ENABLE_WEBCORE)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebCore_LIBRARY_NAME})
ENDIF ()

IF (ENABLE_WEBKIT)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebKit_LIBRARY_NAME})
ENDIF ()

IF (ENABLE_WEBKIT2)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebKit2_LIBRARY_NAME})
ENDIF ()

# -----------------------------------------------------------------------------
# Create the configuration file
# -----------------------------------------------------------------------------
CONFIGURE_FILE(cmakeconfig.h.cmake ${CMAKE_BINARY_DIR}/cmakeconfig.h @ONLY)
