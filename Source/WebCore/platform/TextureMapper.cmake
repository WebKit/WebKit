list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/texmap"
)

list(APPEND WebCore_SOURCES
    platform/graphics/texmap/ClipPath.cpp
    platform/graphics/texmap/ClipStack.cpp
    platform/graphics/texmap/FloatPlane3D.cpp
    platform/graphics/texmap/FloatPolygon3D.cpp
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.cpp
    platform/graphics/texmap/TextureMapper.cpp
    platform/graphics/texmap/TextureMapperAnimation.cpp
    platform/graphics/texmap/TextureMapperBackingStore.cpp
    platform/graphics/texmap/TextureMapperDamageVisualizer.cpp
    platform/graphics/texmap/TextureMapperFPSCounter.cpp
    platform/graphics/texmap/TextureMapperGCGLPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperGPUBuffer.cpp
    platform/graphics/texmap/TextureMapperLayer.cpp
    platform/graphics/texmap/TextureMapperLayer3DRenderingContext.cpp
    platform/graphics/texmap/TextureMapperPlatformLayer.cpp
    platform/graphics/texmap/TextureMapperShaderProgram.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/texmap/ClipPath.h
    platform/graphics/texmap/ClipStack.h
    platform/graphics/texmap/FloatPlane3D.h
    platform/graphics/texmap/FloatPolygon3D.h
    platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.h
    platform/graphics/texmap/GraphicsLayerTextureMapper.h
    platform/graphics/texmap/TextureMapper.h
    platform/graphics/texmap/TextureMapperAnimation.h
    platform/graphics/texmap/TextureMapperBackingStore.h
    platform/graphics/texmap/TextureMapperDamageVisualizer.h
    platform/graphics/texmap/TextureMapperFlags.h
    platform/graphics/texmap/TextureMapperFPSCounter.h
    platform/graphics/texmap/TextureMapperGLHeaders.h
    platform/graphics/texmap/TextureMapperGPUBuffer.h
    platform/graphics/texmap/TextureMapperLayer.h
    platform/graphics/texmap/TextureMapperLayer3DRenderingContext.h
    platform/graphics/texmap/TextureMapperPlatformLayer.h
    platform/graphics/texmap/TextureMapperSolidColorLayer.h
    platform/graphics/texmap/TextureMapperTile.h
    platform/graphics/texmap/TextureMapperTiledBackingStore.h
)

if (NOT USE_COORDINATED_GRAPHICS)
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
