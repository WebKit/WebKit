/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
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

#include "FilterEffect.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

void PointLightSource::initPaintingData(const FilterEffect& filterEffect, PaintingData&)
{
    m_bufferPosition.setXY(filterEffect.mapPointFromUserSpaceToBuffer(m_userSpacePosition.xy()));
    // To scale Z, map a point offset from m_userSpacePosition in the x direction by z.
    FloatPoint mappedZ = filterEffect.mapPointFromUserSpaceToBuffer({ m_userSpacePosition.x() + m_userSpacePosition.z(), m_userSpacePosition.y() });
    m_bufferPosition.setZ(mappedZ.x() - m_bufferPosition.x());
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
    if (m_userSpacePosition.x() == x)
        return false;
    m_userSpacePosition.setX(x);
    return true;
}

bool PointLightSource::setY(float y)
{
    if (m_userSpacePosition.y() == y)
        return false;
    m_userSpacePosition.setY(y);
    return true;
}

bool PointLightSource::setZ(float z)
{
    if (m_userSpacePosition.z() == z)
        return false;
    m_userSpacePosition.setZ(z);
    return true;
}

TextStream& PointLightSource::externalRepresentation(TextStream& ts) const
{
    ts << "[type=POINT-LIGHT] ";
    ts << "[position=\"" << position() << "\"]";
    return ts;
}

}; // namespace WebCore
