include(platform/ImageDecoders.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore"
    "${DirectX_INCLUDE_DIRS}"
    "${WEBKIT_LIBRARIES_DIR}/include"
    "${WEBKIT_LIBRARIES_DIR}/include/cairo"
    "${WEBKIT_LIBRARIES_DIR}/include/SQLite"
    "${WEBKIT_LIBRARIES_DIR}/include/zlib"
    "${JAVASCRIPTCORE_DIR}/wtf/text"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/network/curl"
)

list(APPEND WebCore_SOURCES
    page/win/FrameCairoWin.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/cairo/BitmapImageCairo.cpp
    platform/graphics/cairo/CairoUtilities.cpp
    platform/graphics/cairo/FloatRectCairo.cpp
    platform/graphics/cairo/FontCairo.cpp
    platform/graphics/cairo/GradientCairo.cpp
    platform/graphics/cairo/GraphicsContext3DCairo.cpp
    platform/graphics/cairo/GraphicsContextCairo.cpp
    platform/graphics/cairo/ImageBufferCairo.cpp
    platform/graphics/cairo/ImageCairo.cpp
    platform/graphics/cairo/IntRectCairo.cpp
    platform/graphics/cairo/PathCairo.cpp
    platform/graphics/cairo/PatternCairo.cpp
    platform/graphics/cairo/PlatformContextCairo.cpp
    platform/graphics/cairo/PlatformPathCairo.cpp
    platform/graphics/cairo/RefPtrCairo.cpp
    platform/graphics/cairo/TransformationMatrixCairo.cpp

    platform/graphics/win/FontCustomPlatformDataCairo.cpp
    platform/graphics/win/FontPlatformDataCairoWin.cpp
    platform/graphics/win/GlyphPageTreeNodeCairoWin.cpp
    platform/graphics/win/GraphicsContextCairoWin.cpp
    platform/graphics/win/ImageCairoWin.cpp
    platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    platform/graphics/win/SimpleFontDataCairoWin.cpp

    platform/image-decoders/cairo/ImageDecoderCairo.cpp

    platform/network/NetworkStorageSessionStub.cpp

    platform/network/curl/CookieJarCurl.cpp
    platform/network/curl/CredentialStorageCurl.cpp
    platform/network/curl/CurlCacheEntry.cpp
    platform/network/curl/CurlCacheManager.cpp
    platform/network/curl/CurlDownload.cpp
    platform/network/curl/DNSCurl.cpp
    platform/network/curl/FormDataStreamCurl.cpp
    platform/network/curl/MultipartHandle.cpp
    platform/network/curl/ProxyServerCurl.cpp
    platform/network/curl/ResourceHandleCurl.cpp
    platform/network/curl/ResourceHandleManager.cpp
    platform/network/curl/SSLHandle.cpp
    platform/network/curl/SocketStreamHandleCurl.cpp

    platform/text/win/LocaleWin.cpp

    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragImageCairoWin.cpp
    platform/win/ImportedFunctionsEnumerator.cpp
    platform/win/ImportedModulesEnumerator.cpp
    platform/win/PEImage.cpp
)

list(APPEND WebCore_LIBRARIES
    ${DirectX_LIBRARIES}
    CFLite
    SQLite3
    cairo
    comctl32
    crypt32
    iphlpapi
    libcurl_imp
    libjpeg
    libpng
    libxml2
    libxslt
    rpcrt4
    shlwapi
    usp10
    version
    winmm
    ws2_32
    zdll
)

list(APPEND WebCoreTestSupport_LIBRARIES
    CFLite
    cairo
    shlwapi
)

list(APPEND WebCore_FORWARDING_HEADERS_DIRECTORIES
    platform/graphics/cairo

    platform/network/curl
)
