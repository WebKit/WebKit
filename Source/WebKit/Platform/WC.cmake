list(APPEND WebKit_SOURCES
    GPUProcess/graphics/RemoteGraphicsContextGLWC.cpp

    GPUProcess/graphics/wc/RemoteWCLayerTreeHost.cpp
    GPUProcess/graphics/wc/WCContentBufferManager.cpp
    GPUProcess/graphics/wc/WCRemoteFrameHostLayerManager.cpp
    GPUProcess/graphics/wc/WCScene.cpp
    GPUProcess/graphics/wc/WCSceneContext.cpp

    UIProcess/wc/DrawingAreaProxyWC.cpp

    WebProcess/GPU/graphics/wc/RemoteGraphicsContextGLProxyWC.cpp
    WebProcess/GPU/graphics/wc/RemoteWCLayerTreeHostProxy.cpp

    WebProcess/WebPage/wc/DrawingAreaWC.cpp
    WebProcess/WebPage/wc/GraphicsLayerWC.cpp
    WebProcess/WebPage/wc/WCBackingStore.cpp
    WebProcess/WebPage/wc/WCLayerFactory.cpp
    WebProcess/WebPage/wc/WCTileGrid.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/GPUProcess/graphics/wc"
    "${WEBKIT_DIR}/Shared/wc"
    "${WEBKIT_DIR}/UIProcess/wc"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/wc"
    "${WEBKIT_DIR}/WebProcess/WebPage/wc"
)

list(APPEND WebKit_MESSAGES_IN_FILES
    GPUProcess/graphics/wc/RemoteWCLayerTreeHost
)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    WebProcess/WebPage/wc/WCBackingStore.serialization.in
    WebProcess/WebPage/wc/WCUpdateInfo.serialization.in
)

if (USE_CAIRO)
    list(APPEND WebKit_SOURCES
        UIProcess/cairo/BackingStoreCairo.cpp
    )
elseif (USE_SKIA)
    list(APPEND WebKit_SOURCES
        UIProcess/skia/BackingStoreSkia.cpp
    )
endif ()

if (USE_GRAPHICS_LAYER_TEXTURE_MAPPER)
    list(APPEND WebKit_SOURCES
        WebProcess/WebPage/CoordinatedGraphics/LayerTreeHostTextureMapper.cpp
    )
endif ()
