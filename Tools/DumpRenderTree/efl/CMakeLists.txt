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
    ${TOOLS_DIR}/DumpRenderTree/efl/TextInputController.cpp
    ${TOOLS_DIR}/DumpRenderTree/efl/WorkQueueItemEfl.cpp
)

SET(ImageDiff_SOURCES
    ${TOOLS_DIR}/DumpRenderTree/efl/ImageDiff.cpp
)

SET(DumpRenderTree_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCoreTestSupport_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit_LIBRARY_NAME}
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${Glib_LIBRARIES}
    ${LIBSOUP24_LIBRARIES}
)

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
    ${WEBCORE_DIR}/inspector
    ${WEBCORE_DIR}/page
    ${WEBCORE_DIR}/page/animation
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/animation
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/graphics/cairo
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/platform/network/soup
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
    ${JAVASCRIPTCORE_DIR}/bytecode
    ${JAVASCRIPTCORE_DIR}/dfg
    ${JAVASCRIPTCORE_DIR}/heap
    ${JAVASCRIPTCORE_DIR}/interpreter
    ${JAVASCRIPTCORE_DIR}/jit
    ${JAVASCRIPTCORE_DIR}/llint
    ${JAVASCRIPTCORE_DIR}/runtime
    ${JAVASCRIPTCORE_DIR}/ForwardingHeaders
    ${TOOLS_DIR}/DumpRenderTree
    ${TOOLS_DIR}/DumpRenderTree/cairo
    ${TOOLS_DIR}/DumpRenderTree/efl
    ${WTF_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_BINARY_DIR}
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${WEBCORE_DIR}/bindings/js
    ${WEBCORE_DIR}/testing/js
    ${CAIRO_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIR}
    ${Glib_INCLUDE_DIRS}
    ${LIBSOUP24_INCLUDE_DIRS}
)

SET(DumpRenderTree_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
    ${LIBSOUP24_LDFLAGS}
)

# FIXME: DOWNLOADED_FONTS_DIR should not hardcode the directory
# structure. See <https://bugs.webkit.org/show_bug.cgi?id=81475>.
ADD_DEFINITIONS(-DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
                -DDOWNLOADED_FONTS_DIR="${CMAKE_SOURCE_DIR}/WebKitBuild/Dependencies/Source/webkitgtk-test-fonts-0.0.3"
                -DDATA_DIR="${THEME_BINARY_DIR}")

INCLUDE_DIRECTORIES(${DumpRenderTree_INCLUDE_DIRECTORIES})

ADD_EXECUTABLE(bin/DumpRenderTree ${DumpRenderTree_SOURCES})
TARGET_LINK_LIBRARIES(bin/DumpRenderTree ${DumpRenderTree_LIBRARIES})
ADD_TARGET_PROPERTIES(bin/DumpRenderTree LINK_FLAGS "${DumpRenderTree_LINK_FLAGS}")
SET_TARGET_PROPERTIES(bin/DumpRenderTree PROPERTIES FOLDER "Tools")
SET_TARGET_PROPERTIES(bin/DumpRenderTree PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

ADD_EXECUTABLE(bin/ImageDiff ${ImageDiff_SOURCES})
TARGET_LINK_LIBRARIES(bin/ImageDiff ${DumpRenderTree_LIBRARIES})
ADD_TARGET_PROPERTIES(bin/ImageDiff LINK_FLAGS "${DumpRenderTree_LINK_FLAGS}")
SET_TARGET_PROPERTIES(bin/ImageDiff PROPERTIES FOLDER "Tools")
SET_TARGET_PROPERTIES(bin/ImageDiff PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
