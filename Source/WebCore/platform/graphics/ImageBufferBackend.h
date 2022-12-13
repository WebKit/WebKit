/*
 * Copyright (C) 2020-2022 Apple Inc.  All rights reserved.
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

#include "CopyImageOptions.h"
#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "GraphicsTypesGL.h"
#include "ImageBufferAllocator.h"
#include "ImagePaintingOptions.h"
#include "IntRect.h"
#include "PixelBufferFormat.h"
#include "PlatformLayer.h"
#include "RenderingMode.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if USE(CAIRO)
#include "RefPtrCairo.h"
#include <cairo.h>
#endif

namespace WebCore {

struct ImageBufferCreationContext;
class GraphicsContext;
class GraphicsContextGL;
#if HAVE(IOSURFACE)
class IOSurfacePool;
#endif
class Image;
class NativeImage;
class PixelBuffer;
class ProcessIdentity;

enum class PreserveResolution : uint8_t {
    No,
    Yes,
};

enum class SetNonVolatileResult : uint8_t {
    Valid,
    Empty
};

enum class VolatilityState : uint8_t {
    NonVolatile,
    Volatile
};

class ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ThreadSafeImageBufferFlusher);
public:
    ThreadSafeImageBufferFlusher() = default;
    virtual ~ThreadSafeImageBufferFlusher() = default;
    virtual void flush() = 0;
};

class ImageBufferBackendSharing {
public:
    virtual ~ImageBufferBackendSharing() = default;
    virtual bool isImageBufferBackendHandleSharing() const { return false; }
};

class ImageBufferBackend {
public:
    struct Parameters {
        FloatSize logicalSize;
        float resolutionScale;
        DestinationColorSpace colorSpace;
        PixelFormat pixelFormat;
        RenderingPurpose purpose;

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<Parameters> decode(Decoder&);
    };

    struct Info {
        RenderingMode renderingMode;
        bool canMapBackingStore;
        AffineTransform baseTransform;
        size_t memoryCost;
        size_t externalMemoryCost;
    };

    WEBCORE_EXPORT virtual ~ImageBufferBackend();

    WEBCORE_EXPORT static IntSize calculateBackendSize(const Parameters&);
    WEBCORE_EXPORT static size_t calculateMemoryCost(const IntSize& backendSize, unsigned bytesPerRow);
    static size_t calculateExternalMemoryCost(const Parameters&) { return 0; }
    WEBCORE_EXPORT static AffineTransform calculateBaseTransform(const Parameters&, bool originAtBottomLeftCorner);

    virtual GraphicsContext& context() const = 0;
    virtual void flushContext() { }

    virtual IntSize backendSize() const { return { }; }

    virtual void finalizeDrawIntoContext(GraphicsContext&) { }
    virtual RefPtr<NativeImage> copyNativeImage(BackingStoreCopy) const = 0;

    WEBCORE_EXPORT virtual RefPtr<NativeImage> copyNativeImageForDrawing(BackingStoreCopy copyBehavior) const;
    WEBCORE_EXPORT virtual RefPtr<NativeImage> sinkIntoNativeImage();

    virtual void clipToMask(GraphicsContext&, const FloatRect&) { }

    WEBCORE_EXPORT void convertToLuminanceMask();
    virtual void transformToColorSpace(const DestinationColorSpace&) { }

    virtual RefPtr<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect&, const ImageBufferAllocator& = ImageBufferAllocator()) const = 0;
    virtual void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) = 0;

    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual bool copyToPlatformTexture(GraphicsContextGL&, GCGLenum, PlatformGLObject, GCGLenum, bool, bool) const { return false; }

#if USE(CAIRO)
    virtual RefPtr<cairo_surface_t> createCairoSurface() { return nullptr; }
#endif

    virtual bool isInUse() const { return false; }
    virtual void releaseGraphicsContext() { ASSERT_NOT_REACHED(); }

    virtual void transferToNewContext(const ImageBufferCreationContext&) { }

    // Returns true on success.
    virtual bool setVolatile() { return true; }
    virtual SetNonVolatileResult setNonVolatile() { return SetNonVolatileResult::Valid; }
    virtual VolatilityState volatilityState() const { return VolatilityState::NonVolatile; }
    virtual void setVolatilityState(VolatilityState) { }

    virtual std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher() { return nullptr; }

    void applyBaseTransformToContext() const;

    static constexpr bool isOriginAtBottomLeftCorner = false;
    virtual bool originAtBottomLeftCorner() const { return isOriginAtBottomLeftCorner; }

    static constexpr bool canMapBackingStore = true;
    static constexpr RenderingMode renderingMode = RenderingMode::Unaccelerated;

    virtual void ensureNativeImagesHaveCopiedBackingStore() { }

    virtual ImageBufferBackendSharing* toBackendSharing() { return nullptr; }

    virtual void setOwnershipIdentity(const ProcessIdentity&) { }

    virtual void clearContents() { ASSERT_NOT_REACHED(); }

    const Parameters& parameters() { return m_parameters; }

protected:
    WEBCORE_EXPORT ImageBufferBackend(const Parameters&);

    virtual unsigned bytesPerRow() const = 0;

    template<typename T>
    T toBackendCoordinates(T t) const
    {
        static_assert(std::is_same<T, IntPoint>::value || std::is_same<T, IntSize>::value || std::is_same<T, IntRect>::value);
        if (resolutionScale() != 1)
            t.scale(resolutionScale());
        return t;
    }

    IntSize logicalSize() const { return IntSize(m_parameters.logicalSize); }
    float resolutionScale() const { return m_parameters.resolutionScale; }
    const DestinationColorSpace& colorSpace() const { return m_parameters.colorSpace; }
    PixelFormat pixelFormat() const { return m_parameters.pixelFormat; }
    RenderingPurpose renderingPurpose() const { return m_parameters.purpose; }

    IntRect logicalRect() const { return IntRect(IntPoint::zero(), logicalSize()); };
    IntRect backendRect() const { return IntRect(IntPoint::zero(), backendSize()); };

    WEBCORE_EXPORT RefPtr<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, void* data, const ImageBufferAllocator&) const;
    WEBCORE_EXPORT void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat, void* data);

    Parameters m_parameters;
};

template<typename Encoder> void ImageBufferBackend::Parameters::encode(Encoder& encoder) const
{
    encoder << logicalSize;
    encoder << resolutionScale;
    encoder << colorSpace;
    encoder << pixelFormat;
    encoder << purpose;
}

template<typename Decoder> std::optional<ImageBufferBackend::Parameters> ImageBufferBackend::Parameters::decode(Decoder& decoder)
{
    std::optional<FloatSize> logicalSize;
    decoder >> logicalSize;
    if (!logicalSize)
        return std::nullopt;

    std::optional<float> resolutionScale;
    decoder >> resolutionScale;
    if (!resolutionScale)
        return std::nullopt;

    std::optional<DestinationColorSpace> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return std::nullopt;

    std::optional<PixelFormat> pixelFormat;
    decoder >> pixelFormat;
    if (!pixelFormat)
        return std::nullopt;

    std::optional<RenderingPurpose> purpose;
    decoder >> purpose;
    if (!purpose)
        return std::nullopt;

    return { { *logicalSize, *resolutionScale, *colorSpace, *pixelFormat, *purpose } };
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PreserveResolution> {
    using values = EnumValues<
        WebCore::PreserveResolution,
        WebCore::PreserveResolution::No,
        WebCore::PreserveResolution::Yes
    >;
};

} // namespace WTF
