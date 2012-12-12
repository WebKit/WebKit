list(INSERT WebCore_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}" # For <unicode.h>, which is included from <sys/keycodes.h>.
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)

list(REMOVE_ITEM WebCore_SOURCES
    html/shadow/MediaControlsApple.cpp
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/bindings/cpp"
    "${WEBCORE_DIR}/platform/blackberry/CookieDatabaseBackingStore"
    "${WEBCORE_DIR}/platform/graphics/blackberry/skia"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/opentype/"
    "${WEBCORE_DIR}/platform/graphics/skia"
    "${WEBCORE_DIR}/platform/image-decoders/skia"
    "${WEBCORE_DIR}/platform/image-encoders/skia"
    "${WEBCORE_DIR}/platform/network/blackberry"
    "${WEBCORE_DIR}/platform/network/blackberry/rss"
)

# Skia sources
list(APPEND WebCore_SOURCES
    platform/graphics/skia/FloatPointSkia.cpp
    platform/graphics/skia/FloatRectSkia.cpp
    platform/graphics/skia/FontCustomPlatformData.cpp
    platform/graphics/skia/GradientSkia.cpp
    platform/graphics/skia/GraphicsContext3DSkia.cpp
    platform/graphics/skia/GraphicsContextSkia.cpp
    platform/graphics/skia/HarfbuzzSkia.cpp
    platform/graphics/skia/ImageBufferSkia.cpp
    platform/graphics/skia/ImageSkia.cpp
    platform/graphics/skia/IntPointSkia.cpp
    platform/graphics/skia/IntRectSkia.cpp
    platform/graphics/skia/NativeImageSkia.cpp
    platform/graphics/skia/PathSkia.cpp
    platform/graphics/skia/PatternSkia.cpp
    platform/graphics/skia/PlatformContextSkia.cpp
    platform/graphics/skia/SkiaUtils.cpp
    platform/graphics/skia/TransformationMatrixSkia.cpp
    platform/graphics/chromium/VDMXParser.cpp
    platform/image-decoders/skia/ImageDecoderSkia.cpp
    platform/image-encoders/skia/PNGImageEncoder.cpp
)

# Skia font backend sources
list(APPEND WebCore_SOURCES
    platform/graphics/blackberry/FontCacheBlackberry.cpp
    platform/graphics/blackberry/skia/PlatformBridge.cpp
    platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz.cpp
    platform/graphics/harfbuzz/FontHarfBuzz.cpp
    platform/graphics/harfbuzz/FontPlatformDataHarfBuzz.cpp
    platform/graphics/harfbuzz/HarfBuzzShaper.cpp
    platform/graphics/harfbuzz/HarfBuzzSkia.cpp
    platform/graphics/skia/FontCacheSkia.cpp
    platform/graphics/skia/GlyphPageTreeNodeSkia.cpp
    platform/graphics/skia/SimpleFontDataSkia.cpp
)

# Other sources
list(APPEND WebCore_SOURCES
    bindings/cpp/WebDOMCString.cpp
    bindings/cpp/WebDOMEventTarget.cpp
    bindings/cpp/WebDOMString.cpp
    bindings/cpp/WebExceptionHandler.cpp
    platform/blackberry/CookieDatabaseBackingStore/CookieDatabaseBackingStore.cpp
    platform/blackberry/AuthenticationChallengeManager.cpp
    platform/blackberry/CookieManager.cpp
    platform/blackberry/CookieMap.cpp
    platform/blackberry/CookieParser.cpp
    platform/blackberry/FileSystemBlackBerry.cpp
    platform/blackberry/ParsedCookie.cpp
    platform/graphics/WOFFFileFormat.cpp
    platform/graphics/opentype/OpenTypeSanitizer.cpp
    platform/image-encoders/JPEGImageEncoder.cpp
    platform/image-encoders/skia/JPEGImageEncoder.cpp
    platform/posix/FileSystemPOSIX.cpp
    platform/posix/SharedBufferPOSIX.cpp
    platform/text/TextBreakIteratorICU.cpp
    platform/text/TextCodecICU.cpp
    platform/text/TextEncodingDetectorICU.cpp
    platform/text/blackberry/TextBreakIteratorInternalICUBlackBerry.cpp
)

