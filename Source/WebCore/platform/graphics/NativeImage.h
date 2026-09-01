/*
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2012 Company 100 Inc.
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

#include <WebCore/DecodingOptions.h>
#include <WebCore/GainMap.h>
#include <WebCore/ImageBufferAllocator.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/PixelBufferFormat.h>
#include <WebCore/PlatformImage.h>
#include <WebCore/RenderingResource.h>
#include <wtf/CheckedRef.h>
#include <wtf/Lock.h>
#include <wtf/ScopedLambda.h>
#include <wtf/TZoneMalloc.h>

#if USE(SKIA)
class GrDirectContext;
#endif

#if HAVE(IOSURFACE)
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class Color;
class DestinationColorSpace;
class FloatRect;
class GraphicsContext;
class IntRect;
class IntSize;
class NativeImageBackend;
class PixelBuffer;
struct ConstPixelBufferConversionView;
struct PixelBufferConversionView;
struct ImageOrientation;
struct ImagePaintingOptions;

class NativeImage : public ThreadSafeRefCounted<NativeImage>, public CanMakeThreadSafeCheckedPtr<NativeImage> {
    WTF_MAKE_TZONE_ALLOCATED(NativeImage);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(NativeImage);
public:
#if USE(SKIA)
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, std::optional<GainMap>&&, GrDirectContext* = nullptr);
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, GrDirectContext* = nullptr);
    // Creates a NativeImage that is intended to be drawn once or only few times. Signals the platform to avoid generating any caches for the image.
    static WEBCORE_EXPORT RefPtr<NativeImage> createTransient(PlatformImagePtr&&, GrDirectContext* = nullptr);
#else
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, std::optional<GainMap>&&);
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&);
    // Creates a NativeImage that is intended to be drawn once or only few times. Signals the platform to avoid generating any caches for the image.
    static WEBCORE_EXPORT RefPtr<NativeImage> createTransient(PlatformImagePtr&&);
#endif

#if USE(CG)
    WEBCORE_EXPORT static RefPtr<NativeImage> create(RetainPtr<CVPixelBufferRef>, CGImageAlphaInfo, RetainPtr<CGColorSpaceRef>);
#endif

    WEBCORE_EXPORT virtual ~NativeImage();

    WEBCORE_EXPORT virtual PlatformImagePtr platformImage() const;
    WEBCORE_EXPORT const std::optional<GainMap>& gainMap() const;
    WEBCORE_EXPORT virtual IntSize size() const;
    WEBCORE_EXPORT virtual bool hasAlpha() const;
    WEBCORE_EXPORT size_t sizeInBytes() const;
    WEBCORE_EXPORT std::optional<Color> singlePixelSolidColor() const;
    WEBCORE_EXPORT virtual DestinationColorSpace colorSpace() const;
    WEBCORE_EXPORT bool hasHDRContent() const;
    bool hasHDRGainMap() const { return m_gainMap.has_value(); }
    Headroom baseImageHeadroom() const { return m_baseImageHeadroom; }
    Headroom headroom() const { return m_headroom; }

    RefPtr<NativeImage> rotatedImage(ImageOrientation);

    // Pixel access. All of these read the base image only; a gain map is never applied.
    //
    // Describes the layout of the size() pixels the image already holds, without touching
    // them. nullopt means a borrow is not possible: the image is GPU-backed, or its layout is
    // one that PixelBufferFormat cannot name (indexed, 16-bits-per-component integer, fewer
    // than four components). Requesting this format from withPixels() is what avoids a copy.
    struct PixelSourceInfo {
        PixelBufferFormat format;
        unsigned bytesPerRow;
    };
    WEBCORE_EXPORT std::optional<PixelSourceInfo> pixelSourceInfo() const;

    using PixelSourceFunctor = ScopedLambda<void(const ConstPixelBufferConversionView&)>;

    // Calls the functor with a view of size() pixels, valid for the duration of the call only,
    // escalating only as far as needed:
    //   a) pixels readable -> the image's own memory, format and stride, no copy at all
    //   b) otherwise       -> a scratch buffer filled by a draw or GPU readback
    // `fallbackFormat` applies to (b) only, so the functor must read view.format() rather
    // than assume it. Returns false, without calling the functor, only if both fail.
    WEBCORE_EXPORT bool withPixels(const PixelBufferFormat& fallbackFormat, NOESCAPE const PixelSourceFunctor&) const;

    // Copies `sourceRect` into caller-owned storage at the caller's own bytesPerRow,
    // converting to the destination format. Draws only when the pixels are not readable.
    WEBCORE_EXPORT bool copyPixels(const IntRect& sourceRect, const PixelBufferConversionView& destination) const;

    // As above, into a new tightly packed PixelBuffer.
    WEBCORE_EXPORT RefPtr<PixelBuffer> getPixelBuffer(const PixelBufferFormat&, const IntRect& sourceRect, const ImageBufferAllocator& = ImageBufferAllocator()) const;

    void clearSubimages();

    WEBCORE_EXPORT void replacePlatformImage(PlatformImagePtr&&) const;

#if USE(SKIA) || USE(COORDINATED_GRAPHICS)
    uint64_t uniqueID() const;
#endif

#if USE(SKIA)
    GrDirectContext* grContext() const { return m_grContext; }
#endif

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTF::move(observer));
    }

    RenderingResourceIdentifier renderingResourceIdentifier() const
    {
        return m_renderingResourceIdentifier;
    }

protected:
    WEBCORE_EXPORT NativeImage();
#if USE(SKIA)
    WEBCORE_EXPORT NativeImage(PlatformImagePtr&&, std::optional<GainMap>&&, GrDirectContext*);
#else
    WEBCORE_EXPORT NativeImage(PlatformImagePtr&&, std::optional<GainMap>&&);
#endif

    void computeHeadroom() const WTF_REQUIRES_LOCK(m_lock);

    // Platform hooks behind withPixels(). Both must be callable without holding m_lock,
    // since a functor may re-enter the image.
    //
    // Borrows the platform image's own pixels and invokes the functor. Returns false,
    // without invoking it, when they are not directly readable.
    bool withBorrowedPixels(NOESCAPE const PixelSourceFunctor&) const;
    // Draws or reads back the whole image into `destination` as `format`, tightly packed
    // at `bytesPerRow`.
    bool readPixels(const PixelBufferFormat&, std::span<uint8_t> destination, unsigned bytesPerRow) const;

    mutable Lock m_lock;
    mutable PlatformImagePtr m_platformImage WTF_GUARDED_BY_LOCK(m_lock);
    mutable std::optional<GainMap> m_gainMap;
    mutable Headroom m_baseImageHeadroom { Headroom::None };
    mutable Headroom m_headroom { Headroom::None };
    mutable WeakHashSet<RenderingResourceObserver> m_observers;
    RenderingResourceIdentifier m_renderingResourceIdentifier { RenderingResourceIdentifier::generate() };
#if USE(SKIA)
    GrDirectContext* m_grContext { nullptr };
#endif
};

} // namespace WebCore
