/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageBufferAllocator.h"
#include "ImageBufferBackend.h"
#include "RenderingMode.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Filter;
class GraphicsClient;
#if HAVE(IOSURFACE)
class IOSurfacePool;
#endif

enum class ImageBufferOptions : uint8_t {
    Accelerated     = 1 << 0,
    UseDisplayList  = 1 << 1
};

class ImageBuffer : public ThreadSafeRefCounted<ImageBuffer>, public CanMakeWeakPtr<ImageBuffer> {
public:
    struct CreationContext {
        // clang 13.1.6 throws errors if we use default initializers here.
        GraphicsClient* graphicsClient;
#if HAVE(IOSURFACE)
        IOSurfacePool* surfacePool;
#endif
        bool avoidIOSurfaceSizeCheckInWebProcessForTesting = false;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        enum class UseCGDisplayListImageCache : bool { No, Yes };
        UseCGDisplayListImageCache useCGDisplayListImageCache;
#endif

        CreationContext(GraphicsClient* client = nullptr
#if HAVE(IOSURFACE)
            , IOSurfacePool* pool = nullptr
#endif
            , bool avoidCheck = false
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
            , UseCGDisplayListImageCache useCGDisplayListImageCache = UseCGDisplayListImageCache::No
#endif
        )
            : graphicsClient(client)
#if HAVE(IOSURFACE)
            , surfacePool(pool)
#endif
            , avoidIOSurfaceSizeCheckInWebProcessForTesting(avoidCheck)
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
            , useCGDisplayListImageCache(useCGDisplayListImageCache)
#endif
        { }
    };

    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingPurpose, float resolutionScale, const DestinationColorSpace&, PixelFormat, OptionSet<ImageBufferOptions> = { }, const CreationContext& = { });

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingPurpose purpose, const CreationContext& creationContext, Arguments&&... arguments)
    {
        auto parameters = ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
        auto backend = BackendType::create(parameters, creationContext);
        if (!backend)
            return nullptr;
        auto backendInfo = populateBackendInfo<BackendType>(parameters);
        return create<ImageBufferType>(parameters, backendInfo, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, const GraphicsContext& context, RenderingPurpose purpose, Arguments&&... arguments)
    {
        auto parameters = ImageBufferBackend::Parameters { size, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, purpose };
        auto backend = BackendType::create(parameters, context);
        if (!backend)
            return nullptr;
        auto backendInfo = populateBackendInfo<BackendType>(parameters);
        return create<ImageBufferType>(parameters, backendInfo, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& backendInfo, std::unique_ptr<ImageBufferBackend>&& backend, Arguments&&... arguments)
    {
        return adoptRef(new ImageBufferType(parameters, backendInfo, WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    template<typename BackendType>
    static ImageBufferBackend::Info populateBackendInfo(const ImageBufferBackend::Parameters& parameters)
    {
        return {
            BackendType::renderingMode,
            BackendType::canMapBackingStore,
            BackendType::calculateBaseTransform(parameters, BackendType::isOriginAtBottomLeftCorner),
            BackendType::calculateMemoryCost(parameters),
            BackendType::calculateExternalMemoryCost(parameters)
        };
    }

    WEBCORE_EXPORT virtual ~ImageBuffer();

    // These functions are used when clamping the ImageBuffer which is created for filter, masker or clipper.
    static bool sizeNeedsClamping(const FloatSize&);
    static bool sizeNeedsClamping(const FloatSize&, FloatSize& scale);
    static FloatSize clampedSize(const FloatSize&);
    static FloatSize clampedSize(const FloatSize&, FloatSize& scale);
    static FloatRect clampedRect(const FloatRect&);

    RefPtr<ImageBuffer> clone() const;
    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> cloneForDifferentThread();

    WEBCORE_EXPORT virtual GraphicsContext& context() const;
    WEBCORE_EXPORT virtual void flushContext();

    virtual bool prefersPreparationForDisplay() { return false; }
    virtual void flushDrawingContext() { }
    virtual bool flushDrawingContextAsync() { return false; }

    WEBCORE_EXPORT IntSize backendSize() const;

    ImageBufferBackend* backend() const { return m_backend.get(); }
    virtual ImageBufferBackend* ensureBackendCreated() const { return m_backend.get(); }

    RenderingResourceIdentifier renderingResourceIdentifier() const { return m_renderingResourceIdentifier; }

    FloatSize logicalSize() const { return m_parameters.logicalSize; }
    IntSize truncatedLogicalSize() const { return IntSize(m_parameters.logicalSize); } // You probably should be calling logicalSize() instead.
    float resolutionScale() const { return m_parameters.resolutionScale; }
    DestinationColorSpace colorSpace() const { return m_parameters.colorSpace; }
    
    RenderingPurpose renderingPurpose() const { return m_parameters.purpose; }
    PixelFormat pixelFormat() const { return m_parameters.pixelFormat; }
    const ImageBufferBackend::Parameters& parameters() const { return m_parameters; }

    RenderingMode renderingMode() const { return m_backendInfo.renderingMode; }
    bool canMapBackingStore() const { return m_backendInfo.canMapBackingStore; }
    AffineTransform baseTransform() const { return m_backendInfo.baseTransform; }
    size_t memoryCost() const { return m_backendInfo.memoryCost; }
    size_t externalMemoryCost() const { return m_backendInfo.externalMemoryCost; }

    WEBCORE_EXPORT virtual RefPtr<NativeImage> copyNativeImage(BackingStoreCopy = CopyBackingStore) const;
    WEBCORE_EXPORT virtual RefPtr<NativeImage> copyNativeImageForDrawing(BackingStoreCopy = CopyBackingStore) const;
    WEBCORE_EXPORT virtual RefPtr<NativeImage> sinkIntoNativeImage();
    WEBCORE_EXPORT RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const;
    WEBCORE_EXPORT virtual RefPtr<Image> filteredImage(Filter&);
    RefPtr<Image> filteredImage(Filter&, std::function<void(GraphicsContext&)> drawCallback);

#if USE(CAIRO)
    WEBCORE_EXPORT RefPtr<cairo_surface_t> createCairoSurface();
#endif

    static RefPtr<NativeImage> sinkIntoNativeImage(RefPtr<ImageBuffer>);
    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(RefPtr<ImageBuffer>, PreserveResolution = PreserveResolution::No);

    static RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread(RefPtr<ImageBuffer>);

    WEBCORE_EXPORT virtual void draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);
    WEBCORE_EXPORT virtual void drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&);

    WEBCORE_EXPORT virtual void drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);

    static void drawConsuming(RefPtr<ImageBuffer>, GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& = { });

    void clipToMask(GraphicsContext& destContext, const FloatRect& destRect);

    WEBCORE_EXPORT virtual void convertToLuminanceMask();
    WEBCORE_EXPORT virtual void transformToColorSpace(const DestinationColorSpace& newColorSpace);

    WEBCORE_EXPORT String toDataURL(const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No) const;
    WEBCORE_EXPORT Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No) const;

    WEBCORE_EXPORT static String toDataURL(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No);
    WEBCORE_EXPORT static Vector<uint8_t> toData(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No);

    WEBCORE_EXPORT virtual RefPtr<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, const ImageBufferAllocator& = ImageBufferAllocator()) const;
    WEBCORE_EXPORT virtual void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied);

    PlatformLayer* platformLayer() const;
    bool copyToPlatformTexture(GraphicsContextGL&, GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) const;

    WEBCORE_EXPORT bool isInUse() const;
    WEBCORE_EXPORT void releaseGraphicsContext();
    WEBCORE_EXPORT bool setVolatile();
    WEBCORE_EXPORT SetNonVolatileResult setNonVolatile();
    WEBCORE_EXPORT VolatilityState volatilityState() const;
    WEBCORE_EXPORT void setVolatilityState(VolatilityState);

    WEBCORE_EXPORT void clearContents();

    WEBCORE_EXPORT virtual std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher();

protected:
    WEBCORE_EXPORT ImageBuffer(const ImageBufferBackend::Parameters&, const ImageBufferBackend::Info&, std::unique_ptr<ImageBufferBackend>&& = nullptr, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread();
    ImageBufferBackend::Parameters m_parameters;
    ImageBufferBackend::Info m_backendInfo;
    std::unique_ptr<ImageBufferBackend> m_backend;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
};

inline OptionSet<ImageBufferOptions> bufferOptionsForRendingMode(RenderingMode renderingMode)
{
    if (renderingMode == RenderingMode::Accelerated)
        return { ImageBufferOptions::Accelerated };
    return { };
}

} // namespace WebCore