# Networking sources
list(APPEND WebCore_SOURCES
    platform/network/MIMESniffing.cpp
    platform/network/ProxyServer.cpp
    platform/network/blackberry/AutofillBackingStore.cpp
    platform/network/blackberry/DNSBlackBerry.cpp
    platform/network/blackberry/DeferredData.cpp
    platform/network/blackberry/NetworkJob.cpp
    platform/network/blackberry/NetworkManager.cpp
    platform/network/blackberry/NetworkStateNotifierBlackBerry.cpp
    platform/network/blackberry/ProxyServerBlackBerry.cpp
    platform/network/blackberry/ResourceErrorBlackBerry.cpp
    platform/network/blackberry/ResourceHandleBlackBerry.cpp
    platform/network/blackberry/ResourceRequestBlackBerry.cpp
    platform/network/blackberry/ResourceResponseBlackBerry.cpp
    platform/network/blackberry/SocketStreamHandleBlackBerry.cpp
    platform/network/blackberry/rss/RSSAtomParser.cpp
    platform/network/blackberry/rss/RSS10Parser.cpp
    platform/network/blackberry/rss/RSS20Parser.cpp
    platform/network/blackberry/rss/RSSFilterStream.cpp
    platform/network/blackberry/rss/RSSGenerator.cpp
    platform/network/blackberry/rss/RSSParserBase.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsBlackBerry.css
    ${WEBCORE_DIR}/css/mediaControlsBlackBerryFullscreen.css
    ${WEBCORE_DIR}/css/themeBlackBerry.css
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/bridge/blackberry"
    "${WEBCORE_DIR}/history/blackberry"
    "${WEBCORE_DIR}/page/blackberry"
    "${WEBCORE_DIR}/platform/blackberry"
    "${WEBCORE_DIR}/platform/graphics/blackberry"
    "${WEBCORE_DIR}/platform/image-decoders/bmp"
    "${WEBCORE_DIR}/platform/image-decoders/gif"
    "${WEBCORE_DIR}/platform/image-decoders/ico"
    "${WEBCORE_DIR}/platform/image-decoders/jpeg"
    "${WEBCORE_DIR}/platform/image-decoders/png"
    "${WEBCORE_DIR}/platform/image-encoders"
    "${WEBCORE_DIR}/platform/network/blackberry"
    "${WEBCORE_DIR}/platform/text/blackberry"
    "${WEBKIT_DIR}/blackberry/Api"
    "${WEBKIT_DIR}/blackberry/WebCoreSupport"
    "${WEBKIT_DIR}/blackberry/WebKitSupport"
)

# BlackBerry sources
list(APPEND WebCore_SOURCES
    editing/blackberry/EditorBlackBerry.cpp
    editing/blackberry/SmartReplaceBlackBerry.cpp
    html/shadow/MediaControlsBlackBerry.cpp
    loader/blackberry/CookieJarBlackBerry.cpp
    page/blackberry/AccessibilityObjectBlackBerry.cpp
    page/blackberry/DragControllerBlackBerry.cpp
    page/blackberry/EventHandlerBlackBerry.cpp
    page/blackberry/SettingsBlackBerry.cpp
    platform/blackberry/ClipboardBlackBerry.cpp
    platform/blackberry/ContextMenuBlackBerry.cpp
    platform/blackberry/ContextMenuItemBlackBerry.cpp
    platform/blackberry/CursorBlackBerry.cpp
    platform/blackberry/DragDataBlackBerry.cpp
    platform/blackberry/DragImageBlackBerry.cpp
    platform/blackberry/EventLoopBlackBerry.cpp
    platform/blackberry/KURLBlackBerry.cpp
    platform/blackberry/LocalizedStringsBlackBerry.cpp
    platform/blackberry/LoggingBlackBerry.cpp
    platform/blackberry/MIMETypeRegistryBlackBerry.cpp
    platform/blackberry/PasteboardBlackBerry.cpp
    platform/blackberry/PlatformKeyboardEventBlackBerry.cpp
    platform/blackberry/PlatformMouseEventBlackBerry.cpp
    platform/blackberry/PlatformScreenBlackBerry.cpp
    platform/blackberry/PlatformTouchEventBlackBerry.cpp
    platform/blackberry/PlatformTouchPointBlackBerry.cpp
    platform/blackberry/PopupMenuBlackBerry.cpp
    platform/blackberry/RenderThemeBlackBerry.cpp
    platform/blackberry/RunLoopBlackBerry.cpp
    platform/blackberry/SSLKeyGeneratorBlackBerry.cpp
    platform/blackberry/ScrollbarThemeBlackBerry.cpp
    platform/blackberry/SearchPopupMenuBlackBerry.cpp
    platform/blackberry/SharedTimerBlackBerry.cpp
    platform/blackberry/SoundBlackBerry.cpp
    platform/blackberry/SystemTimeBlackBerry.cpp
    platform/blackberry/TemporaryLinkStubs.cpp
    platform/blackberry/WidgetBlackBerry.cpp
    platform/graphics/blackberry/FloatPointBlackBerry.cpp
    platform/graphics/blackberry/FloatRectBlackBerry.cpp
    platform/graphics/blackberry/FloatSizeBlackBerry.cpp
    platform/graphics/blackberry/IconBlackBerry.cpp
    platform/graphics/blackberry/ImageBlackBerry.cpp
    platform/graphics/blackberry/IntPointBlackBerry.cpp
    platform/graphics/blackberry/IntRectBlackBerry.cpp
    platform/graphics/blackberry/IntSizeBlackBerry.cpp
    platform/graphics/blackberry/MediaPlayerPrivateBlackBerry.cpp
    platform/text/blackberry/StringBlackBerry.cpp
)

