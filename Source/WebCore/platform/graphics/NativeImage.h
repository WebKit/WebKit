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
#include <WebCore/ImageTypes.h>
#include <WebCore/IntRect.h>
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
class ColorSpace;
class FloatRect;
class GraphicsContext;
class NativeImageBackend;
class PixelBuffer;
struct ConstPixelBufferConversionView;
struct PixelBufferConversionView;
class PixelBuffer;
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
    WEBCORE_EXPORT static RefPtr<NativeImage> create(Ref<PixelBuffer>&&);

    WEBCORE_EXPORT virtual ~NativeImage();

    WEBCORE_EXPORT virtual PlatformImagePtr platformImage() const;
    WEBCORE_EXPORT const std::optional<GainMap>& gainMap() const;
    WEBCORE_EXPORT virtual IntSize size() const;
    WEBCORE_EXPORT virtual bool hasAlpha() const;
    WEBCORE_EXPORT size_t sizeInBytes() const;
    WEBCORE_EXPORT std::optional<Color> singlePixelSolidColor() const;
    WEBCORE_EXPORT virtual ColorSpace colorSpace() const;
    WEBCORE_EXPORT bool hasHDRContent() const;
    bool hasHDRGainMap() const { return m_gainMap.has_value(); }
    Headroom baseImageHeadroom() const { return m_baseImageHeadroom; }
    Headroom headroom() const { return m_headroom; }

    RefPtr<NativeImage> rotatedImage(ImageOrientation);

    // Pixel access. All of these read the base image only; a gain map is never applied.

    // The format of the pixels the image already holds
    WEBCORE_EXPORT std::optional<PixelBufferFormat> pixelSourceFormat() const;

    using PixelSourceFunctor = ScopedLambda<void(const ConstPixelBufferConversionView&)>;

    bool withPixels(const PixelBufferFormat& fallbackFormat, NOESCAPE const PixelSourceFunctor&) const;
    WEBCORE_EXPORT bool withPixels(const IntRect& sourceRect, const PixelBufferFormat& fallbackFormat, NOESCAPE const PixelSourceFunctor&) const;
    bool copyPixels(const PixelBufferConversionView& destination) const;
    WEBCORE_EXPORT bool copyPixels(const IntRect& sourceRect, const PixelBufferConversionView& destination) const;

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

    // Borrows the platform image's own pixels, narrowed to `sourceRect`, and invokes the
    // functor. Returns false, without invoking it, when they are not directly readable or
    // the rect does not lie within the image.
    bool withBorrowedPixels(const IntRect& sourceRect, NOESCAPE const PixelSourceFunctor&) const;
    // Draws or reads back `sourceRect`, which must lie within the image, into `destination`.
    // The destination must have room for every byte of every row, padding included, since a
    // platform may write a row's whole stride.
    bool readPixels(const IntRect& sourceRect, const PixelBufferConversionView& destination) const;
    // Exactly the formats readPixels() can produce on this platform.
    static bool canReadPixelsTo(const PixelBufferFormat&);
    // readPixels() of `sourceRect` into a scratch buffer of its own, tightly packed, for
    // callers that have no destination of the right shape to read into.
    bool withTemporaryPixels(const PixelBufferFormat&, const IntRect& sourceRect, NOESCAPE const PixelSourceFunctor&) const;

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

inline bool NativeImage::withPixels(const PixelBufferFormat& fallbackFormat, NOESCAPE const PixelSourceFunctor& functor) const
{
    return withPixels({ { }, size() }, fallbackFormat, functor);
}

inline bool NativeImage::copyPixels(const PixelBufferConversionView& destination) const
{
    return copyPixels({ { }, size() }, destination);
}

} // namespace WebCore
