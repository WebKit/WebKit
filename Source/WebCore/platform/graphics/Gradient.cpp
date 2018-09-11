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

Ref<Gradient> Gradient::create(LinearData&& data)
{
    return adoptRef(*new Gradient(WTFMove(data)));
}

Ref<Gradient> Gradient::create(RadialData&& data)
{
    return adoptRef(*new Gradient(WTFMove(data)));
}

Ref<Gradient> Gradient::create(ConicData&& data)
{
    return adoptRef(*new Gradient(WTFMove(data)));
}

Gradient::Gradient(LinearData&& data)
    : m_data(WTFMove(data))
{
    platformInit();
}

Gradient::Gradient(RadialData&& data)
    : m_data(WTFMove(data))
{
    platformInit();
}
    
Gradient::Gradient(ConicData&& data)
    : m_data(WTFMove(data))
{
    platformInit();
}

Gradient::~Gradient()
{
    platformDestroy();
}

auto Gradient::type() const -> Type
{
    return WTF::switchOn(m_data,
        [] (const LinearData&) {
            return Type::Linear;
        },
        [] (const RadialData&) {
            return Type::Radial;
        },
        [] (const ConicData&) {
            return Type::Conic;
        }
    );
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

void Gradient::addColorStop(float offset, const Color& color)
{
    addColorStop({ offset, color });
}

void Gradient::addColorStop(const Gradient::ColorStop& stop)
{
    m_stops.append(stop);

    m_stopsSorted = false;

    platformDestroy();
    invalidateHash();
}

void Gradient::setSortedColorStops(ColorStopVector&& stops)
{
    m_stops = WTFMove(stops);

    m_stopsSorted = true;

    platformDestroy();
    invalidateHash();
}

static inline bool compareStops(const Gradient::ColorStop& a, const Gradient::ColorStop& b)
{
    return a.offset < b.offset;
}

void Gradient::sortStopsIfNecessary()
{
    if (m_stopsSorted)
        return;

    m_stopsSorted = true;

    if (!m_stops.size())
        return;

    std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
    invalidateHash();
}

bool Gradient::hasAlpha() const
{
    for (const auto& stop : m_stops) {
        if (!stop.color.isOpaque())
            return true;
    }

    return false;
}

void Gradient::setSpreadMethod(GradientSpreadMethod spreadMethod)
{
    // FIXME: Should it become necessary, allow calls to this method after m_gradient has been set.
    ASSERT(m_gradient == 0);

    if (m_spreadMethod == spreadMethod)
        return;

    m_spreadMethod = spreadMethod;

    invalidateHash();
}

void Gradient::setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation)
{
    if (m_gradientSpaceTransformation == gradientSpaceTransformation)
        return;

    m_gradientSpaceTransformation = gradientSpaceTransformation;

    invalidateHash();
}

unsigned Gradient::hash() const
{
    if (m_cachedHash)
        return m_cachedHash;

    struct {
        Type type;
        FloatPoint point0;
        FloatPoint point1;
        float startRadius;
        float endRadius;
        float aspectRatio;
        float angleRadians;
        GradientSpreadMethod spreadMethod;
        AffineTransform gradientSpaceTransformation;
    } parameters;

    // StringHasher requires that the memory it hashes be a multiple of two in size.
    COMPILE_ASSERT(!(sizeof(parameters) % 2), Gradient_parameters_size_should_be_multiple_of_two);
    COMPILE_ASSERT(!(sizeof(ColorStop) % 2), Color_stop_size_should_be_multiple_of_two);
    
    // Ensure that any padding in the struct is zero-filled, so it will not affect the hash value.
    // FIXME: This is asking for trouble, because it is a nontrivial type.
    memset(static_cast<void*>(&parameters), 0, sizeof(parameters));
    
    WTF::switchOn(m_data,
        [&parameters] (const LinearData& data) {
            parameters.point0 = data.point0;
            parameters.point1 = data.point1;
            parameters.startRadius = 0;
            parameters.endRadius = 0;
            parameters.aspectRatio = 0;
            parameters.angleRadians = 0;
            parameters.type = Type::Linear;
        },
        [&parameters] (const RadialData& data) {
            parameters.point0 = data.point0;
            parameters.point1 = data.point1;
            parameters.startRadius = data.startRadius;
            parameters.endRadius = data.endRadius;
            parameters.aspectRatio = data.aspectRatio;
            parameters.angleRadians = 0;
            parameters.type = Type::Radial;
        },
        [&parameters] (const ConicData& data) {
            parameters.point0 = data.point0;
            parameters.point1 = {0, 0};
            parameters.startRadius = 0;
            parameters.endRadius = 0;
            parameters.aspectRatio = 0;
            parameters.angleRadians = data.angleRadians;
            parameters.type = Type::Conic;
        }
    );

    parameters.spreadMethod = m_spreadMethod;
    parameters.gradientSpaceTransformation = m_gradientSpaceTransformation;

    unsigned parametersHash = StringHasher::hashMemory(&parameters, sizeof(parameters));
    unsigned stopHash = StringHasher::hashMemory(m_stops.data(), m_stops.size() * sizeof(ColorStop));

    m_cachedHash = pairIntHash(parametersHash, stopHash);

    return m_cachedHash;
}

}
