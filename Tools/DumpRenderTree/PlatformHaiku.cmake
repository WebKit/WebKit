list(APPEND DumpRenderTree_SOURCES
    haiku/DumpRenderTree.cpp
    haiku/EditingCallbacks.cpp
    haiku/EventSender.cpp
    haiku/GCControllerHaiku.cpp
    haiku/JSStringUtils.cpp
    haiku/PixelDumpSupportHaiku.cpp
    haiku/TestRunnerHaiku.cpp
    haiku/UIScriptControllerHaiku.cpp
    haiku/WorkQueueItemHaiku.cpp
)

list(APPEND DumpRenderTree_LIBRARIES
	WebKitLegacy
	WebCore
	stdc++fs
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
	${WEBKITLEGACY_DIR}/haiku/API
	${WEBKITLEGACY_DIR}/haiku
	${WEBKITLEGACY_DIR}/haiku/WebCoreSupport
    ${WEBCORE_DIR}/css/parser
    ${WEBCORE_DIR}/platform/graphics/haiku
    ${WEBCORE_DIR}/platform/network/haiku
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/style
    ${TOOLS_DIR}/DumpRenderTree/haiku
)

list(APPEND DumpRenderTree_LOCAL_INCLUDE_DIRECTORIES
	${FORWARDING_HEADERS_DIR}/WebCore
)

foreach(inc ${DumpRenderTree_LOCAL_INCLUDE_DIRECTORIES})
    ADD_DEFINITIONS(-iquote ${inc})
endforeach(inc)

if (ENABLE_ACCESSIBILITY)
    list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
        ${TOOLS_DIR}/DumpRenderTree/haiku
    )
endif ()

# FIXME: DOWNLOADED_FONTS_DIR should not hardcode the directory
# structure. See <https://bugs.webkit.org/show_bug.cgi?id=81475>.
add_definitions(-DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
    -DDOWNLOADED_FONTS_DIR="${CMAKE_SOURCE_DIR}/WebKitBuild/Dependencies/Source/webkitgtk-test-fonts-0.0.3"
)
