/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FloatRect.h"
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>

using WTF::pairIntHash;

namespace WebCore {

Ref<Gradient> Gradient::create(Data&& data)
{
    return adoptRef(*new Gradient(WTFMove(data)));
}

Gradient::Gradient(Data&& data)
    : m_data(WTFMove(data))
{
}

Gradient::~Gradient() = default;

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

void Gradient::addColorStop(Gradient::ColorStop&& stop)
{
    m_stops.append(WTFMove(stop));
    m_stopsSorted = false;
    m_cachedHash = 0;
    stopsChanged();
}

void Gradient::setSortedColorStops(ColorStopVector&& stops)
{
    m_stops = WTFMove(stops);
    m_stopsSorted = true;
    m_cachedHash = 0;
    stopsChanged();
}

void Gradient::sortStops() const
{
    if (m_stopsSorted)
        return;
    m_stopsSorted = true;
    std::stable_sort(m_stops.begin(), m_stops.end(), [] (auto& a, auto& b) {
        return a.offset < b.offset;
    });
}

void Gradient::setSpreadMethod(GradientSpreadMethod spreadMethod)
{
    if (m_spreadMethod == spreadMethod)
        return;
    m_spreadMethod = spreadMethod;
    m_cachedHash = 0;
}

void Gradient::setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation)
{
    if (m_gradientSpaceTransformation == gradientSpaceTransformation)
        return;
    m_gradientSpaceTransformation = gradientSpaceTransformation;
    m_cachedHash = 0;
}

// FIXME: Instead of these add(Hasher) functions, consider using encode functions to compute the hash.

static void add(Hasher& hasher, const Color& color)
{
    // FIXME: We don't want to hash a hash; do better.
    add(hasher, color.hash());
}

static void add(Hasher& hasher, const FloatPoint& point)
{
    add(hasher, point.x(), point.y());
}

static void add(Hasher& hasher, const AffineTransform& transform)
{
    add(hasher, transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f());
}

static void add(Hasher& hasher, const Gradient::ColorStop& stop)
{
    add(hasher, stop.offset, stop.color);
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
    if (!m_cachedHash) {
        sortStops();
        m_cachedHash = computeHash(m_data, m_spreadMethod, m_gradientSpaceTransformation, m_stops);
    }
    return m_cachedHash;
}

}
