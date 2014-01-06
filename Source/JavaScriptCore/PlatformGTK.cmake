configure_file(javascriptcoregtk.pc.in ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-3.0.pc @ONLY)
configure_file(JavaScriptCore.gir.in ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir @ONLY)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir
    COMMAND ${INTROSPECTION_COMPILER} ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir -o ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib
)

ADD_TYPELIB(${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib)

install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-3.0.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)

install(FILES API/JavaScript.h
              API/JSBase.h
              API/JSContextRef.h
              API/JSObjectRef.h
              API/JSStringRef.h
              API/JSValueRef.h
              API/WebKitAvailability.h
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/JavaScriptCore"
)

install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir
        DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
)
install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib
        DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
)
