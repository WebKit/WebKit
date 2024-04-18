list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/texmap"
)

list(APPEND WebCore_SOURCES
    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/ClipStack.cpp
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGCGLPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperShaderProgram.cpp
    platform/graphics/texmap/TextureMapperTile.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/texmap/BitmapTexture.h
    platform/graphics/texmap/BitmapTexturePool.h
    platform/graphics/texmap/ClipStack.h
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.h
    platform/graphics/texmap/GraphicsLayerTextureMapper.h
    platform/graphics/texmap/TextureMapper.h
    platform/graphics/texmap/TextureMapperBackingStore.h
    platform/graphics/texmap/TextureMapperFlags.h
    platform/graphics/texmap/TextureMapperFPSCounter.h
    platform/graphics/texmap/TextureMapperGLHeaders.h
    platform/graphics/texmap/TextureMapperLayer.h
    platform/graphics/texmap/TextureMapperPlatformLayer.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxy.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxyGL.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxyProvider.h
    platform/graphics/texmap/TextureMapperSolidColorLayer.h
    platform/graphics/texmap/TextureMapperTile.h
    platform/graphics/texmap/TextureMapperTiledBackingStore.h
)

if (USE_TEXTURE_MAPPER_DMABUF)
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/TextureMapperPlatformLayerProxyDMABuf.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/TextureMapperPlatformLayerProxyDMABuf.h
    )
endif ()

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/TextureMapperPlatformLayerBuffer.cpp
        platform/graphics/texmap/TextureMapperPlatformLayerProxy.cpp
        platform/graphics/texmap/TextureMapperPlatformLayerProxyGL.cpp

        platform/graphics/texmap/coordinated/CoordinatedBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.cpp
        platform/graphics/texmap/coordinated/Tile.cpp
        platform/graphics/texmap/coordinated/TiledBackingStore.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.h
        platform/graphics/texmap/coordinated/Tile.h
        platform/graphics/texmap/coordinated/TiledBackingStore.h
        platform/graphics/texmap/coordinated/TiledBackingStoreClient.h
    )

    if (USE_CAIRO)
        list(APPEND WebCore_SOURCES
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
        platform/graphics/texmap/TextureMapperTiledBackingStore.cpp
    )

    # FIXME: Share NicosiaAnimation since its used outside of Nicosia
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/nicosia"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/nicosia/NicosiaAnimation.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/nicosia/NicosiaAnimation.h
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

        platform/graphics/nicosia/NicosiaAnimatedBackingStoreClient.h
        platform/graphics/nicosia/NicosiaAnimation.h
        platform/graphics/nicosia/NicosiaBackingStore.h
        platform/graphics/nicosia/NicosiaBuffer.h
        platform/graphics/nicosia/NicosiaCompositionLayer.h
        platform/graphics/nicosia/NicosiaContentLayer.h
        platform/graphics/nicosia/NicosiaImageBacking.h
        platform/graphics/nicosia/NicosiaImageBackingStore.h
        platform/graphics/nicosia/NicosiaPlatformLayer.h
        platform/graphics/nicosia/NicosiaScene.h
        platform/graphics/nicosia/NicosiaSceneIntegration.h
    )

    if (USE_CAIRO)
        list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
            "${WEBCORE_DIR}/platform/graphics/nicosia/cairo"
        )
        list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
            platform/graphics/nicosia/NicosiaPaintingEngine.h
        )

        # Currently NicosiaPaintingContext.cpp will cause a compilation error
        # when building without USE_CAIRO so these are not in unified sources
        list(APPEND WebCore_SOURCES
            platform/graphics/nicosia/NicosiaPaintingContext.cpp
            platform/graphics/nicosia/NicosiaPaintingEngine.cpp
            platform/graphics/nicosia/NicosiaPaintingEngineBasic.cpp
            platform/graphics/nicosia/NicosiaPaintingEngineThreaded.cpp

            platform/graphics/nicosia/cairo/NicosiaCairoOperationRecorder.cpp
            platform/graphics/nicosia/cairo/NicosiaPaintingContextCairo.cpp
        )
    endif ()
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
        platform/graphics/gbm/DMABufColorSpace.h
        platform/graphics/gbm/DMABufEGLUtilities.h
        platform/graphics/gbm/DMABufFormat.h
        platform/graphics/gbm/DMABufObject.h
        platform/graphics/gbm/DMABufReleaseFlag.h
        platform/graphics/gbm/DRMDeviceManager.h
        platform/graphics/gbm/DRMDeviceNode.h
        platform/graphics/gbm/GBMBufferSwapchain.h
        platform/graphics/gbm/GraphicsContextGLGBM.h
    )
endif ()
