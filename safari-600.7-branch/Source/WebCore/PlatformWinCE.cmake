list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/wince"
    "${WEBCORE_DIR}/platform/graphics/wince"
    "${WEBCORE_DIR}/platform/network/win"
    "${3RDPARTY_DIR}/libjpeg"
    "${3RDPARTY_DIR}/libpng"
    "${3RDPARTY_DIR}/libxml2/include"
    "${3RDPARTY_DIR}/libxslt/include"
    "${3RDPARTY_DIR}/sqlite"
    "${3RDPARTY_DIR}/zlib"
)

list(APPEND WebCore_SOURCES
    page/win/FrameGdiWin.cpp

    platform/ScrollAnimatorNone.cpp

    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/GDIExtras.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp

    platform/graphics/wince/FontCacheWince.cpp
    platform/graphics/wince/FontCustomPlatformData.cpp
    platform/graphics/wince/FontPlatformData.cpp
    platform/graphics/wince/FontWince.cpp
    platform/graphics/wince/GlyphPageTreeNodeWince.cpp
    platform/graphics/wince/GradientWince.cpp
    platform/graphics/wince/GraphicsContextWince.cpp
    platform/graphics/wince/ImageBufferWince.cpp
    platform/graphics/wince/ImageWinCE.cpp
    platform/graphics/wince/PathWince.cpp
    platform/graphics/wince/PlatformPathWince.cpp
    platform/graphics/wince/SharedBitmap.cpp
    platform/graphics/wince/SimpleFontDataWince.cpp

    platform/network/NetworkStorageSessionStub.cpp

    platform/network/win/CookieJarWin.cpp
    platform/network/win/CredentialStorageWin.cpp
    platform/network/win/ProxyServerWin.cpp
    platform/network/win/ResourceHandleWin.cpp
    platform/network/win/SocketStreamHandleWin.cpp

    platform/text/LocaleNone.cpp

    platform/text/win/TextCodecWin.cpp

    rendering/RenderThemeWince.cpp
)

list(APPEND WebCore_LIBRARIES
    crypt32
    iphlpapi
    libjpeg
    libpng
    libxml2
    libxslt
    sqlite
    wininet
)
