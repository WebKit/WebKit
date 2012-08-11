SET(WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/intents"
    "${WEBCORE_DIR}/Modules/networkinfo"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/bindings/generic"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/c"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/default"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/shadow"
    "${WEBCORE_DIR}/inspector"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBCORE_DIR}/svg/properties"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${WTF_DIR}"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${CMAKE_BINARY_DIR}"
)

IF (WTF_USE_SOUP)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES "${WEBCORE_DIR}/platform/network/soup")
ENDIF ()

IF (WTF_USE_CURL)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES "${WEBCORE_DIR}/platform/network/curl")
ENDIF ()

SET(WebKit_LIBRARIES
    ${WebCore_LIBRARY_NAME}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

ADD_DEFINITIONS(-DBUILDING_WebKit)
INCLUDE_DIRECTORIES(${WebKit_INCLUDE_DIRECTORIES})
ADD_LIBRARY(${WebKit_LIBRARY_NAME} ${WebKit_LIBRARY_TYPE} ${WebKit_SOURCES})
ADD_DEPENDENCIES(${WebKit_LIBRARY_NAME} ${WebCore_LIBRARY_NAME})
TARGET_LINK_LIBRARIES(${WebKit_LIBRARY_NAME} ${WebKit_LIBRARIES})
SET_TARGET_PROPERTIES(${WebKit_LIBRARY_NAME} PROPERTIES FOLDER "WebKit")
SET_TARGET_PROPERTIES(${WebKit_LIBRARY_NAME} PROPERTIES LINK_INTERFACE_LIBRARIES "")

IF (WebKit_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(${WebKit_LIBRARY_NAME} LINK_FLAGS "${WebKit_LINK_FLAGS}")
ENDIF ()

IF (VERSION_SCRIPT)
    ADD_TARGET_PROPERTIES(${WebKit_LIBRARY_NAME} LINK_FLAGS "${VERSION_SCRIPT}")
ENDIF ()

SET_TARGET_PROPERTIES(${WebKit_LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
INSTALL(TARGETS ${WebKit_LIBRARY_NAME} DESTINATION "${LIB_INSTALL_DIR}")
