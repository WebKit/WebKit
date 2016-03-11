list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_IMF_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
    "${WTF_DIR}"
    "${WTF_DIR}/wtf/efl"
)
add_definitions(-DSTATICALLY_LINKED_WITH_WTF)

install(FILES API/JavaScript.h
              API/JSBase.h
              API/JSContextRef.h
              API/JSObjectRef.h
              API/JSStringRef.h
              API/JSValueRef.h
              API/JSTypedArray.h
              API/WebKitAvailability.h
        DESTINATION "${HEADER_INSTALL_DIR}/JavaScriptCore"
)
