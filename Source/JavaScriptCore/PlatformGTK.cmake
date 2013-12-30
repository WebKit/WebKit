configure_file(javascriptcoregtk.pc.in ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-3.0.pc @ONLY)
configure_file(JavaScriptCore.gir.in ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir @ONLY)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir
    COMMAND g-ir-compiler ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.gir -o ${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib
)

ADD_TYPELIB(${CMAKE_BINARY_DIR}/JavaScriptCore-3.0.typelib)
