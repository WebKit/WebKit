/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "SVGFEConvolveMatrixElement.h"

#include "CommonAtomStrings.h"
#include "FEConvolveMatrix.h"
#include "NodeName.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEConvolveMatrixElement);

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feConvolveMatrixTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEConvolveMatrixElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::orderAttr, &SVGFEConvolveMatrixElement::m_orderX, &SVGFEConvolveMatrixElement::m_orderY>();
        PropertyRegistry::registerProperty<SVGNames::kernelMatrixAttr, &SVGFEConvolveMatrixElement::m_kernelMatrix>();
        PropertyRegistry::registerProperty<SVGNames::divisorAttr, &SVGFEConvolveMatrixElement::m_divisor>();
        PropertyRegistry::registerProperty<SVGNames::biasAttr, &SVGFEConvolveMatrixElement::m_bias>();
        PropertyRegistry::registerProperty<SVGNames::targetXAttr, &SVGFEConvolveMatrixElement::m_targetX>();
        PropertyRegistry::registerProperty<SVGNames::targetYAttr, &SVGFEConvolveMatrixElement::m_targetY>();
        PropertyRegistry::registerProperty<SVGNames::edgeModeAttr, EdgeModeType, &SVGFEConvolveMatrixElement::m_edgeMode>();
        PropertyRegistry::registerProperty<SVGNames::kernelUnitLengthAttr, &SVGFEConvolveMatrixElement::m_kernelUnitLengthX, &SVGFEConvolveMatrixElement::m_kernelUnitLengthY>();
        PropertyRegistry::registerProperty<SVGNames::preserveAlphaAttr, &SVGFEConvolveMatrixElement::m_preserveAlpha>();
    });
}

Ref<SVGFEConvolveMatrixElement> SVGFEConvolveMatrixElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEConvolveMatrixElement(tagName, document));
}

void SVGFEConvolveMatrixElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::inAttr:
        m_in1->setBaseValInternal(newValue);
        break;
    case AttributeNames::orderAttr: {
        auto result = parseNumberOptionalNumber(newValue);
        if (result && result->first >= 1 && result->second >= 1) {
            m_orderX->setBaseValInternal(result->first);
            m_orderY->setBaseValInternal(result->second);
        } else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing order=\"" + newValue + "\". Filtered element will not be displayed.");
        break;
    }
    case AttributeNames::edgeModeAttr: {
        EdgeModeType propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(newValue);
        if (propertyValue != EdgeModeType::Unknown)
            m_edgeMode->setBaseValInternal<EdgeModeType>(propertyValue);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing edgeMode=\"" + newValue + "\". Filtered element will not be displayed.");
        break;
    }
    case AttributeNames::kernelMatrixAttr:
        m_kernelMatrix->baseVal()->parse(newValue);
        break;
    case AttributeNames::divisorAttr:
        if (float divisor = newValue.toFloat())
            m_divisor->setBaseValInternal(divisor);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing divisor=\"" + newValue + "\". Filtered element will not be displayed.");
        break;
    case AttributeNames::biasAttr:
        m_bias->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::targetXAttr:
        m_targetX->setBaseValInternal(parseInteger<unsigned>(newValue).value_or(0));
        break;
    case AttributeNames::targetYAttr:
        m_targetY->setBaseValInternal(parseInteger<unsigned>(newValue).value_or(0));
        break;
    case AttributeNames::kernelUnitLengthAttr: {
        auto result = parseNumberOptionalNumber(newValue);
        if (result && result->first > 0 && result->second > 0) {
            m_kernelUnitLengthX->setBaseValInternal(result->first);
            m_kernelUnitLengthY->setBaseValInternal(result->second);
        } else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing kernelUnitLength=\"" + newValue + "\". Filtered element will not be displayed.");
        break;
    }
    case AttributeNames::preserveAlphaAttr:
        if (newValue == trueAtom())
            m_preserveAlpha->setBaseValInternal(true);
        else if (newValue == falseAtom())
            m_preserveAlpha->setBaseValInternal(false);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing preserveAlphaAttr=\"" + newValue + "\". Filtered element will not be displayed.");
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEConvolveMatrixElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    auto& effect = downcast<FEConvolveMatrix>(filterEffect);
    switch (attrName.nodeName()) {
    case AttributeNames::edgeModeAttr:
        return effect.setEdgeMode(edgeMode());
    case AttributeNames::divisorAttr:
        return effect.setDivisor(divisor());
    case AttributeNames::biasAttr:
        return effect.setBias(bias());
    case AttributeNames::targetXAttr:
    case AttributeNames::targetYAttr:
        return effect.setTargetOffset(IntPoint(targetX(), targetY()));
    case AttributeNames::kernelUnitLengthAttr:
        return effect.setKernelUnitLength(FloatPoint(kernelUnitLengthX(), kernelUnitLengthY()));
    case AttributeNames::preserveAlphaAttr:
        return effect.setPreserveAlpha(preserveAlpha());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEConvolveMatrixElement::setOrder(float x, float y)
{
    m_orderX->setBaseValInternal(x);
    m_orderY->setBaseValInternal(y);
    updateSVGRendererForElementChange();
}

void SVGFEConvolveMatrixElement::setKernelUnitLength(float x, float y)
{
    m_kernelUnitLengthX->setBaseValInternal(x);
    m_kernelUnitLengthY->setBaseValInternal(y);
    updateSVGRendererForElementChange();
}

bool SVGFEConvolveMatrixElement::isValidTargetXOffset() const
{
    auto orderXValue = hasAttribute(SVGNames::orderAttr) ? orderX() : 3;
    auto targetXValue = hasAttribute(SVGNames::targetXAttr) ? targetX() : static_cast<int>(floorf(orderXValue / 2));
    return targetXValue >= 0 && targetXValue < orderXValue;
}

bool SVGFEConvolveMatrixElement::isValidTargetYOffset() const
{
    auto orderYValue = hasAttribute(SVGNames::orderAttr) ? orderY() : 3;
    auto targetYValue = hasAttribute(SVGNames::targetYAttr) ? targetY() : static_cast<int>(floorf(orderYValue / 2));
    return targetYValue >= 0 && targetYValue < orderYValue;
}

void SVGFEConvolveMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if ((attrName == SVGNames::targetXAttr || attrName == SVGNames::orderAttr) && !isValidTargetXOffset()) {
        InstanceInvalidationGuard guard(*this);
        markFilterEffectForRebuild();
        return;
    }

    if ((attrName == SVGNames::targetYAttr || attrName == SVGNames::orderAttr) && !isValidTargetYOffset()) {
        InstanceInvalidationGuard guard(*this);
        markFilterEffectForRebuild();
        return;
    }

    switch (attrName.nodeName()) {
    case AttributeNames::inAttr:
    case AttributeNames::orderAttr:
    case AttributeNames::kernelMatrixAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::edgeModeAttr:
    case AttributeNames::divisorAttr:
    case AttributeNames::biasAttr:
    case AttributeNames::targetXAttr:
    case AttributeNames::targetYAttr:
    case AttributeNames::kernelUnitLengthAttr:
    case AttributeNames::preserveAlphaAttr: {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        break;
    }
    default:
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        break;
    }
}

