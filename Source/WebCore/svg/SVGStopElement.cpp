/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "LegacyRenderSVGResource.h"
#include "RenderSVGGradientStop.h"
#include "RenderStyle+GettersInlines.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGStopElement);

inline SVGStopElement::SVGStopElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , m_offset { SVGAnimatedNumber::create(this, 0) }
{
    ASSERT(hasTagName(SVGNames::stopTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::offsetAttr, &SVGStopElement::m_offset>();
    }
}

Ref<SVGStopElement> SVGStopElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGStopElement(tagName, document));
}

void SVGStopElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::offsetAttr) {
        auto valueView = StringView(newValue).trim(isASCIIWhitespace<char16_t>);
        bool isPercentage = valueView.endsWith('%');

        if (isPercentage)
            valueView = valueView.left(valueView.length() - 1);

        auto parsedValue = parseNumber(valueView, SuffixSkippingPolicy::DontSkip);
        float value = parsedValue.value_or(0);

        if (parsedValue && isPercentage)
            value /= 100.0f;

        Ref { m_offset }->setBaseValInternal(value);
    }

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGStopElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::offsetAttr);
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGStopElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGGradientStop>(*this, WTF::move(style));
}

bool SVGStopElement::rendererIsNeeded(const RenderStyle&)
{
    return true;
}

Color SVGStopElement::stopColorIncludingOpacity() const
{
    // Return initial value 'black' as per web specification below:
    // https://svgwg.org/svg2-draft/pservers.html#StopColorProperties
    if (!renderer())
        return Color::black;

    CheckedRef style = renderer()->style();
    auto stopColor = style->stopColorResolvingCurrentColor();
    return stopColor.colorWithAlphaMultipliedBy(style->stopOpacity().value.value);
}

}
