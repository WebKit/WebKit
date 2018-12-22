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
#include "SVGFEColorMatrixElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEColorMatrixElement);

inline SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feColorMatrixTag));
    registerAttributes();
}

Ref<SVGFEColorMatrixElement> SVGFEColorMatrixElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEColorMatrixElement(tagName, document));
}

void SVGFEColorMatrixElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFEColorMatrixElement::m_in1>();
    registry.registerAttribute<SVGNames::typeAttr, ColorMatrixType, &SVGFEColorMatrixElement::m_type>();
    registry.registerAttribute<SVGNames::valuesAttr, &SVGFEColorMatrixElement::m_values>();
}

void SVGFEColorMatrixElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::typeAttr) {
        auto propertyValue = SVGPropertyTraits<ColorMatrixType>::fromString(value);
        if (propertyValue > 0)
            m_type.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    if (name == SVGNames::valuesAttr) {
        SVGNumberListValues newList;
        newList.parse(value);
        m_values.detachAnimatedListWrappers(attributeOwnerProxy(), newList.size());
        m_values.setValue(WTFMove(newList));
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEColorMatrixElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEColorMatrix* colorMatrix = static_cast<FEColorMatrix*>(effect);
    if (attrName == SVGNames::typeAttr)
        return colorMatrix->setType(type());
    if (attrName == SVGNames::valuesAttr)
        return colorMatrix->setValues(values());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEColorMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::typeAttr || attrName == SVGNames::valuesAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEColorMatrixElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    Vector<float> filterValues;
    ColorMatrixType filterType = type();

    // Use defaults if values is empty (SVG 1.1 15.10).
    if (!hasAttribute(SVGNames::valuesAttr)) {
        switch (filterType) {
        case FECOLORMATRIX_TYPE_MATRIX:
            for (size_t i = 0; i < 20; i++)
                filterValues.append((i % 6) ? 0 : 1);
            break;
        case FECOLORMATRIX_TYPE_HUEROTATE:
            filterValues.append(0);
            break;
        case FECOLORMATRIX_TYPE_SATURATE:
            filterValues.append(1);
            break;
        default:
            break;
        }
    } else {
        filterValues = values();
        unsigned size = filterValues.size();

        if ((filterType == FECOLORMATRIX_TYPE_MATRIX && size != 20)
            || (filterType == FECOLORMATRIX_TYPE_HUEROTATE && size != 1)
            || (filterType == FECOLORMATRIX_TYPE_SATURATE && size != 1))
            return nullptr;
    }

    auto effect = FEColorMatrix::create(filter, filterType, filterValues);
    effect->inputEffects().append(input1);
    return WTFMove(effect);
}

} // namespace WebCore
