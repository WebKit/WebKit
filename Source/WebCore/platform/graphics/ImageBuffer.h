/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2024 Apple Inc. All rights reserved.
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

#include <WebCore/ImageBufferAllocator.h>
#include <WebCore/ImageBufferBackend.h>
#include <WebCore/ImageBufferFormat.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingMode.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/Function.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include <WebCore/DynamicContentScalingResourceCache.h>
#endif

#if HAVE(IOSURFACE)
#include <WebCore/IOSurface.h>
#include <WebCore/IOSurfacePool.h>
#endif

#if USE(SKIA)
class SkSurface;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class BifurcatedGraphicsContext;
class DynamicContentScalingDisplayList;
class Filter;
class GLFence;
class GraphicsClient;
class ScriptExecutionContext;
class SerializedImageBuffer;

struct ImageBufferCreationContext {
#if HAVE(IOSURFACE)
    RefPtr<IOSurfacePool> surfacePool;
    PlatformDisplayID displayID { 0 };
#endif
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    DynamicContentScalingResourceCache dynamicContentScalingResourceCache;
#endif
    ProcessIdentity resourceOwner;

    ImageBufferCreationContext() = default;
};

struct ImageBufferParameters {
    FloatSize logicalSize;
    float resolutionScale;
    DestinationColorSpace colorSpace;
    ImageBufferFormat bufferFormat;
    RenderingPurpose purpose;
};

class ImageBuffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ImageBuffer> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ImageBuffer, WEBCORE_EXPORT);
public:
    using Parameters = ImageBufferParameters;

    static RefPtr<ImageBuffer> create(const FloatSize& size, RenderingMode mode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat bufferFormat, GraphicsClient* client = nullptr)
    {
        return create(size, mode, purpose, resolutionScale, colorSpace, ImageBufferFormat { bufferFormat }, client);
    }

    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingMode, RenderingPurpose, float resolutionScale, const DestinationColorSpace&, ImageBufferFormat, GraphicsClient* = nullptr);

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferFormat bufferFormat, RenderingPurpose purpose, const ImageBufferCreationContext& creationContext, Arguments&&... arguments)
    {
        Parameters parameters { size, resolutionScale, colorSpace, bufferFormat, purpose };
        auto backendParameters = ImageBuffer::backendParameters(parameters);
        auto backend = BackendType::create(backendParameters, creationContext);
        if (!backend)
            return nullptr;
        auto backendInfo = populateBackendInfo<BackendType>(backendParameters);
        return create<ImageBufferType>(parameters, backendInfo, creationContext, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, const ImageBufferCreationContext& creationContext, std::unique_ptr<ImageBufferBackend>&& backend, Arguments&&... arguments)
    {
        auto backendParameters = backend->parameters();
        auto parameters = Parameters { size, backendParameters.resolutionScale, backendParameters.colorSpace, backendParameters.bufferFormat, backendParameters.purpose };
        auto backendInfo = populateBackendInfo<BackendType>(backendParameters);
        return create<ImageBufferType>(parameters, backendInfo, creationContext, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(Parameters parameters, const ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext& creationContext, std::unique_ptr<ImageBufferBackend>&& backend, Arguments&&... arguments)
    {
        return adoptRef(new ImageBufferType(parameters, backendInfo, creationContext, WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    template<typename BackendType>
    static ImageBufferBackend::Info populateBackendInfo(const ImageBufferBackend::Parameters& parameters)
    {
        return {
            BackendType::renderingMode,
            ImageBufferBackend::calculateBaseTransform(parameters),
            BackendType::calculateMemoryCost(parameters),
        };
    }

    WEBCORE_EXPORT virtual ~ImageBuffer();

    WEBCORE_EXPORT static IntSize calculateBackendSize(FloatSize logicalSize, float resolutionScale);
    WEBCORE_EXPORT static ImageBufferBackendParameters backendParameters(const Parameters&);

    // These functions are used when clamping the ImageBuffer which is created for filter, masker or clipper.
    static bool sizeNeedsClamping(const FloatSize&);
    static bool sizeNeedsClamping(const FloatSize&, FloatSize& scale);
    static FloatSize clampedSize(const FloatSize&);
    static FloatSize clampedSize(const FloatSize&, FloatSize& scale);
    static FloatRect clampedRect(const FloatRect&);

    WEBCORE_EXPORT RefPtr<ImageBuffer> clone() const;

    WEBCORE_EXPORT virtual GraphicsContext& context() const;

    WEBCORE_EXPORT virtual void flushDrawingContext();
    WEBCORE_EXPORT virtual bool flushDrawingContextAsync();

    void prepareForDisplay();

    WEBCORE_EXPORT IntSize backendSize() const;

    virtual void ensureBackendCreated() const { ensureBackend(); }
    bool hasBackend() { return !!backend(); }

    WEBCORE_EXPORT void transferToNewContext(const ImageBufferCreationContext&);

    RenderingResourceIdentifier renderingResourceIdentifier() const { return m_renderingResourceIdentifier; }

    FloatSize logicalSize() const { return m_parameters.logicalSize; }
    IntSize truncatedLogicalSize() const { return IntSize(m_parameters.logicalSize); } // You probably should be calling logicalSize() instead.
    float resolutionScale() const { return m_parameters.resolutionScale; }
    DestinationColorSpace colorSpace() const { return m_parameters.colorSpace; }
    
    RenderingPurpose renderingPurpose() const { return m_parameters.purpose; }
    PixelFormat pixelFormat() const { return m_parameters.bufferFormat.pixelFormat; }
    const Parameters& parameters() const { return m_parameters; }

    RenderingMode renderingMode() const { return m_backendInfo.renderingMode; }
    AffineTransform baseTransform() const { return m_backendInfo.baseTransform; }
    size_t memoryCost() const { return m_backendInfo.memoryCost; }
    const ImageBufferBackend::Info& backendInfo() const { return m_backendInfo; }

    // Returns NativeImage of the current drawing results. Results in an immutable copy of the current back buffer.
    WEBCORE_EXPORT virtual RefPtr<NativeImage> copyNativeImage() const;

    // Returns NativeImage referencing the back buffer. Changes to ImageBuffer might be reflected to the NativeImage.
    // Useful when caller can guarantee the use of the NativeImage ends "immediately", before the next draw to this ImageBuffer.
    WEBCORE_EXPORT virtual RefPtr<NativeImage> createNativeImageReference() const;

    WEBCORE_EXPORT virtual RefPtr<NativeImage> filteredNativeImage(Filter&);
    RefPtr<NativeImage> filteredNativeImage(Filter&, Function<void(GraphicsContext&)> drawCallback);

#if HAVE(IOSURFACE)
    IOSurface* surface();
#endif

#if USE(CAIRO)
    WEBCORE_EXPORT RefPtr<cairo_surface_t> createCairoSurface();
#endif

#if USE(SKIA)
    SkSurface* surface() const;
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WEBCORE_EXPORT virtual std::optional<DynamicContentScalingDisplayList> dynamicContentScalingDisplayList();
#endif

    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate();

    // Returns NativeImage of the current drawing results. Results in an immutable copy of the current back buffer.
    // Caller is responsible for ensuring that the passed reference is the only reference to the ImageBuffer.
    // Has better performance than:
    //     RefPtr<ImageBuffer> buffer = ..;
    //     ASSERT(buffer.hasOneRef());
    //     auto nativeImage = buffer.copyNativeImage();
    //     buffer = nullptr;
    WEBCORE_EXPORT static RefPtr<NativeImage> sinkIntoNativeImage(RefPtr<ImageBuffer>);
    WEBCORE_EXPORT static RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread(RefPtr<ImageBuffer>);
#if USE(SKIA)
    static RefPtr<ImageBuffer> sinkIntoImageBufferForCrossThreadTransfer(RefPtr<ImageBuffer>);
    static RefPtr<ImageBuffer> sinkIntoImageBufferAfterCrossThreadTransfer(RefPtr<ImageBuffer>, std::unique_ptr<GLFence>&&);
#endif
    static std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer(RefPtr<ImageBuffer>&&);
    WEBCORE_EXPORT static RefPtr<SharedBuffer> sinkIntoPDFDocument(RefPtr<ImageBuffer>);

    WEBCORE_EXPORT virtual void convertToLuminanceMask();
    WEBCORE_EXPORT virtual void transformToColorSpace(const DestinationColorSpace& newColorSpace);

    WEBCORE_EXPORT String toDataURL(const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No) const;
    WEBCORE_EXPORT Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No) const;

    WEBCORE_EXPORT static String toDataURL(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No);
    WEBCORE_EXPORT static Vector<uint8_t> toData(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No);

    WEBCORE_EXPORT virtual RefPtr<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, const ImageBufferAllocator& = ImageBufferAllocator()) const;
    WEBCORE_EXPORT virtual void putPixelBuffer(const PixelBufferSourceView&, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied);

    WEBCORE_EXPORT virtual RefPtr<SharedBuffer> sinkIntoPDFDocument();

    WEBCORE_EXPORT bool isInUse() const;
    WEBCORE_EXPORT virtual void releaseGraphicsContext();
    WEBCORE_EXPORT bool setVolatile();
    WEBCORE_EXPORT SetNonVolatileResult setNonVolatile();
    WEBCORE_EXPORT VolatilityState volatilityState() const;
    WEBCORE_EXPORT void setVolatilityState(VolatilityState);
    WEBCORE_EXPORT void setVolatileAndPurgeForTesting();
    WEBCORE_EXPORT virtual std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher();

    // This value increments when the ImageBuffer gets a new backend, which can happen if, for example, the GPU Process exits.
    WEBCORE_EXPORT unsigned backendGeneration() const;

    WEBCORE_EXPORT virtual String debugDescription() const;

    WEBCORE_EXPORT virtual ImageBufferBackendSharing* toBackendSharing();

protected:
    WEBCORE_EXPORT ImageBuffer(ImageBufferParameters, const ImageBufferBackend::Info&, const WebCore::ImageBufferCreationContext&, std::unique_ptr<ImageBufferBackend>&& = nullptr, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT virtual RefPtr<NativeImage> sinkIntoNativeImage();
    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread();
    WEBCORE_EXPORT virtual std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer();

    WEBCORE_EXPORT void setBackend(std::unique_ptr<ImageBufferBackend>&&);
    ImageBufferBackend* backend() const { return m_backend.get(); }
    virtual ImageBufferBackend* ensureBackend() const { return m_backend.get(); }

    Parameters m_parameters;
    ImageBufferBackend::Info m_backendInfo;
    std::unique_ptr<ImageBufferBackend> m_backend;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
    unsigned m_backendGeneration { 0 };
    bool m_hasForcedPurgeForTesting { false };
};

class SerializedImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SerializedImageBuffer, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(SerializedImageBuffer);
public:

    SerializedImageBuffer() = default;
    virtual ~SerializedImageBuffer() = default;

    virtual size_t memoryCost() const = 0;

    WEBCORE_EXPORT static RefPtr<ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer>, GraphicsClient* = nullptr);

    virtual bool isRemoteSerializedImageBufferProxy() const { return false; }

protected:
    virtual RefPtr<ImageBuffer> sinkIntoImageBuffer() = 0;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const ImageBuffer&);

} // namespace WebCore
