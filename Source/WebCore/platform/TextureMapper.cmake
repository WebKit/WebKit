list(APPEND WebCore_INCLUDE_DIRECTORIES
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
    list(APPEND WebCore_INCLUDE_DIRECTORIES
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

    # FIXME: Move this into Nicosia.cmake once the component is set for long-term use.
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/nicosia"
        "${WEBCORE_DIR}/platform/graphics/nicosia/cairo"
        "${WEBCORE_DIR}/platform/graphics/nicosia/texmap"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/nicosia/NicosiaBuffer.cpp
        platform/graphics/nicosia/NicosiaPaintingContext.cpp
        platform/graphics/nicosia/NicosiaPaintingEngine.cpp
        platform/graphics/nicosia/NicosiaPaintingEngineBasic.cpp
        platform/graphics/nicosia/NicosiaPaintingEngineThreaded.cpp
        platform/graphics/nicosia/NicosiaPlatformLayer.cpp
        platform/graphics/nicosia/NicosiaScene.cpp

        platform/graphics/nicosia/cairo/NicosiaCairoOperationRecorder.cpp
        platform/graphics/nicosia/cairo/NicosiaPaintingContextCairo.cpp

        platform/graphics/nicosia/texmap/NicosiaBackingStoreTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaCompositionLayerTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaContentLayerTextureMapperImpl.cpp
        platform/graphics/nicosia/texmap/NicosiaGC3DLayer.cpp
        platform/graphics/nicosia/texmap/NicosiaImageBackingTextureMapperImpl.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
        platform/graphics/texmap/TextureMapperTiledBackingStore.cpp
    )
endif ()
