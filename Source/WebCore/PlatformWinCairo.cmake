list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore"
    "${DirectX_INCLUDE_DIRS}"
    "$ENV{WEBKIT_LIBRARIES}/include"
    "$ENV{WEBKIT_LIBRARIES}/include/cairo"
    "$ENV{WEBKIT_LIBRARIES}/include/SQLite"
    "$ENV{WEBKIT_LIBRARIES}/include/zlib"
    "${JAVASCRIPTCORE_DIR}/wtf/text"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/network/curl"
    "${WEBCORE_DIR}/platform/network/win"
)

list(APPEND WebCore_SOURCES
    accessibility/win/AXObjectCacheWin.cpp
    accessibility/win/AccessibilityObjectWin.cpp
    accessibility/win/AccessibilityObjectWrapperWin.cpp

    editing/SmartReplace.cpp
    editing/SmartReplaceCF.cpp

    loader/archive/cf/LegacyWebArchive.cpp

    page/win/FrameCairoWin.cpp
    page/win/FrameWin.cpp

    platform/cf/CFURLExtras.cpp
    platform/cf/FileSystemCF.cpp
    platform/cf/SharedBufferCF.cpp
    platform/cf/URLCF.cpp

    platform/cf/win/CertificateCFWin.cpp

    platform/graphics/FontPlatformData.cpp
    platform/graphics/GLContext.cpp
    platform/graphics/GraphicsLayer.cpp
    platform/graphics/ImageSource.cpp
    platform/graphics/PlatformDisplay.cpp
    platform/graphics/ShadowBlur.cpp
    platform/graphics/WOFFFileFormat.cpp

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

    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTextureGL.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperAnimation.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGL.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperShaderProgram.cpp
    platform/graphics/texmap/TextureMapperSurfaceBackingStore.cpp
    platform/graphics/texmap/TextureMapperTile.cpp
    platform/graphics/texmap/TextureMapperTiledBackingStore.cpp

    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/FontCacheWin.cpp
    platform/graphics/win/FontCustomPlatformDataCairo.cpp
    platform/graphics/win/FontPlatformDataCairoWin.cpp
    platform/graphics/win/FontPlatformDataWin.cpp
    platform/graphics/win/FontWin.cpp
    platform/graphics/win/FullScreenController.cpp
    platform/graphics/win/GlyphPageTreeNodeCairoWin.cpp
    platform/graphics/win/GraphicsContextCairoWin.cpp
    platform/graphics/win/GraphicsContextWin.cpp
    platform/graphics/win/ImageCairoWin.cpp
    platform/graphics/win/SimpleFontDataCairoWin.cpp
    platform/graphics/win/SimpleFontDataWin.cpp
    platform/graphics/win/TransformationMatrixWin.cpp
    platform/graphics/win/UniscribeController.cpp

    platform/image-decoders/ImageDecoder.cpp

    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/bmp/BMPImageReader.cpp

    platform/image-decoders/cairo/ImageDecoderCairo.cpp

    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp

    platform/image-decoders/ico/ICOImageDecoder.cpp

    platform/image-decoders/jpeg/JPEGImageDecoder.cpp

    platform/image-decoders/png/PNGImageDecoder.cpp

    platform/image-decoders/webp/WEBPImageDecoder.cpp

    platform/network/CredentialStorage.cpp
    platform/network/NetworkStorageSessionStub.cpp
    platform/network/SynchronousLoaderClient.cpp

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

    platform/network/win/DownloadBundleWin.cpp

    platform/text/cf/HyphenationCF.cpp

    platform/text/win/LocaleWin.cpp
    platform/text/win/TextBreakIteratorInternalICUWin.cpp

    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragImageCairoWin.cpp
    platform/win/GDIObjectCounter.cpp
    platform/win/ImportedFunctionsEnumerator.cpp
    platform/win/ImportedModulesEnumerator.cpp
    platform/win/LoggingWin.cpp
    platform/win/PEImage.cpp
    platform/win/PathWalker.cpp
    platform/win/ScrollbarThemeSafari.cpp
    platform/win/WebCoreBundleWin.cpp
    platform/win/WebCoreTextRenderer.cpp
    platform/win/WindowMessageBroadcaster.cpp

    rendering/RenderLayerBacking.cpp
    rendering/RenderLayerCompositor.cpp
    rendering/RenderThemeSafari.cpp
    rendering/RenderThemeWin.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeWin.css
    ${WEBCORE_DIR}/css/themeWinQuirks.css
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

set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    accessibility
    bindings
    bridge
    css
    dom
    editing
    history
    html
    inspector
    loader
    page
    platform
    plugins
    rendering
    storage
    svg
    websockets
    workers
    xml

    Modules/geolocation
    Modules/notifications
    Modules/webdatabase

    accessibility/win

    bindings/generic
    bindings/js

    bridge/jsc

    history/cf

    html/forms
    html/parser

    loader/appcache
    loader/archive
    loader/cache
    loader/icon

    loader/archive/cf

    page/animation
    page/win

    platform/animation
    platform/cf
    platform/graphics
    platform/mock
    platform/network
    platform/sql
    platform/text
    platform/win

    platform/cf/win

    platform/graphics/cairo
    platform/graphics/opentype
    platform/graphics/transforms
    platform/graphics/win

    platform/graphics/ca/win

    platform/network/curl

    platform/text/transcoder

    rendering/style
    rendering/svg

    svg/animation
    svg/graphics
    svg/properties

    svg/graphics/filters
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES})
