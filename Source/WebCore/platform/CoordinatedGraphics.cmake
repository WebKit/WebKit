list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/page/scrolling/coordinated"
    "${WEBCORE_DIR}/platform/graphics/coordinated"
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

    platform/graphics/coordinated/AcceleratedAnimation.cpp
    platform/graphics/coordinated/AcceleratedAnimations.cpp
    platform/graphics/coordinated/CoordinatedAnimatedBackingStoreClient.cpp
    platform/graphics/coordinated/CoordinatedBackingStoreProxy.cpp
    platform/graphics/coordinated/CoordinatedImageBackingStore.cpp
    platform/graphics/coordinated/CoordinatedPlatformLayer.cpp
    platform/graphics/coordinated/CoordinatedPlatformLayerBufferHolePunch.cpp
    platform/graphics/coordinated/CoordinatedPlatformLayerBufferProxy.cpp
    platform/graphics/coordinated/CoordinatedPlatformLayerBufferSkiaDeferredImage.cpp
    platform/graphics/coordinated/CoordinatedPlatformLayerBufferSkiaImage.cpp
    platform/graphics/coordinated/CoordinatedTileBuffer.cpp
    platform/graphics/coordinated/GraphicsContextGLEGLCoordinated.cpp
    platform/graphics/coordinated/GraphicsLayerAsyncContentsDisplayDelegateCoordinated.cpp
    platform/graphics/coordinated/GraphicsLayerContentsDisplayDelegateCoordinated.cpp
    platform/graphics/coordinated/GraphicsLayerCoordinated.cpp
)

if (USE_TEXTURE_MAPPER)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/texmap"
        "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    )

    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.cpp
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.cpp
        platform/graphics/texmap/coordinated/CoordinatedImageBackingStoreTextureMapper.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferDMABufTextureMapper.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferExternalOES.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferNativeImage.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferRGB.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferVideoTextureMapper.cpp
        platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferYUV.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/texmap/coordinated/CoordinatedBackingStore.h
        platform/graphics/texmap/coordinated/CoordinatedBackingStoreTile.h
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    page/scrolling/coordinated/ScrollingTreeCoordinated.h

    platform/graphics/coordinated/AcceleratedAnimation.h
    platform/graphics/coordinated/AcceleratedAnimations.h
    platform/graphics/coordinated/CoordinatedAnimatedBackingStoreClient.h
    platform/graphics/coordinated/CoordinatedBackingStoreProxy.h
    platform/graphics/coordinated/CoordinatedCompositionReason.h
    platform/graphics/coordinated/CoordinatedImageBackingStore.h
    platform/graphics/coordinated/CoordinatedPlatformLayer.h
    platform/graphics/coordinated/CoordinatedPlatformLayerBuffer.h
    platform/graphics/coordinated/CoordinatedPlatformLayerBufferProxy.h
    platform/graphics/coordinated/CoordinatedTileBuffer.h
    platform/graphics/coordinated/GraphicsLayerContentsDisplayDelegateCoordinated.h
    platform/graphics/coordinated/GraphicsLayerCoordinated.h
)

if (USE_GSTREAMER)
    list(APPEND WebCore_SOURCES
        platform/graphics/coordinated/CoordinatedPlatformLayerBufferVideo.cpp
    )
endif ()

if (USE_GBM)
    list(APPEND WebCore_SOURCES
        platform/graphics/coordinated/CoordinatedPlatformLayerBufferDMABuf.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/coordinated/CoordinatedPlatformLayerBufferDMABuf.h

        platform/graphics/gbm/DRMDevice.h
        platform/graphics/gbm/DRMDeviceManager.h
        platform/graphics/gbm/GBMDevice.h
        platform/graphics/gbm/GraphicsContextGLGBM.h
        platform/graphics/gbm/MemoryMappedGPUBuffer.h
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