# Credential Persistence sources
list(APPEND WebCore_SOURCES
    platform/network/blackberry/CredentialBackingStore.cpp
    platform/network/blackberry/CredentialStorageBlackBerry.cpp
)

# File System support
if (ENABLE_FILE_SYSTEM)
    list(APPEND WebCore_SOURCES
        platform/blackberry/AsyncFileSystemBlackBerry.cpp
    )
endif ()

# Touch sources
list(APPEND WebCore_SOURCES
    dom/Touch.cpp
    dom/TouchEvent.cpp
    dom/TouchList.cpp
)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/blackberry/ScrollAnimatorBlackBerry.cpp
    )
endif ()

list(APPEND WEBDOM_IDL_HEADERS
    bindings/cpp/WebDOMCString.h
    bindings/cpp/WebDOMEventTarget.h
    bindings/cpp/WebDOMObject.h
    bindings/cpp/WebDOMString.h
)

if (ENABLE_REQUEST_ANIMATION_FRAME)
    list(APPEND WebCore_SOURCES
        platform/graphics/blackberry/DisplayRefreshMonitorBlackBerry.cpp
        platform/graphics/DisplayRefreshMonitor.cpp
    )
endif ()

if (ENABLE_WEBGL)
    add_definitions(-DWTF_USE_OPENGL_ES_2=1)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gpu"
        "${WEBCORE_DIR}/platform/graphics/opengl"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/blackberry/DrawingBufferBlackBerry.cpp
        platform/graphics/blackberry/GraphicsContext3DBlackBerry.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
        platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
        platform/graphics/opengl/Extensions3DOpenGLES.cpp
        platform/graphics/gpu/SharedGraphicsContext3D.cpp
    )
endif ()

if (ENABLE_MEDIA_STREAM)
    list(APPEND WebCore_SOURCES
        platform/mediastream/blackberry/MediaStreamCenterBlackBerry.cpp
    )
endif ()

if (ENABLE_NETSCAPE_PLUGIN_API)
    list(APPEND WebCore_SOURCES
        plugins/PluginDatabase.cpp
        plugins/PluginPackage.cpp
        plugins/PluginView.cpp
        plugins/blackberry/NPCallbacksBlackBerry.cpp
        plugins/blackberry/PluginDataBlackBerry.cpp
        plugins/blackberry/PluginPackageBlackBerry.cpp
        plugins/blackberry/PluginViewBlackBerry.cpp
        plugins/blackberry/PluginViewPrivateBlackBerry.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        plugins/PluginDataNone.cpp
        plugins/PluginDatabase.cpp
        plugins/PluginPackage.cpp
        plugins/PluginPackageNone.cpp
        plugins/PluginView.cpp
        plugins/PluginViewNone.cpp
    )
endif ()

# To speed up linking when working on accel comp, you can move this whole chunk
# to Source/WebKit/blackberry/CMakeListsBlackBerry.txt.
# Append to WebKit_SOURCES instead of WebCore_SOURCES.
if (WTF_USE_ACCELERATED_COMPOSITING)
    list(APPEND WebCore_SOURCES
        ${WEBCORE_DIR}/platform/graphics/GraphicsLayer.cpp
        ${WEBCORE_DIR}/platform/graphics/GraphicsLayerUpdater.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/CanvasLayerWebKitThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/EGLImageLayerWebKitThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/EGLImageLayerCompositingThreadClient.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/GraphicsLayerBlackBerry.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerAnimation.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerCompositingThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerFilterRenderer.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerRenderer.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerRendererSurface.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerTile.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerTiler.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/LayerWebKitThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/PluginLayerWebKitThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/Texture.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/TextureCacheCompositingThread.cpp
        ${WEBCORE_DIR}/platform/graphics/blackberry/WebGLLayerWebKitThread.cpp
        ${WEBCORE_DIR}/rendering/RenderLayerBacking.cpp
        ${WEBCORE_DIR}/rendering/RenderLayerCompositor.cpp
    )
