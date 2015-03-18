list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${ECORE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
)
add_definitions(-DSTATICALLY_LINKED_WITH_WTF)

install(FILES API/JavaScript.h
              API/JSBase.h
              API/JSContextRef.h
              API/JSObjectRef.h
              API/JSStringRef.h
              API/JSValueRef.h
              API/WebKitAvailability.h
        DESTINATION "${HEADER_INSTALL_DIR}/JavaScriptCore"
)
