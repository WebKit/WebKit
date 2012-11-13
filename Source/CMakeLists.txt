# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
ADD_SUBDIRECTORY(WTF)

ADD_SUBDIRECTORY(JavaScriptCore)

IF (ENABLE_API_TESTS)
    ADD_SUBDIRECTORY(cmake/gtest)
ENDIF ()

IF (ENABLE_WEBCORE)
    ADD_SUBDIRECTORY(WebCore)
ENDIF ()

IF (ENABLE_WEBKIT)
    ADD_SUBDIRECTORY(WebKit)
ENDIF ()

IF (ENABLE_WEBKIT2)
    ADD_SUBDIRECTORY(WebKit2)
ENDIF ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# -----------------------------------------------------------------------------
# Set compiler flags for all targets
# -----------------------------------------------------------------------------
IF (${PORT} STREQUAL "Efl")
    SET(ADDITIONAL_FLAGS ENABLE_WERROR)
ENDIF ()

WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WTF_LIBRARY_NAME} ${ADDITIONAL_FLAGS})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${JavaScriptCore_LIBRARY_NAME} ${ADDITIONAL_FLAGS})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebCoreTestSupport_LIBRARY_NAME} ${ADDITIONAL_FLAGS})

IF (ENABLE_WEBCORE)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebCore_LIBRARY_NAME} ${ADDITIONAL_FLAGS})
ENDIF ()

IF (ENABLE_WEBKIT)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebKit_LIBRARY_NAME} ${ADDITIONAL_FLAGS})
ENDIF ()

IF (ENABLE_WEBKIT2)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(${WebKit2_LIBRARY_NAME} ${ADDITIONAL_FLAGS})
ENDIF ()

# -----------------------------------------------------------------------------
# Create the configuration file
# -----------------------------------------------------------------------------
CONFIGURE_FILE(cmakeconfig.h.cmake ${CMAKE_BINARY_DIR}/cmakeconfig.h @ONLY)