RefPtr<FilterEffect> SVGFEConvolveMatrixElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    int orderXValue = 3;
    int orderYValue = 3;
    if (hasAttribute(SVGNames::orderAttr)) {
        orderXValue = orderX();
        orderYValue = orderY();
        // Spec says order must be > 0. Bail if it is not.
        if (orderXValue <= 0 || orderYValue <= 0)
            return nullptr;
    }

    auto& kernelMatrix = this->kernelMatrix();
    size_t kernelMatrixSize = kernelMatrix.items().size();
    // The spec says this is a requirement, and should bail out if fails
    if ((size_t)(orderXValue * orderYValue) != kernelMatrixSize)
        return nullptr;

    if (!isValidTargetXOffset() || !isValidTargetYOffset())
        return nullptr;

    // The spec says the default value is: targetX = floor ( orderX / 2 ))
    int targetXValue = hasAttribute(SVGNames::targetXAttr) ? targetX() : static_cast<int>(floorf(orderXValue / 2));

    // The spec says the default value is: targetY = floor ( orderY / 2 ))
    int targetYValue = hasAttribute(SVGNames::targetYAttr) ? targetY() : static_cast<int>(floorf(orderYValue / 2));

    // The spec says the default kernelUnitLength is 1.0, and a specified length cannot be 0.
    int kernelUnitLengthXValue = 1;
    int kernelUnitLengthYValue = 1;
    if (hasAttribute(SVGNames::kernelUnitLengthAttr)) {
        kernelUnitLengthXValue = kernelUnitLengthX();
        kernelUnitLengthYValue = kernelUnitLengthY();
        if (kernelUnitLengthXValue <= 0 || kernelUnitLengthYValue <= 0)
            return nullptr;
    }

    float divisorValue = divisor();
    if (hasAttribute(SVGNames::divisorAttr) && !divisorValue)
        return nullptr;

    if (!hasAttribute(SVGNames::divisorAttr)) {
        for (size_t i = 0; i < kernelMatrixSize; ++i)
            divisorValue += kernelMatrix.items()[i]->value();
        if (!divisorValue)
            divisorValue = 1;
    }

    return FEConvolveMatrix::create(IntSize(orderXValue, orderYValue), divisorValue, bias(), IntPoint(targetXValue, targetYValue), edgeMode(), FloatPoint(kernelUnitLengthXValue, kernelUnitLengthYValue), preserveAlpha(), kernelMatrix);
}

} // namespace WebCore
