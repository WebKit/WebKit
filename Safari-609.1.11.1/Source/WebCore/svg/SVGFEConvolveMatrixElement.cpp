/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "SVGFEConvolveMatrixElement.h"

#include "FilterEffect.h"
#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEConvolveMatrixElement);

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
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

void SVGFEConvolveMatrixElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::orderAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y) && x >= 1 && y >= 1) {
            m_orderX->setBaseValInternal(x);
            m_orderY->setBaseValInternal(y);
        } else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing order=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    if (name == SVGNames::edgeModeAttr) {
        EdgeModeType propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(value);
        if (propertyValue > 0)
            m_edgeMode->setBaseValInternal<EdgeModeType>(propertyValue);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing edgeMode=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    if (name == SVGNames::kernelMatrixAttr) {
        m_kernelMatrix->baseVal()->parse(value);
        return;
    }

    if (name == SVGNames::divisorAttr) {
        float divisor = value.toFloat();
        if (divisor)
            m_divisor->setBaseValInternal(divisor);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing divisor=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }
    
    if (name == SVGNames::biasAttr) {
        m_bias->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::targetXAttr) {
        m_targetX->setBaseValInternal(value.string().toUIntStrict());
        return;
    }

    if (name == SVGNames::targetYAttr) {
        m_targetY->setBaseValInternal(value.string().toUIntStrict());
        return;
    }

    if (name == SVGNames::kernelUnitLengthAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y) && x > 0 && y > 0) {
            m_kernelUnitLengthX->setBaseValInternal(x);
            m_kernelUnitLengthY->setBaseValInternal(y);
        } else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing kernelUnitLength=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    if (name == SVGNames::preserveAlphaAttr) {
        if (value == "true")
            m_preserveAlpha->setBaseValInternal(true);
        else if (value == "false")
            m_preserveAlpha->setBaseValInternal(false);
        else
            document().accessSVGExtensions().reportWarning("feConvolveMatrix: problem parsing preserveAlphaAttr=\"" + value  + "\". Filtered element will not be displayed.");
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEConvolveMatrixElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEConvolveMatrix* convolveMatrix = static_cast<FEConvolveMatrix*>(effect);
    if (attrName == SVGNames::edgeModeAttr)
        return convolveMatrix->setEdgeMode(edgeMode());
    if (attrName == SVGNames::divisorAttr)
        return convolveMatrix->setDivisor(divisor());
    if (attrName == SVGNames::biasAttr)
        return convolveMatrix->setBias(bias());
    if (attrName == SVGNames::targetXAttr)
        return convolveMatrix->setTargetOffset(IntPoint(targetX(), targetY()));
    if (attrName == SVGNames::targetYAttr)
        return convolveMatrix->setTargetOffset(IntPoint(targetX(), targetY()));
    if (attrName == SVGNames::kernelUnitLengthAttr)
        return convolveMatrix->setKernelUnitLength(FloatPoint(kernelUnitLengthX(), kernelUnitLengthY()));
    if (attrName == SVGNames::preserveAlphaAttr)
        return convolveMatrix->setPreserveAlpha(preserveAlpha());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEConvolveMatrixElement::setOrder(float x, float y)
{
    m_orderX->setBaseValInternal(x);
    m_orderY->setBaseValInternal(y);
    invalidate();
}

void SVGFEConvolveMatrixElement::setKernelUnitLength(float x, float y)
{
    m_kernelUnitLengthX->setBaseValInternal(x);
    m_kernelUnitLengthY->setBaseValInternal(y);
    invalidate();
}

void SVGFEConvolveMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::edgeModeAttr || attrName == SVGNames::divisorAttr || attrName == SVGNames::biasAttr || attrName == SVGNames::targetXAttr || attrName == SVGNames::targetYAttr || attrName == SVGNames::kernelUnitLengthAttr || attrName == SVGNames::preserveAlphaAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::orderAttr || attrName == SVGNames::kernelMatrixAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEConvolveMatrixElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    int orderXValue = orderX();
    int orderYValue = orderY();
    if (!hasAttribute(SVGNames::orderAttr)) {
        orderXValue = 3;
        orderYValue = 3;
    }
    // Spec says order must be > 0. Bail if it is not.
    if (orderXValue < 1 || orderYValue < 1)
        return nullptr;
    auto& kernelMatrix = this->kernelMatrix();
    int kernelMatrixSize = kernelMatrix.items().size();
    // The spec says this is a requirement, and should bail out if fails
    if (orderXValue * orderYValue != kernelMatrixSize)
        return nullptr;

    int targetXValue = targetX();
    int targetYValue = targetY();
    if (hasAttribute(SVGNames::targetXAttr) && (targetXValue < 0 || targetXValue >= orderXValue))
        return nullptr;
    // The spec says the default value is: targetX = floor ( orderX / 2 ))
    if (!hasAttribute(SVGNames::targetXAttr))
        targetXValue = static_cast<int>(floorf(orderXValue / 2));
    if (hasAttribute(SVGNames::targetYAttr) && (targetYValue < 0 || targetYValue >= orderYValue))
        return nullptr;
    // The spec says the default value is: targetY = floor ( orderY / 2 ))
    if (!hasAttribute(SVGNames::targetYAttr))
        targetYValue = static_cast<int>(floorf(orderYValue / 2));

    // Spec says default kernelUnitLength is 1.0, and a specified length cannot be 0.
    int kernelUnitLengthXValue = kernelUnitLengthX();
    int kernelUnitLengthYValue = kernelUnitLengthY();
    if (!hasAttribute(SVGNames::kernelUnitLengthAttr)) {
        kernelUnitLengthXValue = 1;
        kernelUnitLengthYValue = 1;
    }
    if (kernelUnitLengthXValue <= 0 || kernelUnitLengthYValue <= 0)
        return nullptr;

    float divisorValue = divisor();
    if (hasAttribute(SVGNames::divisorAttr) && !divisorValue)
        return nullptr;
    if (!hasAttribute(SVGNames::divisorAttr)) {
        for (int i = 0; i < kernelMatrixSize; ++i)
            divisorValue += kernelMatrix.items()[i]->value();
        if (!divisorValue)
            divisorValue = 1;
    }

    auto effect = FEConvolveMatrix::create(filter, IntSize(orderXValue, orderYValue), divisorValue, bias(), IntPoint(targetXValue, targetYValue), edgeMode(), FloatPoint(kernelUnitLengthXValue, kernelUnitLengthYValue), preserveAlpha(), kernelMatrix);
    effect->inputEffects().append(input1);
    return effect;
}

} // namespace WebCore
