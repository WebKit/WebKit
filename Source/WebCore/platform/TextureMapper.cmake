list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/texmap"
)
list(APPEND WebCore_SOURCES
    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/GraphicsContext3DTextureMapper.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperAnimation.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGC3DPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperTile.cpp
)
list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/nicosia/NicosiaBuffer.h
    platform/graphics/nicosia/NicosiaPaintingEngine.h

    platform/graphics/texmap/BitmapTexture.h
    platform/graphics/texmap/ClipStack.h
    platform/graphics/texmap/GraphicsLayerTextureMapper.h
    platform/graphics/texmap/TextureMapper.h
    platform/graphics/texmap/TextureMapperAnimation.h
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

    platform/graphics/texmap/coordinated/AreaAllocator.h
    platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.h
    platform/graphics/texmap/coordinated/CoordinatedGraphicsState.h
    platform/graphics/texmap/coordinated/CoordinatedImageBacking.h
    platform/graphics/texmap/coordinated/SurfaceUpdateInfo.h
    platform/graphics/texmap/coordinated/Tile.h
    platform/graphics/texmap/coordinated/TiledBackingStore.h
    platform/graphics/texmap/coordinated/TiledBackingStoreClient.h
    platform/graphics/texmap/coordinated/UpdateAtlas.h
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
        "${WEBCORE_DIR}/page/scrolling/coordinatedgraphics"
        "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    )
    list(APPEND WebCore_SOURCES
        page/scrolling/coordinatedgraphics/ScrollingCoordinatorCoordinatedGraphics.cpp
        page/scrolling/coordinatedgraphics/ScrollingStateNodeCoordinatedGraphics.cpp

        platform/graphics/texmap/TextureMapperPlatformLayerBuffer.cpp
        platform/graphics/texmap/TextureMapperPlatformLayerProxy.cpp

        platform/graphics/texmap/coordinated/AreaAllocator.cpp
        platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.cpp
        platform/graphics/texmap/coordinated/CoordinatedImageBacking.cpp
        platform/graphics/texmap/coordinated/Tile.cpp
        platform/graphics/texmap/coordinated/TiledBackingStore.cpp
        platform/graphics/texmap/coordinated/UpdateAtlas.cpp
    )

    # FIXME: Move this into Nicosia.cmake once the component is set for long-term use.
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/nicosia"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/nicosia/NicosiaBuffer.cpp
        platform/graphics/nicosia/NicosiaPaintingContext.cpp
        platform/graphics/nicosia/NicosiaPaintingContextCairo.cpp
        platform/graphics/nicosia/NicosiaPaintingEngine.cpp
        platform/graphics/nicosia/NicosiaPaintingEngineBasic.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
        platform/graphics/texmap/TextureMapperTiledBackingStore.cpp
    )
endif ()
