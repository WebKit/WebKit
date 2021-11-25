/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) Apple Inc. 2017-2021 All rights reserved.
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
#include "FEMorphology.h"

#include "Filter.h"
#include "FEMorphologySoftwareApplier.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEMorphology> FEMorphology::create(MorphologyOperatorType type, float radiusX, float radiusY)
{
    return adoptRef(*new FEMorphology(type, radiusX, radiusY));
}

FEMorphology::FEMorphology(MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect(FilterEffect::Type::FEMorphology)
    , m_type(type)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
{
}

bool FEMorphology::setMorphologyOperator(MorphologyOperatorType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FEMorphology::setRadiusX(float radiusX)
{
    if (m_radiusX == radiusX)
        return false;
    m_radiusX = radiusX;
    return true;
}

bool FEMorphology::setRadiusY(float radiusY)
{
    if (m_radiusY == radiusY)
        return false;
    m_radiusY = radiusY;
    return true;
}

void FEMorphology::determineAbsolutePaintRect(const Filter& filter)
{
    FloatRect paintRect = inputEffect(0)->absolutePaintRect();
    paintRect.inflate(filter.scaledByFilterScale({ m_radiusX, m_radiusY }));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

bool FEMorphology::resultIsAlphaImage() const
{
    return inputEffect(0)->resultIsAlphaImage();
}

bool FEMorphology::platformApplySoftware(const Filter& filter)
{
    return FEMorphologySoftwareApplier(*this).apply(filter, inputFilterImages(), *filterImage());
}

static TextStream& operator<<(TextStream& ts, const MorphologyOperatorType& type)
{
    switch (type) {
    case MorphologyOperatorType::Unknown:
        ts << "UNKNOWN";
        break;
    case MorphologyOperatorType::Erode:
        ts << "ERODE";
        break;
    case MorphologyOperatorType::Dilate:
        ts << "DILATE";
        break;
    }
    return ts;
}

TextStream& FEMorphology::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feMorphology";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " operator=\"" << morphologyOperator() << "\" "
       << "radius=\"" << radiusX() << ", " << radiusY() << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
