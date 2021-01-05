list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WTF_DIR}"
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
