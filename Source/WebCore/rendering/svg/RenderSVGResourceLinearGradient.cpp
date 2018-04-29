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
#include "RenderSVGResourceLinearGradient.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceLinearGradient);

RenderSVGResourceLinearGradient::RenderSVGResourceLinearGradient(SVGLinearGradientElement& element, RenderStyle&& style)
    : RenderSVGResourceGradient(element, WTFMove(style))
{
}

RenderSVGResourceLinearGradient::~RenderSVGResourceLinearGradient() = default;

bool RenderSVGResourceLinearGradient::collectGradientAttributes()
{
    m_attributes = LinearGradientAttributes();
    return linearGradientElement().collectGradientAttributes(m_attributes);
}

FloatPoint RenderSVGResourceLinearGradient::startPoint(const LinearGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&linearGradientElement(), attributes.gradientUnits(), attributes.x1(), attributes.y1());
}

FloatPoint RenderSVGResourceLinearGradient::endPoint(const LinearGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&linearGradientElement(), attributes.gradientUnits(), attributes.x2(), attributes.y2());
}

void RenderSVGResourceLinearGradient::buildGradient(GradientData* gradientData, const RenderStyle& style) const
{
    gradientData->gradient = Gradient::create(Gradient::LinearData { startPoint(m_attributes), endPoint(m_attributes) });
    gradientData->gradient->setSpreadMethod(platformSpreadMethodFromSVGType(m_attributes.spreadMethod()));
    addStops(gradientData, m_attributes.stops(), style);
}

}
