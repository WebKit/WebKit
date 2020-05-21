/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGStopElement.h"

#include "Document.h"
#include "RenderSVGGradientStop.h"
#include "RenderSVGResource.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGStopElement);

inline SVGStopElement::SVGStopElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::stopTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::offsetAttr, &SVGStopElement::m_offset>();
    });
}

Ref<SVGStopElement> SVGStopElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGStopElement(tagName, document));
}

void SVGStopElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::offsetAttr) {
        if (value.endsWith('%'))
            m_offset->setBaseValInternal(value.string().left(value.length() - 1).toFloat() / 100.0f);
        else
            m_offset->setBaseValInternal(value.toFloat());
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGStopElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::offsetAttr) {
        if (auto renderer = this->renderer()) {
            InstanceInvalidationGuard guard(*this);
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGStopElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGGradientStop>(*this, WTFMove(style));
}

bool SVGStopElement::rendererIsNeeded(const RenderStyle&)
{
    return true;
}

Color SVGStopElement::stopColorIncludingOpacity() const
{
    if (!renderer())
        return Color(Color::transparent, true);

    auto& style = renderer()->style();
    auto& svgStyle = style.svgStyle();
    auto stopColor = style.colorResolvingCurrentColor(svgStyle.stopColor());

    return stopColor.colorWithAlphaMultipliedByUsingAlternativeRounding(svgStyle.stopOpacity());
}

}
