/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PointLightSource.h"

#include "Filter.h"
#include "FilterImage.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<PointLightSource> PointLightSource::create(const FloatPoint3D& position)
{
    return adoptRef(*new PointLightSource(position));
}

PointLightSource::PointLightSource(const FloatPoint3D& position)
    : LightSource(LightType::LS_POINT)
    , m_position(position)
{
}

bool PointLightSource::operator==(const PointLightSource& other) const
{
    return LightSource::operator==(other) && m_position == other.m_position;
}

void PointLightSource::initPaintingData(const Filter& filter, const FilterImage& result, PaintingData&) const
{
    auto position = filter.resolvedPoint3D(m_position);
    auto absolutePosition = filter.scaledByFilterScale(position.xy());
    m_bufferPosition.setXY(result.mappedAbsolutePoint(absolutePosition));

    // To scale Z, map a point offset from position in the x direction by z.
    auto absoluteMappedZ = filter.scaledByFilterScale(FloatPoint { position.x() + position.z(), position.y() });
    m_bufferPosition.setZ(result.mappedAbsolutePoint(absoluteMappedZ).x() - m_bufferPosition.x());
}

LightSource::ComputedLightingData PointLightSource::computePixelLightingData(const PaintingData& paintingData, int x, int y, float z) const
{
    FloatPoint3D lightVector = {
        m_bufferPosition.x() - x,
        m_bufferPosition.y() - y,
        m_bufferPosition.z() - z
    };

    return { lightVector, paintingData.initialLightingData.colorVector, lightVector.length() };
}

bool PointLightSource::setX(float x)
{
    if (m_position.x() == x)
        return false;
    m_position.setX(x);
    return true;
}

bool PointLightSource::setY(float y)
{
    if (m_position.y() == y)
        return false;
    m_position.setY(y);
    return true;
}

bool PointLightSource::setZ(float z)
{
    if (m_position.z() == z)
        return false;
    m_position.setZ(z);
    return true;
}

TextStream& PointLightSource::externalRepresentation(TextStream& ts) const
{
    ts << "[type=POINT-LIGHT] ";
    ts << "[position=\"" << position() << "\"]";
    return ts;
}

}; // namespace WebCore
