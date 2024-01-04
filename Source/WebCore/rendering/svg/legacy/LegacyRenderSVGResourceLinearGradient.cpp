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
#include "LegacyRenderSVGResourceLinearGradient.h"

#include "LegacyRenderSVGResourceLinearGradientInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceLinearGradient);

LegacyRenderSVGResourceLinearGradient::LegacyRenderSVGResourceLinearGradient(SVGLinearGradientElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceGradient(Type::LegacySVGResourceLinearGradient, element, WTFMove(style))
{
}

LegacyRenderSVGResourceLinearGradient::~LegacyRenderSVGResourceLinearGradient() = default;

bool LegacyRenderSVGResourceLinearGradient::collectGradientAttributes()
{
    m_attributes = LinearGradientAttributes();
    return linearGradientElement().collectGradientAttributes(m_attributes);
}

FloatPoint LegacyRenderSVGResourceLinearGradient::startPoint(const LinearGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&linearGradientElement(), attributes.gradientUnits(), attributes.x1(), attributes.y1());
}

FloatPoint LegacyRenderSVGResourceLinearGradient::endPoint(const LinearGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&linearGradientElement(), attributes.gradientUnits(), attributes.x2(), attributes.y2());
}

Ref<Gradient> LegacyRenderSVGResourceLinearGradient::buildGradient(const RenderStyle& style) const
{
    return Gradient::create(
        Gradient::LinearData { startPoint(m_attributes), endPoint(m_attributes) },
        { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied },
        platformSpreadMethodFromSVGType(m_attributes.spreadMethod()),
        stopsByApplyingColorFilter(m_attributes.stops(), style),
        RenderingResourceIdentifier::generate()
    );
}

}
