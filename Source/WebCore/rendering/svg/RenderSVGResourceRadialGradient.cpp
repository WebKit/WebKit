/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG)
#include "RenderSVGResourceRadialGradient.h"

#include "RadialGradientAttributes.h"
#include "SVGRadialGradientElement.h"

namespace WebCore {

RenderSVGResourceType RenderSVGResourceRadialGradient::s_resourceType = RadialGradientResourceType;

RenderSVGResourceRadialGradient::RenderSVGResourceRadialGradient(SVGRadialGradientElement* node)
    : RenderSVGResourceGradient(node)
{
}

RenderSVGResourceRadialGradient::~RenderSVGResourceRadialGradient()
{
}

bool RenderSVGResourceRadialGradient::collectGradientAttributes(SVGGradientElement* gradientElement)
{
    m_attributes = RadialGradientAttributes();
    return static_cast<SVGRadialGradientElement*>(gradientElement)->collectGradientAttributes(m_attributes);
}

FloatPoint RenderSVGResourceRadialGradient::centerPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(static_cast<const SVGElement*>(node()), attributes.gradientUnits(), attributes.cx(), attributes.cy());
}

FloatPoint RenderSVGResourceRadialGradient::focalPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(static_cast<const SVGElement*>(node()), attributes.gradientUnits(), attributes.fx(), attributes.fy());
}

float RenderSVGResourceRadialGradient::radius(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolveLength(static_cast<const SVGElement*>(node()), attributes.gradientUnits(), attributes.r());
}

void RenderSVGResourceRadialGradient::adjustFocalPointIfNeeded(float radius, const FloatPoint& centerPoint, FloatPoint& focalPoint) const
{
    // Eventually adjust focal points, as described below.
    float deltaX = focalPoint.x() - centerPoint.x();
    float deltaY = focalPoint.y() - centerPoint.y();
    float radiusMax = 0.99f * radius;

    // Spec: If (fx, fy) lies outside the circle defined by (cx, cy) and r, set
    // (fx, fy) to the point of intersection of the line through (fx, fy) and the circle.
    // We scale the radius by 0.99 to match the behavior of FireFox.
    if (sqrt(deltaX * deltaX + deltaY * deltaY) <= radiusMax)
        return;

    float angle = atan2f(deltaY, deltaX);
    deltaX = cosf(angle) * radiusMax;
    deltaY = sinf(angle) * radiusMax;
    focalPoint = FloatPoint(deltaX + centerPoint.x(), deltaY + centerPoint.y());
}

void RenderSVGResourceRadialGradient::buildGradient(GradientData* gradientData) const
{
    // Determine gradient focal/center points and radius
    float radius = this->radius(m_attributes);
    FloatPoint centerPoint = this->centerPoint(m_attributes);
    FloatPoint focalPoint = this->focalPoint(m_attributes);
    adjustFocalPointIfNeeded(radius, centerPoint, focalPoint);

    gradientData->gradient = Gradient::create(focalPoint,
                                              0, // SVG does not support a "focus radius"
                                              centerPoint,
                                              radius);

    gradientData->gradient->setSpreadMethod(platformSpreadMethodFromSVGType(m_attributes.spreadMethod()));

    addStops(gradientData, m_attributes.stops());
}

}

#endif
