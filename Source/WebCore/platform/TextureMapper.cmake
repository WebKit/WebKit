list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/texmap"
)

list(APPEND WebCore_SOURCES
    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/ClipStack.cpp
    platform/graphics/texmap/FloatPlane3D.cpp
    platform/graphics/texmap/FloatPolygon3D.cpp
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperAnimation.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGCGLPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperLayer3DRenderingContext.cpp
    platform/graphics/texmap/TextureMapperPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperShaderProgram.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/texmap/BitmapTexture.h
    platform/graphics/texmap/BitmapTexturePool.h
    platform/graphics/texmap/ClipStack.h
    platform/graphics/texmap/FloatPlane3D.h
    platform/graphics/texmap/FloatPolygon3D.h
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.h
    platform/graphics/texmap/GraphicsLayerContentsDisplayDelegateTextureMapper.h
    platform/graphics/texmap/GraphicsLayerTextureMapper.h
    platform/graphics/texmap/TextureMapper.h
    platform/graphics/texmap/TextureMapperAnimation.h
    platform/graphics/texmap/TextureMapperBackingStore.h
    platform/graphics/texmap/TextureMapperFlags.h
    platform/graphics/texmap/TextureMapperFPSCounter.h
    platform/graphics/texmap/TextureMapperGLHeaders.h
    platform/graphics/texmap/TextureMapperLayer.h
    platform/graphics/texmap/TextureMapperLayer3DRenderingContext.h
    platform/graphics/texmap/TextureMapperPlatformLayer.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxy.h
    platform/graphics/texmap/TextureMapperSolidColorLayer.h
    platform/graphics/texmap/TextureMapperTile.h
    platform/graphics/texmap/TextureMapperTiledBackingStore.h
)

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerAsyncContentsDisplayDelegateTextureMapper.cpp
        platform/graphics/texmap/TextureMapperPlatformLayerProxy.cpp

        platform/graphics/texmap/coordinated/CoordinatedAnimatedBackingStoreClient.cpp
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreProxy.cpp
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.cpp
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.cpp
        platform/graphics/texmap/coordinated/CoordinatedImageBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferExternalOES.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferHolePunch.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferNativeImage.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferRGB.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferYUV.cpp
        platform/graphics/texmap/coordinated/CoordinatedTileBuffer.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/coordinated/CoordinatedAnimatedBackingStoreClient.h
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreProxy.h
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.h
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.h
        platform/graphics/texmap/coordinated/CoordinatedImageBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBuffer.h
        platform/graphics/texmap/coordinated/CoordinatedTileBuffer.h
    )

    if (USE_GSTREAMER)
        list(APPEND WebCore_SOURCES
            platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferVideo.cpp
        )
    endif ()

    if (USE_GBM)
        list(APPEND WebCore_SOURCES
            platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferDMABuf.cpp
        )
        list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
            platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferDMABuf.h
        )
    endif ()

    if (USE_CAIRO)
        list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
            platform/graphics/cairo/CairoPaintingEngine.h
        )

        list(APPEND WebCore_SOURCES
            platform/graphics/cairo/CairoOperationRecorder.cpp
            platform/graphics/cairo/CairoPaintingContext.cpp
            platform/graphics/cairo/CairoPaintingEngine.cpp
            platform/graphics/cairo/CairoPaintingEngineBasic.cpp
            platform/graphics/cairo/CairoPaintingEngineThreaded.cpp

            platform/graphics/texmap/coordinated/CoordinatedGraphicsLayerCairo.cpp
        )
    elseif (USE_SKIA)
        list(APPEND WebCore_SOURCES
            platform/graphics/texmap/coordinated/CoordinatedGraphicsLayerSkia.cpp
        )
    endif ()
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
        platform/graphics/texmap/TextureMapperTile.cpp
        platform/graphics/texmap/TextureMapperTiledBackingStore.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/GraphicsLayerTextureMapper.h
        platform/graphics/texmap/TextureMapperTile.h
        platform/graphics/texmap/TextureMapperTiledBackingStore.h
    )
endif ()

if (USE_NICOSIA)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/page/scrolling/nicosia"
        "${WEBCORE_DIR}/platform/graphics/nicosia"
        "${WEBCORE_DIR}/platform/graphics/nicosia/texmap"
    )
    list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
        "platform/SourcesNicosia.txt"
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        page/scrolling/nicosia/ScrollingTreeFixedNodeNicosia.h
        page/scrolling/nicosia/ScrollingTreeStickyNodeNicosia.h

        platform/graphics/nicosia/NicosiaCompositionLayer.h
        platform/graphics/nicosia/NicosiaPlatformLayer.h
        platform/graphics/nicosia/NicosiaScene.h
        platform/graphics/nicosia/NicosiaSceneIntegration.h
    )
endif ()

if (USE_GRAPHICS_LAYER_WC)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/wc"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/TextureMapperSparseBackingStore.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/TextureMapperSparseBackingStore.h

        platform/graphics/wc/WCPlatformLayer.h
    )
endif ()

if (USE_GBM)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/gbm/DMABufBuffer.h
        platform/graphics/gbm/DRMDeviceManager.h
        platform/graphics/gbm/DRMDeviceNode.h
        platform/graphics/gbm/GraphicsContextGLTextureMapperGBM.h
        platform/graphics/gbm/GraphicsLayerContentsDisplayDelegateGBM.h
    )
endif ()
