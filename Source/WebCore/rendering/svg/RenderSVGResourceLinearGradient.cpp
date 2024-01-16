/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceLinearGradientInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceLinearGradient);

RenderSVGResourceLinearGradient::RenderSVGResourceLinearGradient(SVGLinearGradientElement& element, RenderStyle&& style)
    : RenderSVGResourceGradient(Type::SVGResourceLinearGradient, element, WTFMove(style))
{
}

RenderSVGResourceLinearGradient::~RenderSVGResourceLinearGradient() = default;

void RenderSVGResourceLinearGradient::collectGradientAttributesIfNeeded()
{
    if (m_attributes.has_value())
        return;

    linearGradientElement().synchronizeAllAttributes();

    auto attributes = LinearGradientAttributes { };
    if (linearGradientElement().collectGradientAttributes(attributes))
        m_attributes = WTFMove(attributes);
}

RefPtr<Gradient> RenderSVGResourceLinearGradient::createGradient(const RenderStyle& style)
{
    if (!m_attributes)
        return nullptr;

    auto startPoint = SVGLengthContext::resolvePoint(&linearGradientElement(), m_attributes->gradientUnits(), m_attributes->x1(), m_attributes->y1());
    auto endPoint = SVGLengthContext::resolvePoint(&linearGradientElement(), m_attributes->gradientUnits(), m_attributes->x2(), m_attributes->y2());

    return Gradient::create(
        Gradient::LinearData { startPoint, endPoint },
        { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied },
        platformSpreadMethodFromSVGType(m_attributes->spreadMethod()),
        stopsByApplyingColorFilter(m_attributes->stops(), style),
        RenderingResourceIdentifier::generate()
    );
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
