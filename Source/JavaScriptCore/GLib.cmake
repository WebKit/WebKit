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

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    "${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}"
    "${JAVASCRIPTCORE_DIR}/API/glib"
)

configure_file(API/glib/JSCVersion.h.in ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/JSCVersion.h)

set(JavaScriptCoreGLib_FRAMEWORK_HEADERS
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/JSCVersion.h

    API/glib/JSCAutocleanups.h
    API/glib/JSCClass.h
    API/glib/JSCContext.h
    API/glib/JSCDefines.h
    API/glib/JSCException.h
    API/glib/JSCOptions.h
    API/glib/JSCValue.h
    API/glib/JSCVirtualMachine.h
    API/glib/JSCWeakValue.h
    API/glib/jsc.h
)

WEBKIT_COPY_FILES(JavaScriptCoreGLib_CopyHeaders
    DESTINATION ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}/jsc
    FILES ${JavaScriptCoreGLib_FRAMEWORK_HEADERS}
    FLATTENED
)
list(APPEND JavaScriptCore_DEPENDENCIES JavaScriptCoreGLib_CopyHeaders)

set(JavaScriptCoreGLib_PRIVATE_FRAMEWORK_HEADERS
    API/glib/JSCContextPrivate.h
    API/glib/JSCValuePrivate.h
)
WEBKIT_COPY_FILES(JavaScriptCoreGLib_CopyPrivateHeaders
    DESTINATION ${JavaScriptCoreGLib_PRIVATE_FRAMEWORK_HEADERS_DIR}/jsc
    FILES ${JavaScriptCoreGLib_PRIVATE_FRAMEWORK_HEADERS}
    FLATTENED
)

list(APPEND JavaScriptCore_INTERFACE_INCLUDE_DIRECTORIES
    ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCoreGLib_PRIVATE_FRAMEWORK_HEADERS_DIR}
)
list(APPEND JavaScriptCore_INTERFACE_DEPENDENCIES
    JavaScriptCoreGLib_CopyHeaders
    JavaScriptCoreGLib_CopyPrivateHeaders
)

set(JavaScriptCore_INSTALLED_HEADERS
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/JSCVersion.h
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
