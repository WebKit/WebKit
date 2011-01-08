/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(FILTERS)
#include "LightSource.h"

#include "DistantLightSource.h"
#include "PointLightSource.h"
#include "RenderTreeAsText.h"
#include "SpotLightSource.h"
#include <wtf/MathExtras.h>

namespace WebCore {

void PointLightSource::initPaintingData(PaintingData&)
{
}

void PointLightSource::updatePaintingData(PaintingData& paintingData, int x, int y, float z)
{
    paintingData.lightVector.setX(m_position.x() - x);
    paintingData.lightVector.setY(m_position.y() - y);
    paintingData.lightVector.setZ(m_position.z() - z);
    paintingData.lightVectorLength = paintingData.lightVector.length();
}

// spot-light edge darkening depends on an absolute treshold
// according to the SVG 1.1 SE light regression tests
static const float antiAliasTreshold = 0.016f;

void SpotLightSource::initPaintingData(PaintingData& paintingData)
{
    paintingData.privateColorVector = paintingData.colorVector;
    paintingData.directionVector.setX(m_direction.x() - m_position.x());
    paintingData.directionVector.setY(m_direction.y() - m_position.y());
    paintingData.directionVector.setZ(m_direction.z() - m_position.z());
    paintingData.directionVector.normalize();

    if (!m_limitingConeAngle) {
        paintingData.coneCutOffLimit = 0.0f;
        paintingData.coneFullLight = -antiAliasTreshold;
    } else {
        float limitingConeAngle = m_limitingConeAngle;
        if (limitingConeAngle < 0.0f)
            limitingConeAngle = -limitingConeAngle;
        if (limitingConeAngle > 90.0f)
            limitingConeAngle = 90.0f;
        paintingData.coneCutOffLimit = cosf(deg2rad(180.0f - limitingConeAngle));
        paintingData.coneFullLight = paintingData.coneCutOffLimit - antiAliasTreshold;
    }

    // Optimization for common specularExponent values
    if (!m_specularExponent)
        paintingData.specularExponent = 0;
    else if (m_specularExponent == 1.0f)
        paintingData.specularExponent = 1;
    else // It is neither 0.0f nor 1.0f
        paintingData.specularExponent = 2;
}

void SpotLightSource::updatePaintingData(PaintingData& paintingData, int x, int y, float z)
{
    paintingData.lightVector.setX(m_position.x() - x);
    paintingData.lightVector.setY(m_position.y() - y);
    paintingData.lightVector.setZ(m_position.z() - z);
    paintingData.lightVectorLength = paintingData.lightVector.length();

    float cosineOfAngle = (paintingData.lightVector * paintingData.directionVector) / paintingData.lightVectorLength;
    if (cosineOfAngle > paintingData.coneCutOffLimit) {
        // No light is produced, scanlines are not updated
        paintingData.colorVector.setX(0.0f);
        paintingData.colorVector.setY(0.0f);
        paintingData.colorVector.setZ(0.0f);
        return;
    }

    // Set the color of the pixel
    float lightStrength;
    switch (paintingData.specularExponent) {
    case 0:
        lightStrength = 1.0f; // -cosineOfAngle ^ 0 == 1
        break;
    case 1:
        lightStrength = -cosineOfAngle; // -cosineOfAngle ^ 1 == -cosineOfAngle
        break;
    default:
        lightStrength = powf(-cosineOfAngle, m_specularExponent);
        break;
    }

    if (cosineOfAngle > paintingData.coneFullLight)
        lightStrength *= (paintingData.coneCutOffLimit - cosineOfAngle) / (paintingData.coneCutOffLimit - paintingData.coneFullLight);

    if (lightStrength > 1.0f)
        lightStrength = 1.0f;

    paintingData.colorVector.setX(paintingData.privateColorVector.x() * lightStrength);
    paintingData.colorVector.setY(paintingData.privateColorVector.y() * lightStrength);
    paintingData.colorVector.setZ(paintingData.privateColorVector.z() * lightStrength);
}

void DistantLightSource::initPaintingData(PaintingData& paintingData)
{
    float azimuth = deg2rad(m_azimuth);
    float elevation = deg2rad(m_elevation);
    paintingData.lightVector.setX(cosf(azimuth) * cosf(elevation));
    paintingData.lightVector.setY(sinf(azimuth) * cosf(elevation));
    paintingData.lightVector.setZ(sinf(elevation));
    paintingData.lightVectorLength = 1;
}

void DistantLightSource::updatePaintingData(PaintingData&, int, int, float)
{
}

static TextStream& operator<<(TextStream& ts, const FloatPoint3D& p)
{
    ts << "x=" << p.x() << " y=" << p.y() << " z=" << p.z();
    return ts;
}

TextStream& PointLightSource::externalRepresentation(TextStream& ts) const
{
    ts << "[type=POINT-LIGHT] ";
    ts << "[position=\"" << position() << "\"]";
    return ts;
}

TextStream& SpotLightSource::externalRepresentation(TextStream& ts) const
{
    ts << "[type=SPOT-LIGHT] ";
    ts << "[position=\"" << position() << "\"]";
    ts << "[direction=\"" << direction() << "\"]";
    ts << "[specularExponent=\"" << specularExponent() << "\"]";
    ts << "[limitingConeAngle=\"" << limitingConeAngle() << "\"]";
    return ts;
}

TextStream& DistantLightSource::externalRepresentation(TextStream& ts) const
{
    ts << "[type=DISTANT-LIGHT] ";
    ts << "[azimuth=\"" << azimuth() << "\"]";
    ts << "[elevation=\"" << elevation() << "\"]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
