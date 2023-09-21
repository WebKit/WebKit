/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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
#include "PlatformScreen.h"
#include "ProcessIdentity.h"
#include "RenderingMode.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Filter;
class GraphicsClient;
#if HAVE(IOSURFACE)
class IOSurfacePool;
#endif
class ScriptExecutionContext;

enum class ImageBufferOptions : uint8_t {
    Accelerated     = 1 << 0,
    UseDisplayList  = 1 << 1,
    AvoidBackendSizeCheckForTesting = 1 << 2,
};

class SerializedImageBuffer;

struct ImageBufferCreationContext {
#if HAVE(IOSURFACE)
    IOSurfacePool* surfacePool { nullptr };
    PlatformDisplayID displayID { 0 };
#endif
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    enum class UseCGDisplayListImageCache : bool { No, Yes };
    UseCGDisplayListImageCache useCGDisplayListImageCache { UseCGDisplayListImageCache::No };
#endif
    WebCore::ProcessIdentity resourceOwner;

    ImageBufferCreationContext() = default; // To guarantee order in presence of ifdefs, use individual .property to initialize them.
};

class ImageBuffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ImageBuffer> {
public:

    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingPurpose, float resolutionScale, const DestinationColorSpace&, PixelFormat, OptionSet<ImageBufferOptions> = { }, GraphicsClient* graphicsClient = nullptr);

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingPurpose purpose, const ImageBufferCreationContext& creationContext, Arguments&&... arguments)
    {
        auto parameters = ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
        auto backend = BackendType::create(parameters, creationContext);
        if (!backend)
            return nullptr;
        auto backendInfo = populateBackendInfo<BackendType>(parameters);
        return create<ImageBufferType>(parameters, backendInfo, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize&, const GraphicsContext&, RenderingPurpose, Arguments&&...);

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

    WEBCORE_EXPORT RefPtr<ImageBuffer> clone() const;

    WEBCORE_EXPORT virtual GraphicsContext& context() const;

    virtual bool prefersPreparationForDisplay() { return false; }
    WEBCORE_EXPORT virtual void flushDrawingContext();
    WEBCORE_EXPORT virtual bool flushDrawingContextAsync();

    WEBCORE_EXPORT std::unique_ptr<ImageBufferBackend> takeBackend();
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
    const ImageBufferBackend::Info& backendInfo() { return m_backendInfo; }

    // Returns NativeImage of the current drawing results. Results in an immutable copy of the current back buffer.
    WEBCORE_EXPORT virtual RefPtr<NativeImage> copyNativeImage() const;

    // Returns NativeImage referencing the back buffer. Changes to ImageBuffer might be reflected to the NativeImage.
    // Useful when caller can guarantee the use of the NativeImage ends "immediately", before the next draw to this ImageBuffer.
    WEBCORE_EXPORT virtual RefPtr<NativeImage> createNativeImageReference() const;

    WEBCORE_EXPORT RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const;
    WEBCORE_EXPORT virtual RefPtr<Image> filteredImage(Filter&);
    RefPtr<Image> filteredImage(Filter&, std::function<void(GraphicsContext&)> drawCallback);

#if USE(CAIRO)
    WEBCORE_EXPORT RefPtr<cairo_surface_t> createCairoSurface();
#endif

    // Returns NativeImage of the current drawing results. Results in an immutable copy of the current back buffer.
    // Caller is responsible for ensuring that the passed reference is the only reference to the ImageBuffer.
    // Has better performance than:
    //     RefPtr<ImageBuffer> buffer = ..;
    //     ASSERT(buffer.hasOneRef());
    //     auto nativeImage = buffer.copyNativeImage();
    //     buffer = nullptr;
    static RefPtr<NativeImage> sinkIntoNativeImage(RefPtr<ImageBuffer>);
    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(RefPtr<ImageBuffer>, PreserveResolution = PreserveResolution::No);
    static RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread(RefPtr<ImageBuffer>);
    static std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer(RefPtr<ImageBuffer>&&);

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
    WEBCORE_EXPORT virtual std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher();

    // This value increments when the ImageBuffer gets a new backend, which can happen if, for example, the GPU Process exits.
    WEBCORE_EXPORT unsigned backendGeneration() const;

    WEBCORE_EXPORT virtual String debugDescription() const;

protected:
    WEBCORE_EXPORT ImageBuffer(const ImageBufferBackend::Parameters&, const ImageBufferBackend::Info&, std::unique_ptr<ImageBufferBackend>&& = nullptr, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT virtual RefPtr<NativeImage> sinkIntoNativeImage();
    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread();
    WEBCORE_EXPORT virtual std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer();

    WEBCORE_EXPORT void setBackend(std::unique_ptr<ImageBufferBackend>&&);

    ImageBufferBackend::Parameters m_parameters;
    ImageBufferBackend::Info m_backendInfo;
    std::unique_ptr<ImageBufferBackend> m_backend;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
    unsigned m_backendGeneration { 0 };
};

class SerializedImageBuffer {
    WTF_MAKE_NONCOPYABLE(SerializedImageBuffer);
    WTF_MAKE_FAST_ALLOCATED;
public:

    SerializedImageBuffer() = default;
    virtual ~SerializedImageBuffer() = default;

    virtual size_t memoryCost() = 0;

    WEBCORE_EXPORT static RefPtr<ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer>, GraphicsClient* = nullptr);

    virtual bool isRemoteSerializedImageBufferProxy() const { return false; }

protected:
    virtual RefPtr<ImageBuffer> sinkIntoImageBuffer() = 0;
};

inline OptionSet<ImageBufferOptions> bufferOptionsForRendingMode(RenderingMode renderingMode)
{
    if (renderingMode == RenderingMode::Accelerated)
        return { ImageBufferOptions::Accelerated };
    return { };
}

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const ImageBuffer&);

} // namespace WebCore
