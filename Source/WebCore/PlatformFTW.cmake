include(platform/Curl.cmake)
include(platform/ImageDecoders.cmake)
# FIXME: Enable
# include(platform/TextureMapper.cmake)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${DirectX_INCLUDE_DIRS}"
    "${WEBKIT_LIBRARIES_DIR}/include"
    "${WEBCORE_DIR}/accessibility/win"
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/network/win"
    "${WEBCORE_DIR}/platform/win")

list(APPEND WebCore_SOURCES
    page/win/FrameWinDirect2D.cpp

    platform/graphics/win/FontCascadeDirect2D.cpp
    platform/graphics/win/FontCustomPlatformData.cpp
    platform/graphics/win/FontPlatformDataDirect2D.cpp
    platform/graphics/win/GlyphPageTreeNodeDirect2D.cpp
    platform/graphics/win/GradientDirect2D.cpp
    platform/graphics/win/GraphicsContextDirect2D.cpp
    platform/graphics/win/GraphicsLayerDirect2D.cpp
    platform/graphics/win/ImageBufferDataDirect2D.cpp
    platform/graphics/win/ImageBufferDirect2D.cpp
    platform/graphics/win/ImageDecoderDirect2D.cpp
    platform/graphics/win/ImageDirect2D.cpp
    platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    platform/graphics/win/NativeImageDirect2D.cpp
    platform/graphics/win/PathDirect2D.cpp
    platform/graphics/win/PatternDirect2D.cpp
    platform/graphics/win/SimpleFontDataDirect2D.cpp
    platform/graphics/win/TextAnalyzerHelper.cpp

    platform/network/win/CurlSSLHandleWin.cpp

    platform/text/win/LocaleWin.cpp

    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragImageDirect2D.cpp
    platform/win/ImportedFunctionsEnumerator.cpp
    platform/win/ImportedModulesEnumerator.cpp
    platform/win/PEImage.cpp
)

list(APPEND WebCore_LIBRARIES
    ${DirectX_LIBRARIES}
    comctl32
    crypt32
    iphlpapi
    rpcrt4
    shlwapi
    usp10
    version
    winmm
    ws2_32
)

list(APPEND WebCoreTestSupport_LIBRARIES
    shlwapi
)

set(WebCore_OUTPUT_NAME WebCore${DEBUG_SUFFIX})
