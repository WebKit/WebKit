SET(DumpRenderTree_SOURCES
    ${TOOLS_DIR}/DumpRenderTree/CyclicRedundancyCheck.cpp
    ${TOOLS_DIR}/DumpRenderTree/GCController.cpp
    ${TOOLS_DIR}/DumpRenderTree/LayoutTestController.cpp
    ${TOOLS_DIR}/DumpRenderTree/PixelDumpSupport.cpp
    ${TOOLS_DIR}/DumpRenderTree/WorkQueue.cpp
    ${TOOLS_DIR}/DumpRenderTree/cairo/PixelDumpSupportCairo.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/DumpHistoryItem.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/DumpRenderTree.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/DumpRenderTreeChrome.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/DumpRenderTreeView.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/EventSender.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/FontManagement.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/GCControllerEfl.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/JSStringUtils.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/LayoutTestControllerEfl.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/PixelDumpSupportEfl.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/WorkQueueItemEfl.cpp
)

SET(ImageDiff_SOURCES
    ${TOOLS_DIR}/DumpRenderTree/efl/ImageDiff.cpp
)

SET(DumpRenderTree_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit_LIBRARY_NAME}
    ${Cairo_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

SET(DumpRenderTree_LIBRARIES ${DumpRenderTree_LIBRARIES})
SET(DumpRenderTree_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/efl/ewk"
    ${WEBKIT_DIR}/efl
    ${WEBCORE_DIR}
    ${WEBCORE_DIR}/bridge
    ${WEBCORE_DIR}/bridge/jsc
    ${WEBCORE_DIR}/bindings
    ${WEBCORE_DIR}/dom
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/css
    ${WEBCORE_DIR}/html
    ${WEBCORE_DIR}/page
    ${WEBCORE_DIR}/page/animation
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/animation
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/graphics/cairo
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/plugins
    ${WEBCORE_DIR}/rendering
    ${WEBCORE_DIR}/rendering/style
    ${WEBCORE_DIR}/history
    ${WEBCORE_DIR}/loader
    ${WEBCORE_DIR}/loader/cache
    ${WEBCORE_DIR}/loader/icon
    ${JAVASCRIPTCORE_DIR}
    ${JAVASCRIPTCORE_DIR}/API
    ${JAVASCRIPTCORE_DIR}/assembler
    ${JAVASCRIPTCORE_DIR}/dfg
    ${JAVASCRIPTCORE_DIR}/heap
    ${JAVASCRIPTCORE_DIR}/interpreter
    ${JAVASCRIPTCORE_DIR}/jit
    ${JAVASCRIPTCORE_DIR}/runtime
    ${JAVASCRIPTCORE_DIR}/ForwardingHeaders
    ${JAVASCRIPTCORE_DIR}/wtf
    ${JAVASCRIPTCORE_DIR}/wtf/efl
    ${TOOLS_DIR}/DumpRenderTree
    ${TOOLS_DIR}/DumpRenderTree/cairo
    ${TOOLS_DIR}/DumpRenderTree/efl
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_BINARY_DIR}
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${WEBCORE_DIR}/bindings/js
    ${Cairo_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
)

SET(DumpRenderTree_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
)

IF (ENABLE_GLIB_SUPPORT)
    LIST(APPEND DumpRenderTree_INCLUDE_DIRECTORIES "${JAVASCRIPTCORE_DIR}/wtf/gobject")

    LIST(APPEND DumpRenderTree_LIBRARIES
        ${Gdk_LIBRARIES}
        ${Glib_LIBRARIES}
        ${Gthread_LIBRARIES}
    )
ENDIF ()

IF (WTF_USE_SOUP)
    LIST(APPEND DumpRenderTree_LIBRARIES ${LIBSOUP24_LIBRARIES})
    LIST(APPEND DumpRenderTree_LINK_FLAGS ${LIBSOUP24_LDFLAGS})
    LIST(APPEND DumpRenderTree_INCLUDE_DIRECTORIES ${LIBSOUP24_INCLUDE_DIRS} ${WEBCORE_DIR}/platform/network/soup)
ENDIF ()

IF (WTF_USE_CURL)
    LIST(APPEND DumpRenderTree_LIBRARIES ${CURL_LIBRARIES})
    LIST(APPEND DumpRenderTree_LINK_FLAGS ${CURL_LDFLAGS})
    LIST(APPEND DumpRenderTree_INCLUDE_DIRECTORIES ${WEBCORE_DIR}/platform/network/curl)
ENDIF ()

ADD_DEFINITIONS(-DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
                -DDATA_DIR="${THEME_BINARY_DIR}")

INCLUDE_DIRECTORIES(${DumpRenderTree_INCLUDE_DIRECTORIES})

ADD_EXECUTABLE(Programs/DumpRenderTree ${DumpRenderTree_SOURCES})
TARGET_LINK_LIBRARIES(Programs/DumpRenderTree ${DumpRenderTree_LIBRARIES})
ADD_TARGET_PROPERTIES(Programs/DumpRenderTree LINK_FLAGS "${DumpRenderTree_LINK_FLAGS}")
SET_TARGET_PROPERTIES(Programs/DumpRenderTree PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

ADD_EXECUTABLE(Programs/ImageDiff ${ImageDiff_SOURCES})
TARGET_LINK_LIBRARIES(Programs/ImageDiff ${DumpRenderTree_LIBRARIES})
ADD_TARGET_PROPERTIES(Programs/ImageDiff LINK_FLAGS "${DumpRenderTree_LINK_FLAGS}")
SET_TARGET_PROPERTIES(Programs/ImageDiff PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
