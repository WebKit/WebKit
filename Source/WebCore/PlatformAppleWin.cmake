add_definitions(-DQUARTZCORE_DLL -DDISABLE_COREIMAGE -DDISABLE_FRONTEND -DDISABLE_IOSURFACE -DDISABLE_RENDERSERVER
    -DDISABLE_3D_TRANSFORMS -DWEBCORE_CONTEXT_MENUS -DPSAPI_VERSION=1)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_LIBRARIES_DIR}/include"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/win"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/network/cf"
)

list(APPEND WebCore_SOURCES
    loader/cf/ResourceLoaderCFNet.cpp

    page/CaptionUserPreferencesMediaAF.cpp

    platform/cf/MediaAccessibilitySoftLink.cpp

    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/WebMediaSessionManagerMac.cpp

    platform/graphics/avfoundation/cf/CDMSessionAVFoundationCF.cpp
    platform/graphics/avfoundation/cf/InbandTextTrackPrivateLegacyAVCF.cpp
    platform/graphics/avfoundation/cf/InbandTextTrackPrivateAVCF.cpp
    platform/graphics/avfoundation/cf/MediaPlayerPrivateAVFoundationCF.cpp
    platform/graphics/avfoundation/cf/WebCoreAVCFResourceLoader.cpp

    platform/graphics/win/FontCustomPlatformData.cpp

    platform/network/cf/AuthenticationCF.cpp
    platform/network/cf/CertificateInfoCFNet.cpp
    platform/network/cf/CookieStorageCFNet.cpp
    platform/network/cf/CredentialStorageCFNet.cpp
    platform/network/cf/DNSResolveQueueCFNet.cpp
    platform/network/cf/FormDataStreamCFNet.cpp
    platform/network/cf/LoaderRunLoopCF.cpp
    platform/network/cf/NetworkStorageSessionCFNet.cpp
    platform/network/cf/NetworkStorageSessionCFNetWin.cpp
    platform/network/cf/ProtectionSpaceCFNet.cpp
    platform/network/cf/ProxyServerCFNet.cpp
    platform/network/cf/ResourceErrorCF.cpp
    platform/network/cf/ResourceHandleCFNet.cpp
    platform/network/cf/ResourceHandleCFURLConnectionDelegate.cpp
    platform/network/cf/ResourceHandleCFURLConnectionDelegateWithOperationQueue.cpp
    platform/network/cf/ResourceRequestCFNet.cpp
    platform/network/cf/ResourceResponseCFNet.cpp
    platform/network/cf/SocketStreamHandleImplCFNet.cpp
    platform/network/cf/SynchronousLoaderClientCFNet.cpp

    platform/text/LocaleNone.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/network/cf/AuthenticationCF.h
    platform/network/cf/AuthenticationChallenge.h
    platform/network/cf/CertificateInfo.h
    platform/network/cf/DownloadBundle.h
    platform/network/cf/LoaderRunLoopCF.h
    platform/network/cf/ProtectionSpaceCFNet.h
    platform/network/cf/ResourceError.h
    platform/network/cf/ResourceRequest.h
    platform/network/cf/ResourceRequestCFNet.h
    platform/network/cf/ResourceResponse.h
    platform/network/cf/SocketStreamHandleImpl.h
)

if (${USE_DIRECT2D})
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/win"
    )

    list(APPEND WebCore_SOURCES
        page/win/FrameWinDirect2D.cpp

        platform/graphics/win/FontCascadeDirect2D.cpp
        platform/graphics/win/FontPlatformDataDirect2D.cpp
        platform/graphics/win/GlyphPageTreeNodeDirect2D.cpp
        platform/graphics/win/GradientDirect2D.cpp
        platform/graphics/win/GraphicsContextDirect2D.cpp
        platform/graphics/win/GraphicsLayerDirect2D.cpp
        platform/graphics/win/ImageBufferDataDirect2D.cpp
        platform/graphics/win/ImageBufferDirect2D.cpp
        platform/graphics/win/ImageDecoderDirect2D.cpp
        platform/graphics/win/ImageDirect2D.cpp
        platform/graphics/win/NativeImageDirect2D.cpp
        platform/graphics/win/PathDirect2D.cpp
        platform/graphics/win/PatternDirect2D.cpp
        platform/graphics/win/SimpleFontDataDirect2D.cpp
        platform/graphics/win/TextAnalyzerHelper.cpp

        platform/win/DragImageDirect2D.cpp
    )
