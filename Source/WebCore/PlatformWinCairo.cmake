include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_LIBRARIES_DIR}/include"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/platform/cf"
)

list(APPEND WebCore_SOURCES
    page/win/FrameCairoWin.cpp
    page/win/ResourceUsageOverlayWin.cpp
    page/win/ResourceUsageThreadWin.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/win/FontCustomPlatformDataCairo.cpp
    platform/graphics/win/FontPlatformDataCairoWin.cpp
    platform/graphics/win/GlyphPageTreeNodeCairoWin.cpp
    platform/graphics/win/GraphicsContextCairoWin.cpp
    platform/graphics/win/ImageCairoWin.cpp
    platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    platform/graphics/win/SimpleFontDataCairoWin.cpp

    platform/network/win/CurlSSLHandleWin.cpp

    platform/text/win/LocaleWin.cpp

    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragImageCairoWin.cpp
    platform/win/ImportedFunctionsEnumerator.cpp
    platform/win/ImportedModulesEnumerator.cpp
    platform/win/PEImage.cpp
)

list(APPEND WebCore_LIBRARIES
    D3d9
    Mf
    Mfplat
    comctl32
    crypt32
    delayimp
    dxva2
    evr
    iphlpapi
    rpcrt4
    shlwapi
    usp10
    version
    winmm
    ws2_32
)

target_link_options(WebCore PUBLIC /DELAYLOAD:mf.dll /DELAYLOAD:mfplat.dll)

if (USE_WOFF2)
    # The WOFF2 libraries don't compile as DLLs on Windows, so add in
    # the additional libraries WOFF2::dec requires
    list(APPEND WebCore_LIBRARIES
        WOFF2::common
        brotlidec
    )
endif ()

list(APPEND WebCoreTestSupport_LIBRARIES
    Cairo::Cairo
    shlwapi
)
