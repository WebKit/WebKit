/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include <WebCore/FloatSize.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/IntSize.h>
#include <WebCore/Path.h>
#include <WebCore/TextFlags.h>
#include <wtf/Forward.h>

#include <wtf/Platform.h>
#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>
typedef struct CGImage *CGImageRef;
#elif PLATFORM(MAC)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSImage;
#elif USE(CAIRO)
#if PLATFORM(WIN)
#include <wtf/win/GDIObject.h>
typedef struct HBITMAP__* HBITMAP;
typedef struct HDC__* HDC;
#endif
#include "RefPtrCairo.h"
#elif USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

class Color;
class GraphicsClient;
class NativeImage;

#if PLATFORM(IOS_FAMILY)
using PlatformDragImage = RetainPtr<CGImageRef>;
#elif PLATFORM(MAC)
using PlatformDragImage = RetainPtr<NSImage>;
#elif USE(CAIRO) && PLATFORM(WIN)
using PlatformDragImage = HBITMAP;
#elif USE(CAIRO)
using PlatformDragImage = RefPtr<cairo_surface_t>;
#elif USE(SKIA)
using PlatformDragImage = sk_sp<SkImage>;
#else
using PlatformDragImage = void*;
#endif

#if PLATFORM(WIN) && USE(CAIRO)
class GraphicsContextCairo;

GDIObject<HBITMAP> allocImage(HDC, IntSize, GraphicsContextCairo** targetRef);
void deallocContext(GraphicsContextCairo*);
#endif

#if PLATFORM(COCOA)
inline constexpr float ColorSwatchCornerRadius = 4;
inline constexpr float ColorSwatchStrokeSize = 4;
inline constexpr float ColorSwatchWidth = 24;
#endif

class DragImage final {
public:
    DragImage() = default;
    explicit DragImage(PlatformDragImage&& platformImage)
        : m_platformImage(WTF::move(platformImage))
    {
    }
    WEBCORE_EXPORT DragImage(DragImage&&);
    WEBCORE_EXPORT DragImage& operator=(DragImage&&);
    WEBCORE_EXPORT ~DragImage();

    static DragImage create(NativeImage&, GraphicsClient* = nullptr, float deviceScaleFactor = 1);
    static DragImage createForColor(const Color&, const FloatRect&, float, Path&);
    static DragImage createIconForCachedImageFilename(const String&);

    explicit operator bool() const { return !!m_platformImage; }
    const PlatformDragImage& platformImage() const LIFETIME_BOUND { return m_platformImage; }
    PlatformDragImage takePlatformImage() { return std::exchange(m_platformImage, { }); }

    IntSize size() const;
    IntPoint dragOffsetForLink() const;
    FloatPoint anchorPointForLink() const;

    void scale(FloatSize);
    void fitToMaxSize(const IntSize& layoutSize, const IntSize& maxSize);
    void adjustForDeviceScaleFactor(float);
    void dissolveToFraction(float);

    static IntPoint platformAdjustDragImageHotSpot(const IntPoint& hotSpot, const IntSize& dragImageSize);

private:
    static void deletePlatformImage(PlatformDragImage);

    PlatformDragImage m_platformImage { };
};

}
