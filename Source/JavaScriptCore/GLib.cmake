file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_GLIB_DIR}/jsc)

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
    "${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_GLIB_DIR}/jsc"
    "${JAVASCRIPTCORE_DIR}/API/glib"
)

list(APPEND JavaScriptCore_INTERFACE_INCLUDE_DIRECTORIES
    "${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_GLIB_DIR}"
)

set(JavaScriptCore_INSTALLED_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_GLIB_DIR}/jsc/JSCVersion.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCAutocleanups.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCClass.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCContext.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCDefines.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCException.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCOptions.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCValue.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCVirtualMachine.h
    ${JAVASCRIPTCORE_DIR}/API/glib/JSCWeakValue.h
    ${JAVASCRIPTCORE_DIR}/API/glib/jsc.h
)

configure_file(API/glib/JSCVersion.h.in ${DERIVED_SOURCES_JAVASCRIPTCORE_GLIB_DIR}/jsc/JSCVersion.h)

# These symbolic link allows includes like #include <jsc/jsc.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib/jsc
    DEPENDS ${JAVASCRIPTCORE_DIR}/API/glib
    COMMAND ln -n -s -f ${JAVASCRIPTCORE_DIR}/API/glib ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib/jsc
    VERBATIM
)
add_custom_target(JSC-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib/jsc
)
set(JavaScriptCore_EXTRA_DEPENDENCIES
    JSC-fake-api-headers
)
