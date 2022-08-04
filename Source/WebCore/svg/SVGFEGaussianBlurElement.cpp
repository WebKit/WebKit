/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "FEGaussianBlur.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEGaussianBlurElement);

inline SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feGaussianBlurTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEGaussianBlurElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::stdDeviationAttr, &SVGFEGaussianBlurElement::m_stdDeviationX, &SVGFEGaussianBlurElement::m_stdDeviationY>();
        PropertyRegistry::registerProperty<SVGNames::edgeModeAttr, EdgeModeType, &SVGFEGaussianBlurElement::m_edgeMode>();
    });
}

Ref<SVGFEGaussianBlurElement> SVGFEGaussianBlurElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEGaussianBlurElement(tagName, document));
}

void SVGFEGaussianBlurElement::setStdDeviation(float x, float y)
{
    m_stdDeviationX->setBaseValInternal(x);
    m_stdDeviationY->setBaseValInternal(y);
    updateSVGRendererForElementChange();
}

void SVGFEGaussianBlurElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::stdDeviationAttr) {
        if (auto result = parseNumberOptionalNumber(value)) {
            m_stdDeviationX->setBaseValInternal(result->first);
            m_stdDeviationY->setBaseValInternal(result->second);
        }
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::edgeModeAttr) {
        auto propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(value);
        if (propertyValue != EdgeModeType::Unknown)
            m_edgeMode->setBaseValInternal<EdgeModeType>(propertyValue);
        else
            document().accessSVGExtensions().reportWarning("feGaussianBlur: problem parsing edgeMode=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEGaussianBlurElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    if (attrName == SVGNames::stdDeviationAttr && (stdDeviationX() < 0 || stdDeviationY() < 0)) {
        InstanceInvalidationGuard guard(*this);
        markFilterEffectForRebuild();
        return;
    }

    if (attrName == SVGNames::stdDeviationAttr || attrName == SVGNames::edgeModeAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

bool SVGFEGaussianBlurElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feGaussianBlur = downcast<FEGaussianBlur>(effect);

    if (attrName == SVGNames::stdDeviationAttr) {
        bool stdDeviationXChanged = feGaussianBlur.setStdDeviationX(stdDeviationX());
        bool stdDeviationYChanged = feGaussianBlur.setStdDeviationY(stdDeviationY());
        return stdDeviationXChanged || stdDeviationYChanged;
    }

    if (attrName == SVGNames::edgeModeAttr)
        return feGaussianBlur.setEdgeMode(edgeMode());

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGFEGaussianBlurElement::isIdentity() const
{
    return !stdDeviationX() && !stdDeviationY();
}

IntOutsets SVGFEGaussianBlurElement::outsets(const FloatRect& targetBoundingBox, SVGUnitTypes::SVGUnitType primitiveUnits) const
{
    auto stdDeviation = SVGFilter::calculateResolvedSize({ stdDeviationX(), stdDeviationY() }, targetBoundingBox, primitiveUnits);
    return FEGaussianBlur::calculateOutsets(stdDeviation);
}

RefPtr<FilterEffect> SVGFEGaussianBlurElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    return FEGaussianBlur::create(stdDeviationX(), stdDeviationY(), edgeMode());
}

} // namespace WebCore
