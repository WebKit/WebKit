/*
 * Copyright (C) 2006-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#include "config.h"
#include "Gradient.h"

#include "ColorSpace.h"
#include "FloatRect.h"
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Gradient);

// Compare two colors as equal if they resolve to the same value after replacing
// CSS 'none' (NaN) components with zero. This is conservative: per the CSS spec
// 'none' takes the neighbor's value, but substituting zero is sufficient to detect
// the common case (e.g., color(srgb none 1 none) vs rgb(0 255 0)) and will never
// incorrectly identify a real gradient as solid.
bool Gradient::resolvedColorsMatch(const Color& a, const Color& b)
{
    if (a == b)
        return true;
    auto [csA, compA] = a.colorSpaceAndResolvedColorComponents();
    auto [csB, compB] = b.colorSpaceAndResolvedColorComponents();
    return csA == csB && compA == compB;
}

Ref<Gradient> Gradient::create(Data&& data, ColorInterpolationMethod colorInterpolationMethod, GradientSpreadMethod spreadMethod, GradientColorStops&& stops, bool isTransient)
{
    return adoptRef(*new Gradient(WTF::move(data), colorInterpolationMethod, spreadMethod, WTF::move(stops), isTransient));
}

Gradient::Gradient(Data&& data, ColorInterpolationMethod colorInterpolationMethod, GradientSpreadMethod spreadMethod, GradientColorStops&& stops, bool isTransient)
    : m_data { WTF::move(data) }
    , m_colorInterpolationMethod { colorInterpolationMethod }
    , m_spreadMethod { spreadMethod }
    , m_stops { WTF::move(stops) }
    , m_isTransient { isTransient }
{
}

Gradient::~Gradient()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyGradient(*this);
}

void Gradient::adjustParametersForTiledDrawing(FloatSize& size, FloatRect& srcRect, const FloatSize& spacing)
{
    if (srcRect.isEmpty())
        return;

    if (!spacing.isZero())
        return;

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            if (data.point0.x() == data.point1.x()) {
                size.setWidth(1);
                srcRect.setWidth(1);
                srcRect.setX(0);
                return;
            }
            if (data.point0.y() != data.point1.y())
                return;

            size.setHeight(1);
            srcRect.setHeight(1);
            srcRect.setY(0);
        },
        [] (const RadialData&) {
        },
        [] (const ConicData&) {
        }
    );
}

bool Gradient::isZeroSize() const
{
    return WTF::switchOn(m_data,
        [] (const LinearData& data) {
            return data.point0.x() == data.point1.x() && data.point0.y() == data.point1.y();
        },
        [] (const RadialData& data) {
            return data.point0.x() == data.point1.x() && data.point0.y() == data.point1.y() && data.startRadius == data.endRadius;
        },
        [] (const ConicData&) {
            return false;
        }
    );
}

void Gradient::addColorStop(GradientColorStop&& stop)
{
    m_stops.addColorStop(WTF::move(stop));
    m_cachedHash = 0;
    m_cachedSolidBands = std::nullopt;
    stopsChanged();
}

static void NODELETE add(Hasher& hasher, const Gradient::LinearData& data)
{
    add(hasher, data.point0, data.point1);
}

static void NODELETE add(Hasher& hasher, const Gradient::RadialData& data)
{
    add(hasher, data.point0, data.point1, data.startRadius, data.endRadius, data.aspectRatio);
}

static void NODELETE add(Hasher& hasher, const Gradient::ConicData& data)
{
    add(hasher, data.point0, data.angleRadians);
}

unsigned Gradient::hash() const
{
    if (!m_cachedHash)
        m_cachedHash = computeHash(m_data, m_colorInterpolationMethod, m_spreadMethod, m_stops.sorted());
    return m_cachedHash;
}

Vector<Gradient::SolidBand> Gradient::extractSolidBands(const GradientColorStops& sortedStops)
{
    auto& stops = sortedStops.stops();
    if (stops.isEmpty())
        return { };

    // Resolve CSS 'none' (NaN) components to zero before storing in bands,
    // so that platform color conversions at draw time produce correct results
    // (e.g., oklch with hue=NaN must become hue=0 before converting to sRGB).
    auto resolvedColor = [](const Color& c) -> Color {
        auto [colorSpace, components] = c.colorSpaceAndResolvedColorComponents();
        return callWithColorType(components, colorSpace, [](const auto& typedColor) {
            return Color(typedColor);
        });
    };

    if (stops.size() == 1)
        return { { 0, 1, resolvedColor(stops[0].color) } };

    size_t bandBoundaryCount = 0;
    for (size_t i = 1; i < stops.size(); ++i) {
        if (stops[i].offset != stops[i - 1].offset) {
            if (!resolvedColorsMatch(stops[i].color, stops[i - 1].color))
                return { };
        } else if (!resolvedColorsMatch(stops[i].color, stops[i - 1].color))
            ++bandBoundaryCount;
    }

    // When all stops resolve to the same color, return a single band so that the fill is rendered
    // with fillRect instead of through the default gradient renderer, which can still dither a
    // single-color gradient and produce pixel differences against a solid fill.
    if (!bandBoundaryCount)
        return { { stops.first().offset, stops.last().offset, resolvedColor(stops[0].color) } };

    Vector<SolidBand> solidBands;
    solidBands.reserveInitialCapacity(bandBoundaryCount + 1);
    Color currentColor = stops[0].color;
    float currentStart = stops[0].offset;

    for (size_t i = 1; i < stops.size(); ++i) {
        if (stops[i].offset == stops[i - 1].offset && !resolvedColorsMatch(stops[i].color, currentColor)) {
            solidBands.append({ currentStart, stops[i].offset, resolvedColor(currentColor) });
            currentColor = stops[i].color;
            currentStart = stops[i].offset;
        }
    }

    solidBands.append({ currentStart, stops.last().offset, resolvedColor(currentColor) });
    return solidBands;
}

const Vector<Gradient::SolidBand>& Gradient::solidBands() const
{
    if (!m_cachedSolidBands)
        m_cachedSolidBands = extractSolidBands(m_stops.sorted());
    return *m_cachedSolidBands;
}

TextStream& operator<<(TextStream& ts, const Gradient& gradient)
{
    WTF::switchOn(gradient.data(),
        [&] (const Gradient::LinearData& data) {
            ts.dumpProperty("p0"_s, data.point0);
            ts.dumpProperty("p1"_s, data.point1);
        },
        [&] (const Gradient::RadialData& data) {
            ts.dumpProperty("p0"_s, data.point0);
            ts.dumpProperty("p1"_s, data.point1);
            ts.dumpProperty("start-radius"_s, data.startRadius);
            ts.dumpProperty("end-radius"_s, data.endRadius);
            ts.dumpProperty("aspect-ratio"_s, data.aspectRatio);
        },
        [&] (const Gradient::ConicData& data) {
            ts.dumpProperty("p0"_s, data.point0);
            ts.dumpProperty("angle-radians"_s, data.angleRadians);
        }
    );
    ts.dumpProperty("color-interpolation-method"_s, gradient.colorInterpolationMethod());
    ts.dumpProperty("spread-method"_s, gradient.spreadMethod());
    ts.dumpProperty("stops"_s, gradient.stops());
    return ts;
}

} // namespace WebCore
