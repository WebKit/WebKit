include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesWPE.txt"

    "platform/SourcesGLib.txt"
    "platform/SourcesSoup.txt"
)

# Allow building ANGLE on platforms that don't provide X11 headers.
list(APPEND ANGLE_PLATFORM_DEFINITIONS "USE_WPE")

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector"
    ${JAVASCRIPTCORE_DIR}
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/domjit"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${JAVASCRIPTCORE_DIR}/yarr"
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/wpe"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/icu"
    ${WTF_DIR}
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/wpe/RenderThemeWPE.cpp)

list(APPEND WebCore_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${ICU_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
    ${WPE_LIBRARIES}
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)
