list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)
list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${WTF_DIR}
)

set(WPE_INSTALLED_JAVASCRIPTCORE_HEADERS
    API/JSBase.h
    API/JSContextRef.h
    API/JSObjectRef.h
    API/JSStringRef.h
    API/JSTypedArray.h
    API/JSValueRef.h
    API/JavaScript.h
    API/WebKitAvailability.h
)

install(FILES ${WPE_INSTALLED_JAVASCRIPTCORE_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE/JavaScriptCore"
    COMPONENT "Development"
)
