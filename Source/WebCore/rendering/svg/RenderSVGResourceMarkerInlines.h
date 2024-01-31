/*
 * Copyright (C) 2024 Igalia S.L.
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

#pragma once

#include "RenderSVGResourceMarker.h"
#include "SVGMarkerElement.h"

namespace WebCore {

inline SVGMarkerElement& RenderSVGResourceMarker::markerElement() const
{
    return downcast<SVGMarkerElement>(RenderSVGResourceContainer::element());
}

FloatPoint RenderSVGResourceMarker::referencePoint() const
{
    SVGLengthContext lengthContext(&markerElement());
    return { markerElement().refX().value(lengthContext), markerElement().refY().value(lengthContext) };
}

std::optional<float> RenderSVGResourceMarker::angle() const
{
    if (markerElement().orientType() == SVGMarkerOrientAngle)
        return markerElement().orientAngle().value();
    return std::nullopt;
}

SVGMarkerUnitsType RenderSVGResourceMarker::markerUnits() const
{
    return markerElement().markerUnits();
}

bool RenderSVGResourceMarker::hasReverseStart() const
{
    return markerElement().orientType() == SVGMarkerOrientAutoStartReverse;
}

}
