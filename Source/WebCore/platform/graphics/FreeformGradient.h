/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/Color.h>
#include <WebCore/ColorComponents.h>
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntSize.h>
#include <WebCore/RenderingResource.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class ColorSpace;
class NativeImage;

class FreeformGradient final : public ThreadSafeRefCounted<FreeformGradient>, public CanMakeThreadSafeCheckedPtr<FreeformGradient> {
    WTF_MAKE_TZONE_ALLOCATED(FreeformGradient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FreeformGradient);
public:
    struct Stop {
        FloatPoint position;
        Color color;
        float weight { 0 };
    };

    WEBCORE_EXPORT static Ref<FreeformGradient> create(ColorInterpolationMethod, Vector<Stop>&&);
    WEBCORE_EXPORT static std::optional<Ref<FreeformGradient>> tryCreate(ColorInterpolationMethod, Vector<Stop>&&);

    WEBCORE_EXPORT ~FreeformGradient();

    const ColorInterpolationMethod& colorInterpolationMethod() const LIFETIME_BOUND { return m_colorInterpolationMethod; }
    const Vector<Stop>& stops() const LIFETIME_BOUND { return m_stops; }

    WEBCORE_EXPORT static std::optional<ColorSpaceName> renderingColorSpace(const ColorSpace&);

    WEBCORE_EXPORT ResolvedPremultiplied<ColorComponents<float, 4>> premultipliedColor(FloatPoint, ColorSpaceName) const;

    RefPtr<NativeImage> renderedImage(const IntSize& pixelSize, float scale, const ColorSpace&) const;
    void setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&&) const;

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTF::move(observer));
    }

private:
    FreeformGradient(ColorInterpolationMethod, Vector<Stop>&&);

    ColorInterpolationMethod m_colorInterpolationMethod;
    Vector<Stop> m_stops;

    mutable Lock m_renderedImageLock;
    mutable IntSize m_renderedImagePixelSize WTF_GUARDED_BY_LOCK(m_renderedImageLock);
    mutable float m_renderedImageScale WTF_GUARDED_BY_LOCK(m_renderedImageLock) { 0 };
    mutable RefPtr<NativeImage> m_renderedImage WTF_GUARDED_BY_LOCK(m_renderedImageLock);

    mutable WeakHashSet<RenderingResourceObserver> m_observers;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const FreeformGradient::Stop&);
WEBCORE_EXPORT TextStream& operator<<(TextStream&, const FreeformGradient&);

} // namespace WebCore
