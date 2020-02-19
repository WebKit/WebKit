list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/cairo"
)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "platform/SourcesCairo.txt"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/cairo/BackingStoreBackendCairo.h
    platform/graphics/cairo/BackingStoreBackendCairoImpl.h
    platform/graphics/cairo/BackingStoreBackendCairoX11.h
    platform/graphics/cairo/CairoOperations.h
    platform/graphics/cairo/CairoUtilities.h
    platform/graphics/cairo/GraphicsContextImplCairo.h
    platform/graphics/cairo/ImageBufferCairoBackend.h
    platform/graphics/cairo/ImageBufferCairoGLSurfaceBackend.h
    platform/graphics/cairo/ImageBufferCairoImageSurfaceBackend.h
    platform/graphics/cairo/ImageBufferCairoSurfaceBackend.h
    platform/graphics/cairo/PlatformContextCairo.h
    platform/graphics/cairo/RefPtrCairo.h
)

list(APPEND WebCore_LIBRARIES
    Cairo::Cairo
)
