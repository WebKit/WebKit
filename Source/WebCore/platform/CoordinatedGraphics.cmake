list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/page/scrolling/coordinated"
    "${WEBCORE_DIR}/platform/graphics/texmap"
    "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
)

list(APPEND WebCore_SOURCES
    page/scrolling/coordinated/ScrollerCoordinated.cpp
    page/scrolling/coordinated/ScrollerPairCoordinated.cpp
    page/scrolling/coordinated/ScrollingStateNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingStateScrollingNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeFixedNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeFrameScrollingNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeOverflowScrollProxyNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeOverflowScrollingNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreePositionedNodeCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeScrollingNodeDelegateCoordinated.cpp
    page/scrolling/coordinated/ScrollingTreeStickyNodeCoordinated.cpp

    platform/graphics/texmap/coordinated/CoordinatedAnimatedBackingStoreClient.cpp
    platform/graphics/texmap/coordinated/CoordinatedBackingStoreProxy.cpp
    platform/graphics/texmap/coordinated/CoordinatedImageBackingStore.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayer.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferExternalOES.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferHolePunch.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferNativeImage.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferProxy.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferRGB.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferSkiaDeferredImage.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferSkiaImage.cpp
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferYUV.cpp
    platform/graphics/texmap/coordinated/CoordinatedTileBuffer.cpp
    platform/graphics/texmap/coordinated/GraphicsContextGLTextureMapperANGLECoordinated.cpp
    platform/graphics/texmap/coordinated/GraphicsLayerAsyncContentsDisplayDelegateCoordinated.cpp
    platform/graphics/texmap/coordinated/GraphicsLayerContentsDisplayDelegateCoordinated.cpp
    platform/graphics/texmap/coordinated/GraphicsLayerCoordinated.cpp
)

if (USE_TEXTURE_MAPPER)
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.h
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/FloatPlane3D.cpp
        platform/graphics/texmap/FloatPolygon3D.cpp
        platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.cpp
        platform/graphics/texmap/TextureMapperAnimation.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/FloatPlane3D.h
        platform/graphics/texmap/FloatPolygon3D.h
        platform/graphics/texmap/GraphicsContextGLTextureMapperANGLE.h
        platform/graphics/texmap/TextureMapperAnimation.h
        platform/graphics/texmap/TextureMapperFlags.h
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    page/scrolling/coordinated/ScrollingTreeCoordinated.h

    platform/graphics/texmap/coordinated/CoordinatedAnimatedBackingStoreClient.h
    platform/graphics/texmap/coordinated/CoordinatedBackingStoreProxy.h
    platform/graphics/texmap/coordinated/CoordinatedCompositionReason.h
    platform/graphics/texmap/coordinated/CoordinatedImageBackingStore.h
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayer.h
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBuffer.h
    platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferProxy.h
    platform/graphics/texmap/coordinated/CoordinatedTileBuffer.h
    platform/graphics/texmap/coordinated/GraphicsLayerContentsDisplayDelegateCoordinated.h
    platform/graphics/texmap/coordinated/GraphicsLayerCoordinated.h
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
        platform/graphics/gbm/DRMDevice.h
        platform/graphics/gbm/DRMDeviceManager.h
        platform/graphics/gbm/GBMDevice.h
        platform/graphics/gbm/GraphicsContextGLTextureMapperGBM.h
        platform/graphics/gbm/MemoryMappedGPUBuffer.h

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
    )
endif ()