else ()
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/avfoundation"
        "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
        "${WEBCORE_DIR}/platform/graphics/ca"
        "${WEBCORE_DIR}/platform/graphics/ca/win"
        "${WEBCORE_DIR}/platform/graphics/cg"
    )

    list(APPEND WebCore_SOURCES
        page/win/FrameCGWin.cpp

        platform/graphics/ca/GraphicsLayerCA.cpp
        platform/graphics/ca/LayerPool.cpp
        platform/graphics/ca/PlatformCALayer.cpp
        platform/graphics/ca/TileController.cpp
        platform/graphics/ca/TileCoverageMap.cpp
        platform/graphics/ca/TileGrid.cpp
        platform/graphics/ca/TransformationMatrixCA.cpp

        platform/graphics/ca/win/CACFLayerTreeHost.cpp
        platform/graphics/ca/win/LayerChangesFlusher.cpp
        platform/graphics/ca/win/PlatformCAAnimationWin.cpp
        platform/graphics/ca/win/PlatformCAFiltersWin.cpp
        platform/graphics/ca/win/PlatformCALayerWin.cpp
        platform/graphics/ca/win/PlatformCALayerWinInternal.cpp
        platform/graphics/ca/win/WKCACFViewLayerTreeHost.cpp
        platform/graphics/ca/win/WebTiledBackingLayerWin.cpp

        platform/graphics/cg/ColorCG.cpp
        platform/graphics/cg/FloatPointCG.cpp
        platform/graphics/cg/FloatRectCG.cpp
        platform/graphics/cg/FloatSizeCG.cpp
        platform/graphics/cg/GradientCG.cpp
        platform/graphics/cg/GraphicsContext3DCG.cpp
        platform/graphics/cg/GraphicsContextCG.cpp
        platform/graphics/cg/IOSurfacePool.cpp
        platform/graphics/cg/ImageBufferCG.cpp
        platform/graphics/cg/ImageBufferDataCG.cpp
        platform/graphics/cg/ImageBufferUtilitiesCG.cpp
        platform/graphics/cg/ImageDecoderCG.cpp
        platform/graphics/cg/ImageSourceCGWin.cpp
        platform/graphics/cg/IntPointCG.cpp
        platform/graphics/cg/IntRectCG.cpp
        platform/graphics/cg/IntSizeCG.cpp
        platform/graphics/cg/NativeImageCG.cpp
        platform/graphics/cg/PDFDocumentImage.cpp
        platform/graphics/cg/PathCG.cpp
        platform/graphics/cg/PatternCG.cpp
        platform/graphics/cg/SubimageCacheWithTimer.cpp
        platform/graphics/cg/TransformationMatrixCG.cpp
        platform/graphics/cg/UTIRegistry.cpp

        platform/graphics/opentype/OpenTypeCG.cpp

        platform/graphics/win/FontCGWin.cpp
        platform/graphics/win/FontPlatformDataCGWin.cpp
        platform/graphics/win/GlyphPageTreeNodeCGWin.cpp
        platform/graphics/win/GraphicsContextCGWin.cpp
        platform/graphics/win/ImageCGWin.cpp
        platform/graphics/win/SimpleFontDataCGWin.cpp

        platform/win/DragImageCGWin.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/ca/GraphicsLayerCA.h
        platform/graphics/ca/LayerPool.h
        platform/graphics/ca/PlatformCAAnimation.h
        platform/graphics/ca/PlatformCAFilters.h
        platform/graphics/ca/PlatformCALayer.h
        platform/graphics/ca/PlatformCALayerClient.h
        platform/graphics/ca/TileController.h

        platform/graphics/ca/win/AbstractCACFLayerTreeHost.h
        platform/graphics/ca/win/CACFLayerTreeHost.h
        platform/graphics/ca/win/CACFLayerTreeHostClient.h
        platform/graphics/ca/win/PlatformCALayerWin.h

        platform/graphics/cg/GraphicsContextCG.h
        platform/graphics/cg/IOSurfacePool.h
        platform/graphics/cg/ImageBufferDataCG.h
        platform/graphics/cg/ImageBufferUtilitiesCG.h
        platform/graphics/cg/PDFDocumentImage.h
        platform/graphics/cg/UTIRegistry.h
    )
endif ()
