/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGFEGaussianBlurElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEGaussianBlurElement);

inline SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feGaussianBlurTag));
    registerAttributes();
}

Ref<SVGFEGaussianBlurElement> SVGFEGaussianBlurElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEGaussianBlurElement(tagName, document));
}

const AtomicString& SVGFEGaussianBlurElement::stdDeviationXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFEGaussianBlurElement::stdDeviationYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFEGaussianBlurElement::setStdDeviation(float x, float y)
{
    m_stdDeviationX.setValue(x);
    m_stdDeviationY.setValue(y);
    invalidate();
}

void SVGFEGaussianBlurElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFEGaussianBlurElement::m_in1>();
    registry.registerAttribute<SVGNames::stdDeviationAttr,
        &SVGFEGaussianBlurElement::stdDeviationXIdentifier, &SVGFEGaussianBlurElement::m_stdDeviationX,
        &SVGFEGaussianBlurElement::stdDeviationYIdentifier, &SVGFEGaussianBlurElement::m_stdDeviationY>();
    registry.registerAttribute<SVGNames::edgeModeAttr, EdgeModeType, &SVGFEGaussianBlurElement::m_edgeMode>();
}

void SVGFEGaussianBlurElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::stdDeviationAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_stdDeviationX.setValue(x);
            m_stdDeviationY.setValue(y);
        }
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    if (name == SVGNames::edgeModeAttr) {
        auto propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(value);
        if (propertyValue > 0)
            m_edgeMode.setValue(propertyValue);
        else
            document().accessSVGExtensions().reportWarning("feGaussianBlur: problem parsing edgeMode=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEGaussianBlurElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEGaussianBlurElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    auto effect = FEGaussianBlur::create(filter, stdDeviationX(), stdDeviationY(), edgeMode());
    effect->inputEffects().append(input1);
    return WTFMove(effect);
}

}
