list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"
)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/inspector/remote/glib"
)

configure_file(javascriptcoregtk.pc.in ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc @ONLY)
configure_file(JavaScriptCore.gir.in ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir @ONLY)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
    COMMAND ${INTROSPECTION_COMPILER} ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir -o ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
)

ADD_TYPELIB(${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib)

install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)

install(FILES API/JavaScript.h
              API/JSBase.h
              API/JSContextRef.h
              API/JSObjectRef.h
              API/JSStringRef.h
              API/JSTypedArray.h
              API/JSValueRef.h
              API/WebKitAvailability.h
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/JavaScriptCore"
)

if (ENABLE_INTROSPECTION)
    install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
            DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
    )
    install(FILES ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.typelib
            DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
    )
endif ()

add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
add_definitions(-DLIBDIR="${LIB_INSTALL_DIR}")

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)
list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

# Linking WebKit properly is extremely tricky. We need to build both a static library
# and a shared library for JSC. See https://bugs.webkit.org/show_bug.cgi?id=179914.
set(JavaScriptCoreGTK_LIBRARIES
    JavaScriptCore${DEBUG_SUFFIX}
)
ADD_WHOLE_ARCHIVE_TO_LIBRARIES(JavaScriptCoreGTK_LIBRARIES)

add_library(JavaScriptCoreGTK SHARED "${CMAKE_BINARY_DIR}/cmakeconfig.h")
target_link_libraries(JavaScriptCoreGTK ${JavaScriptCoreGTK_LIBRARIES})
set_target_properties(JavaScriptCoreGTK PROPERTIES OUTPUT_NAME javascriptcoregtk-${WEBKITGTK_API_VERSION})

WEBKIT_POPULATE_LIBRARY_VERSION(JAVASCRIPTCORE)
set_target_properties(JavaScriptCoreGTK PROPERTIES VERSION ${JAVASCRIPTCORE_VERSION} SOVERSION ${JAVASCRIPTCORE_VERSION_MAJOR})

if (NOT DEVELOPER_MODE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    WEBKIT_ADD_TARGET_PROPERTIES(JavaScriptCoreGTK LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/javascriptcoregtk-symbols.map")
endif ()

install(TARGETS JavaScriptCoreGTK DESTINATION "${LIB_INSTALL_DIR}")
