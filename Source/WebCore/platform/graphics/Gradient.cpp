/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<Gradient> Gradient::create(Data&& data, ColorInterpolationMethod colorInterpolationMethod, GradientSpreadMethod spreadMethod, GradientColorStops&& stops, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
{
    return adoptRef(*new Gradient(WTFMove(data), colorInterpolationMethod, spreadMethod, WTFMove(stops), renderingResourceIdentifier));
}

Gradient::Gradient(Data&& data, ColorInterpolationMethod colorInterpolationMethod, GradientSpreadMethod spreadMethod, GradientColorStops&& stops, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : RenderingResource(renderingResourceIdentifier)
    , m_data { WTFMove(data) }
    , m_colorInterpolationMethod { colorInterpolationMethod }
    , m_spreadMethod { spreadMethod }
    , m_stops { WTFMove(stops) }
{
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
    m_stops.addColorStop(WTFMove(stop));
    m_cachedHash = 0;
    stopsChanged();
}

static void add(Hasher& hasher, const Gradient::LinearData& data)
{
    add(hasher, data.point0, data.point1);
}

static void add(Hasher& hasher, const Gradient::RadialData& data)
{
    add(hasher, data.point0, data.point1, data.startRadius, data.endRadius, data.aspectRatio);
}

static void add(Hasher& hasher, const Gradient::ConicData& data)
{
    add(hasher, data.point0, data.angleRadians);
}

unsigned Gradient::hash() const
{
    if (!m_cachedHash)
        m_cachedHash = computeHash(m_data, m_colorInterpolationMethod, m_spreadMethod, m_stops.sorted());
    return m_cachedHash;
}

TextStream& operator<<(TextStream& ts, const Gradient& gradient)
{
    WTF::switchOn(gradient.data(),
        [&] (const Gradient::LinearData& data) {
            ts.dumpProperty("p0", data.point0);
            ts.dumpProperty("p1", data.point1);
        },
        [&] (const Gradient::RadialData& data) {
            ts.dumpProperty("p0", data.point0);
            ts.dumpProperty("p1", data.point1);
            ts.dumpProperty("start-radius", data.startRadius);
            ts.dumpProperty("end-radius", data.endRadius);
            ts.dumpProperty("aspect-ratio", data.aspectRatio);
        },
        [&] (const Gradient::ConicData& data) {
            ts.dumpProperty("p0", data.point0);
            ts.dumpProperty("angle-radians", data.angleRadians);
        }
    );
    ts.dumpProperty("color-interpolation-method", gradient.colorInterpolationMethod());
    ts.dumpProperty("spread-method", gradient.spreadMethod());
    ts.dumpProperty("stops", gradient.stops());
    return ts;
}

} // namespace WebCore