endif ()

set(ENV{WEBKITDIR} ${CMAKE_SOURCE_DIR}/Source)
set(ENV{PLATFORMNAME} ${CMAKE_SYSTEM_NAME})
execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE host
)
string(REPLACE "\n" "" host1 "${host}")
set(ENV{COMPUTERNAME} ${host1})

if ($ENV{PUBLIC_BUILD})
    add_definitions(-DPUBLIC_BUILD=$ENV{PUBLIC_BUILD})
endif ()

install(FILES ${WEBDOM_IDL_HEADERS} DESTINATION usr/include/browser/webkit/dom)

# Create DOM C++ code given an IDL input
# We define a new list of feature defines that is prefixed with LANGUAGE_CPP=1 so as to avoid the
# warning "missing whitespace after the macro name" when inlining "LANGUAGE_CPP=1 ${FEATURE_DEFINES}".
set(FEATURE_DEFINES_WEBCORE "LANGUAGE_CPP=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")

# FIXME: We need to add the IDLs for SQL storage and Web Workers. See PR #123484.
set(WebCore_NO_CPP_IDL_FILES
    ${WebCore_SVG_IDL_FILES}
    dom/CustomEvent.idl
    dom/PopStateEvent.idl
    inspector/ScriptProfile.idl
    inspector/ScriptProfileNode.idl
)

list(APPEND WebCore_IDL_FILES
    css/MediaQueryListListener.idl
)

set(WebCore_CPP_IDL_FILES ${WebCore_IDL_FILES})

foreach (_file ${WebCore_NO_CPP_IDL_FILES})
    string(REPLACE "${_file}" "" WebCore_CPP_IDL_FILES "${WebCore_CPP_IDL_FILES}")
endforeach ()

set(WebCore_CPP_IDL_FILES
    dom/EventListener.idl
    "${WebCore_CPP_IDL_FILES}"
)

foreach (_idl ${WebCore_CPP_IDL_FILES})
    set(IDL_FILES_LIST "${IDL_FILES_LIST}${WEBCORE_DIR}/${_idl}\n")
endforeach ()
file(WRITE ${IDL_FILES_TMP} ${IDL_FILES_LIST})

add_custom_command(
    OUTPUT ${SUPPLEMENTAL_DEPENDENCY_FILE}
    DEPENDS ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl ${SCRIPTS_RESOLVE_SUPPLEMENTAL} ${WebCore_CPP_IDL_FILES} ${IDL_ATTRIBUTES_FILE}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl --defines "${FEATURE_DEFINES_JAVASCRIPT}" --idlFilesList ${IDL_FILES_TMP} --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --supplementalDependencyFile ${SUPPLEMENTAL_DEPENDENCY_FILE} --idlAttributesFile ${IDL_ATTRIBUTES_FILE}
    VERBATIM)

GENERATE_BINDINGS(WebCore_SOURCES
    "${WebCore_CPP_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_WEBCORE}"
    ${DERIVED_SOURCES_WEBCORE_DIR} WebDOM CPP
    ${SUPPLEMENTAL_DEPENDENCY_FILE})

# Generate contents for PopupPicker.cpp
set(WebCore_POPUP_CSS_AND_JS
    ${WEBCORE_DIR}/Resources/blackberry/popupControlBlackBerry.css
    ${WEBCORE_DIR}/Resources/blackberry/selectControlBlackBerry.css
    ${WEBCORE_DIR}/Resources/blackberry/selectControlBlackBerry.js
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/PopupPicker.h ${DERIVED_SOURCES_WEBCORE_DIR}/PopupPicker.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/make-file-arrays.py
    DEPENDS ${WebCore_POPUP_CSS_AND_JS}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/make-file-arrays.py --out-h=${DERIVED_SOURCES_WEBCORE_DIR}/PopupPicker.h --out-cpp=${DERIVED_SOURCES_WEBCORE_DIR}/PopupPicker.cpp ${WebCore_POPUP_CSS_AND_JS}
)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/PopupPicker.cpp)
