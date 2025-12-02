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
#include "NodeDocument.h"
#include "NodeName.h"
#include "SVGDocumentExtensions.h"
#include "SVGFilterRenderer.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFEGaussianBlurElement);

inline SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feGaussianBlurTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEGaussianBlurElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::stdDeviationAttr, &SVGFEGaussianBlurElement::m_stdDeviationX, &SVGFEGaussianBlurElement::m_stdDeviationY>();
        PropertyRegistry::registerProperty<SVGNames::edgeModeAttr, EdgeModeType, &SVGFEGaussianBlurElement::m_edgeMode>();
    }
}

Ref<SVGFEGaussianBlurElement> SVGFEGaussianBlurElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEGaussianBlurElement(tagName, document));
}

void SVGFEGaussianBlurElement::setStdDeviation(float x, float y)
{
    Ref { m_stdDeviationX }->setBaseValInternal(x);
    Ref { m_stdDeviationY }->setBaseValInternal(y);
    updateSVGRendererForElementChange();
}

void SVGFEGaussianBlurElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::stdDeviationAttr:
        if (auto result = parseNumberOptionalNumber(newValue)) {
            Ref { m_stdDeviationX }->setBaseValInternal(result->first);
            Ref { m_stdDeviationY }->setBaseValInternal(result->second);
        }
        break;
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::edgeModeAttr: {
        auto propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(*this, newValue);
        if (propertyValue != EdgeModeType::Unknown)
            Ref { m_edgeMode }->setBaseValInternal<EdgeModeType>(propertyValue);
        else
            protectedDocument()->checkedSVGExtensions()->reportWarning(makeString("feGaussianBlur: problem parsing edgeMode=\""_s, newValue, "\". Filtered element will not be displayed."_s));
        break;
    }
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFEGaussianBlurElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::stdDeviationAttr: {
        if (stdDeviationX() < 0 || stdDeviationY() < 0) {
            InstanceInvalidationGuard guard(*this);
            markFilterEffectForRebuild();
            return;
        }
        [[fallthrough]];
    }
    case AttributeNames::edgeModeAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
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
    auto stdDeviation = SVGFilterRenderer::calculateResolvedSize({ stdDeviationX(), stdDeviationY() }, targetBoundingBox, primitiveUnits);
    return FEGaussianBlur::calculateOutsets(stdDeviation);
}

RefPtr<FilterEffect> SVGFEGaussianBlurElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    return FEGaussianBlur::create(stdDeviationX(), stdDeviationY(), edgeMode());
}

} // namespace WebCore
