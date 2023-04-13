include(GLibMacros)

file(MAKE_DIRECTORY ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR})
file(MAKE_DIRECTORY ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc)

list(APPEND JavaScriptCore_SOURCES
    API/glib/JSAPIWrapperGlobalObject.cpp
    API/glib/JSAPIWrapperObjectGLib.cpp
    API/glib/JSCCallbackFunction.cpp
    API/glib/JSCClass.cpp
    API/glib/JSCContext.cpp
    API/glib/JSCException.cpp
    API/glib/JSCOptions.cpp
    API/glib/JSCValue.cpp
    API/glib/JSCVersion.cpp
    API/glib/JSCVirtualMachine.cpp
    API/glib/JSCWeakValue.cpp
    API/glib/JSCWrapperMap.cpp
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/API/glib"
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}"
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc"
    "${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}"
)

list(APPEND JavaScriptCore_INTERFACE_INCLUDE_DIRECTORIES
    "${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}"
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}"
)

set(JavaScriptCore_INSTALLED_HEADERS
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCOptions.h
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCVersion.h
)

if (NOT ENABLE_2022_GLIB_API)
    list(APPEND JavaScriptCore_INSTALLED_HEADERS
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCAutocleanups.h
    )
endif ()

set(JavaScriptCore_HEADER_TEMPLATES
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCClass.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCContext.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCDefines.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCException.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCValue.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCVirtualMachine.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCWeakValue.h.in
    ${JAVASCRIPTCORE_DIR}/API/glib/jsc.h.in
)

GENERATE_GLIB_API_HEADERS(JavaScriptCore JavaScriptCore_HEADER_TEMPLATES
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc
    JavaScriptCore_INSTALLED_HEADERS
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

configure_file(API/glib/JSCVersion.h.in ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCVersion.h)

# These symbolic link allows includes like #include <jsc/jsc.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}/jsc
    DEPENDS ${JAVASCRIPTCORE_DIR}/API/glib
    COMMAND ln -n -s -f ${JAVASCRIPTCORE_DIR}/API/glib ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}/jsc
    VERBATIM
)
add_custom_target(JSC-fake-api-headers
    DEPENDS ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}/jsc
)
set(JavaScriptCore_EXTRA_DEPENDENCIES
    JSC-fake-api-headers
)
