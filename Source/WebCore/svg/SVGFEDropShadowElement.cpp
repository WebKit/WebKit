/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "SVGFEDropShadowElement.h"

#include "RenderStyle.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEDropShadowElement);

inline SVGFEDropShadowElement::SVGFEDropShadowElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feDropShadowTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEDropShadowElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::dxAttr, &SVGFEDropShadowElement::m_dx>();
        PropertyRegistry::registerProperty<SVGNames::dyAttr, &SVGFEDropShadowElement::m_dy>();
        PropertyRegistry::registerProperty<SVGNames::stdDeviationAttr, &SVGFEDropShadowElement::m_stdDeviationX, &SVGFEDropShadowElement::m_stdDeviationY>();
    });
}

Ref<SVGFEDropShadowElement> SVGFEDropShadowElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDropShadowElement(tagName, document));
}

void SVGFEDropShadowElement::setStdDeviation(float x, float y)
{
    m_stdDeviationX->setBaseValInternal(x);
    m_stdDeviationY->setBaseValInternal(y);
    invalidate();
}

void SVGFEDropShadowElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::stdDeviationAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_stdDeviationX->setBaseValInternal(x);
            m_stdDeviationY->setBaseValInternal(y);
        }
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::dxAttr) {
        m_dx->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::dyAttr) {
        m_dy->setBaseValInternal(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEDropShadowElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEDropShadowElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    const SVGRenderStyle& svgStyle = renderer->style().svgStyle();
    
    Color color = renderer->style().colorByApplyingColorFilter(svgStyle.floodColor());
    float opacity = svgStyle.floodOpacity();

    auto input1 = filterBuilder->getEffectById(in1());
    if (!input1)
        return nullptr;

    auto effect = FEDropShadow::create(filter, stdDeviationX(), stdDeviationY(), dx(), dy(), color, opacity);
    effect->inputEffects().append(input1);
    return effect;
}

}
