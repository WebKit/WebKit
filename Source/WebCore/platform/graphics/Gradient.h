/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Torch Mobile, Inc.
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
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GradientColorStops.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/RenderingResource.h>
#include <atomic>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if USE(SKIA)
#include <skia/core/SkShader.h>
#endif

#if USE(CG)
#include <WebCore/GradientRendererCG.h>
#include <wtf/Lock.h>
#endif

#if USE(CG)
typedef struct CGContext* CGContextRef;
#endif

#if USE(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class AffineTransform;
class FloatRect;
class GraphicsContext;

// Note: currently this class is not usable from multiple threads due to mutating interface.
class Gradient final : public ThreadSafeRefCounted<Gradient> {
public:
    struct LinearData {
        FloatPoint point0;
        FloatPoint point1;
    };

    struct RadialData {
        FloatPoint point0;
        FloatPoint point1;
        float startRadius;
        float endRadius;
        float aspectRatio; // For elliptical gradient, width / height.
    };

    struct ConicData {
        FloatPoint point0;
        float angleRadians;
    };

    using Data = Variant<LinearData, RadialData, ConicData>;

    // isTransient may affect backend rendering implementation caching decisions.
    // Transient instances may be assumed to be drawn only few times or seldomly and as such the backend
    // may not persist caches related to the instance.
    static Ref<const Gradient> create(Data&&, ColorInterpolationMethod, GradientSpreadMethod = GradientSpreadMethod::Pad, GradientColorStops&& = { }, bool isTransient = true);
    WEBCORE_EXPORT static Ref<const Gradient> create(Data&&, ColorInterpolationMethod, GradientSpreadMethod, SortedGradientColorStops&&, bool isTransient = true);

    WEBCORE_EXPORT ~Gradient();

    const Data& data() const { return m_data; }
    ColorInterpolationMethod colorInterpolationMethod() const { return m_colorInterpolationMethod; }
    GradientSpreadMethod spreadMethod() const { return m_spreadMethod; }
    // Returns stops as sorted.
    const GradientColorStops& stops() const { return m_stops; }
    bool isTransient() const { return m_isTransient; }

    bool isZeroSize() const;

    void fill(GraphicsContext&, const FloatRect&) const;
    void adjustParametersForTiledDrawing(FloatSize&, FloatRect&, const FloatSize& spacing) const;

    unsigned hash() const;

#if USE(CAIRO)
    RefPtr<cairo_pattern_t> createPattern(float globalAlpha, const AffineTransform&) const;
#endif

#if USE(CG)
    void paint(GraphicsContext&) const;
    // If the DestinationColorSpace is present, the gradient may cache a platform renderer using colors converted into this colorspace,
    // which can be more efficient to render since it avoids colorspace conversions when lower level frameworks render the gradient.
    void paint(CGContextRef, std::optional<DestinationColorSpace> = { }) const;
#endif

#if USE(SKIA)
    sk_sp<SkShader> shader(float globalAlpha, const AffineTransform&);
#endif

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTFMove(observer));
    }

private:
    Gradient(Data&&, ColorInterpolationMethod, GradientSpreadMethod, GradientColorStops&&, bool isTransient);

    Data m_data;
    ColorInterpolationMethod m_colorInterpolationMethod;
    GradientSpreadMethod m_spreadMethod;
    GradientColorStops m_stops;
    mutable std::atomic<unsigned> m_cachedHash { 0 };

#if USE(CG)
    mutable Lock m_lock;
    mutable std::optional<GradientRendererCG> m_platformRenderer WTF_GUARDED_BY_LOCK(m_lock);
#endif

    mutable WeakHashSet<RenderingResourceObserver> m_observers;
    bool m_isTransient { true };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Gradient&);

inline Ref<const Gradient> Gradient::create(Data&& data, ColorInterpolationMethod colorInterpolationMethod, GradientSpreadMethod spreadMethod, GradientColorStops&& stops, bool isTransient)
{
    std::ranges::stable_sort(stops, { }, &GradientColorStop::offset);
    return create(WTFMove(data), colorInterpolationMethod, spreadMethod, SortedGradientColorStops { WTFMove(stops) }, isTransient);
}

} // namespace WebCore
