list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/texmap"
    "${WEBCORE_DIR}/platform/graphics/nicosia"
)

list(APPEND WebCore_SOURCES
    platform/graphics/nicosia/NicosiaAnimation.cpp
    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/GraphicsContextGLTextureMapper.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGCGLPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperTile.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/nicosia/NicosiaAnimation.h
    platform/graphics/texmap/BitmapTexture.h
    platform/graphics/texmap/ClipStack.h
    platform/graphics/texmap/GraphicsLayerTextureMapper.h
    platform/graphics/texmap/TextureMapper.h
    platform/graphics/texmap/TextureMapperBackingStore.h
    platform/graphics/texmap/TextureMapperContextAttributes.h
    platform/graphics/texmap/TextureMapperFPSCounter.h
    platform/graphics/texmap/TextureMapperGL.h
    platform/graphics/texmap/TextureMapperGLHeaders.h
    platform/graphics/texmap/TextureMapperLayer.h
    platform/graphics/texmap/TextureMapperPlatformLayer.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxy.h
    platform/graphics/texmap/TextureMapperPlatformLayerProxyProvider.h
    platform/graphics/texmap/TextureMapperTile.h
    platform/graphics/texmap/TextureMapperTiledBackingStore.h
)

if (USE_TEXTURE_MAPPER_GL)
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/BitmapTextureGL.cpp
        platform/graphics/texmap/ClipStack.cpp
        platform/graphics/texmap/TextureMapperContextAttributes.cpp
        platform/graphics/texmap/TextureMapperGL.cpp
        platform/graphics/texmap/TextureMapperShaderProgram.cpp
    )
endif ()

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/page/scrolling/nicosia"
        "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/TextureMapperPlatformLayerBuffer.cpp
        platform/graphics/texmap/TextureMapperPlatformLayerProxy.cpp

        platform/graphics/texmap/coordinated/CoordinatedBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.cpp
        platform/graphics/texmap/coordinated/Tile.cpp
        platform/graphics/texmap/coordinated/TiledBackingStore.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.h
        platform/graphics/texmap/coordinated/CoordinatedGraphicsState.h
        platform/graphics/texmap/coordinated/SurfaceUpdateInfo.h
        platform/graphics/texmap/coordinated/Tile.h
        platform/graphics/texmap/coordinated/TiledBackingStore.h
        platform/graphics/texmap/coordinated/TiledBackingStoreClient.h
    )

    # FIXME: Move this into Nicosia.cmake once the component is set for long-term use.
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/nicosia/cairo"
        "${WEBCORE_DIR}/platform/graphics/nicosia/texmap"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/nicosia/NicosiaBuffer.cpp
        platform/graphics/nicosia/NicosiaImageBufferPipe.cpp
        platform/graphics/nicosia/NicosiaPaintingContext.cpp
        platform/graphics/nicosia/NicosiaPaintingEngine.cpp
        platform/graphics/nicosia/NicosiaPaintingEngineBasic.cpp
        platform/graphics/nicosia/NicosiaPaintingEngineThreaded.cpp
        platform/graphics/nicosia/NicosiaPlatformLayer.cpp
        platform/graphics/nicosia/NicosiaScene.cpp
        platform/graphics/nicosia/NicosiaSceneIntegration.cpp

        platform/graphics/nicosia/cairo/NicosiaCairoOperationRecorder.cpp
        platform/graphics/nicosia/cairo/NicosiaPaintingContextCairo.cpp

        platform/graphics/nicosia/texmap/NicosiaBackingStoreTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaCompositionLayerTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaContentLayerTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaGCGLLayer.cpp
        platform/graphics/nicosia/texmap/NicosiaImageBackingTextureMapperImpl.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        page/scrolling/nicosia/ScrollingTreeFixedNode.h
        page/scrolling/nicosia/ScrollingTreeStickyNode.h

        platform/graphics/nicosia/NicosiaAnimatedBackingStoreClient.h
        platform/graphics/nicosia/NicosiaBuffer.h
        platform/graphics/nicosia/NicosiaPaintingEngine.h
        platform/graphics/nicosia/NicosiaPlatformLayer.h
        platform/graphics/nicosia/NicosiaScene.h
        platform/graphics/nicosia/NicosiaSceneIntegration.h

        platform/graphics/nicosia/texmap/NicosiaBackingStoreTextureMapperImpl.h
        platform/graphics/nicosia/texmap/NicosiaCompositionLayerTextureMapperImpl.h
        platform/graphics/nicosia/texmap/NicosiaContentLayerTextureMapperImpl.h
        platform/graphics/nicosia/texmap/NicosiaImageBackingTextureMapperImpl.h
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
        platform/graphics/texmap/TextureMapperTiledBackingStore.cpp
    )
endif ()

if (USE_ANGLE_WEBGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/nicosia/texmap/NicosiaGCGLANGLELayer.cpp
    )
endif ()
