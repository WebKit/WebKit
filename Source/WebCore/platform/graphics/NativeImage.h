/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformExportMacros.h>
#include <WebCore/PlatformImage.h>
#include <WebCore/RenderingResource.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Color;
class DestinationColorSpace;
class FloatRect;
class GraphicsContext;
class IntSize;
class NativeImageBackend;
struct Headroom;
struct ImagePaintingOptions;

class NativeImage : public ThreadSafeRefCounted<NativeImage> {
    WTF_MAKE_TZONE_ALLOCATED(NativeImage);
public:
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&);
    // Creates a NativeImage that is intended to be drawn once or only few times. Signals the platform to avoid generating any caches for the image.
    static WEBCORE_EXPORT RefPtr<NativeImage> createTransient(PlatformImagePtr&&);

    WEBCORE_EXPORT virtual ~NativeImage();

    WEBCORE_EXPORT virtual const PlatformImagePtr& platformImage() const;
    WEBCORE_EXPORT virtual IntSize size() const;
    WEBCORE_EXPORT virtual bool hasAlpha() const;
    std::optional<Color> singlePixelSolidColor() const;
    WEBCORE_EXPORT virtual DestinationColorSpace colorSpace() const;
    WEBCORE_EXPORT bool hasHDRContent() const;
    WEBCORE_EXPORT Headroom headroom() const;

    void clearSubimages();

    WEBCORE_EXPORT void replacePlatformImage(PlatformImagePtr&&);

#if USE(COORDINATED_GRAPHICS)
    uint64_t uniqueID() const;
#endif

    void addObserver(WeakRef<RenderingResourceObserver>&& observer)
    {
        m_observers.add(WTFMove(observer));
    }

    RenderingResourceIdentifier renderingResourceIdentifier() const
    {
        return m_renderingResourceIdentifier;
    }

protected:
    WEBCORE_EXPORT NativeImage(PlatformImagePtr&&);

    mutable PlatformImagePtr m_platformImage;
    mutable WeakHashSet<RenderingResourceObserver> m_observers;
    RenderingResourceIdentifier m_renderingResourceIdentifier { RenderingResourceIdentifier::generate() };
};

} // namespace WebCore
