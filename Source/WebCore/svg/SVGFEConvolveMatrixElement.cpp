/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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
#include "FEOffset.h"
#include "NodeDocument.h"
#include "NodeName.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPropertyOwnerRegistry.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGFEConvolveMatrixElement);

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, PropertyRegistry::singleton())
{
    ASSERT(hasTagName(SVGNames::feConvolveMatrixTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEConvolveMatrixElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::orderAttr, &SVGFEConvolveMatrixElement::m_orderX, &SVGFEConvolveMatrixElement::m_orderY>(initialOrderValue, initialOrderValue);
        PropertyRegistry::registerProperty<SVGNames::kernelMatrixAttr, &SVGFEConvolveMatrixElement::m_kernelMatrix>();
        PropertyRegistry::registerProperty<SVGNames::divisorAttr, &SVGFEConvolveMatrixElement::m_divisor>(initialDivisorValue);
        PropertyRegistry::registerProperty<SVGNames::biasAttr, &SVGFEConvolveMatrixElement::m_bias>();
        PropertyRegistry::registerProperty<SVGNames::targetXAttr, &SVGFEConvolveMatrixElement::m_targetX>();
        PropertyRegistry::registerProperty<SVGNames::targetYAttr, &SVGFEConvolveMatrixElement::m_targetY>();
        PropertyRegistry::registerProperty<SVGNames::edgeModeAttr, EdgeModeType, &SVGFEConvolveMatrixElement::m_edgeMode>();
        PropertyRegistry::registerProperty<SVGNames::kernelUnitLengthAttr, &SVGFEConvolveMatrixElement::m_kernelUnitLengthX, &SVGFEConvolveMatrixElement::m_kernelUnitLengthY>();
        PropertyRegistry::registerProperty<SVGNames::preserveAlphaAttr, &SVGFEConvolveMatrixElement::m_preserveAlpha>();
    }
}

Ref<SVGFEConvolveMatrixElement> SVGFEConvolveMatrixElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEConvolveMatrixElement(tagName, document));
}

void SVGFEConvolveMatrixElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::inAttr:
        Ref { m_in1 }->setBaseValInternal(newValue);
        break;
    case AttributeNames::orderAttr: {
        // An order that does not parse or is below 1 is invalid and reverts to the initial value,
        // like an absent order. A single invalid component makes the whole attribute invalid, so
        // both components revert.
        if (newValue.isEmpty()) {
            m_orderX->setBaseValInternal(std::nullopt);
            m_orderY->setBaseValInternal(std::nullopt);
            break;
        }
        auto result = parseNumberOptionalNumber(newValue);
        if (!result || result->first < 1 || result->second < 1) {
            m_orderX->setBaseValInternal(std::nullopt);
            m_orderY->setBaseValInternal(std::nullopt);
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing order=\""_s, newValue, "\". Using the initial value 3."_s));
            break;
        }
        m_orderX->setBaseValInternal(result->first);
        m_orderY->setBaseValInternal(result->second);
        break;
    }
    case AttributeNames::edgeModeAttr:
        if (!protect(m_edgeMode)->parseBaseVal<EdgeModeType>(*this, newValue) && !newValue.isNull())
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing edgeMode=\""_s, newValue, "\". Using the initial value duplicate."_s));
        break;
    case AttributeNames::kernelMatrixAttr:
        if (!protect(protect(m_kernelMatrix)->baseVal())->parse(newValue))
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing kernelMatrix=\""_s, newValue, "\". The filter primitive acts as a pass through filter."_s));
        break;
    case AttributeNames::divisorAttr: {
        auto result = parseNumber(newValue);
        m_divisor->setBaseValInternal(result);
        if (!newValue.isEmpty() && (!result || !*result))
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing divisor=\""_s, newValue, "\". Using the default value."_s));
        break;
    }
    case AttributeNames::biasAttr:
        m_bias->setBaseValInternal(parseNumber(newValue));
        break;
    case AttributeNames::targetXAttr:
    case AttributeNames::targetYAttr: {
        // Out of range values are kept, and effectiveTargetOffset() replaces them. A value that is
        // too large depends on order, so only values that do not parse or are negative are reported.
        auto result = parseInteger<int>(newValue);
        Ref target = name == SVGNames::targetXAttr ? m_targetX : m_targetY;
        target->setBaseValInternal(result);
        if (!newValue.isEmpty() && (!result || *result < 0))
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing "_s, name.localName(), "=\""_s, newValue, "\". Using the default value."_s));
        break;
    }
    case AttributeNames::kernelUnitLengthAttr: {
        auto result = parseNumberOptionalNumber(newValue);
        if (!result) {
            m_kernelUnitLengthX->setBaseValInternal(std::nullopt);
            m_kernelUnitLengthY->setBaseValInternal(std::nullopt);
            if (!newValue.isEmpty())
                protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing kernelUnitLength=\""_s, newValue, "\". Using the default value."_s));
            break;
        }
        m_kernelUnitLengthX->setBaseValInternal(result->first);
        m_kernelUnitLengthY->setBaseValInternal(result->second);
        if (result->first <= 0 || result->second <= 0)
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing kernelUnitLength=\""_s, newValue, "\". Using the default value."_s));
        break;
    }
    case AttributeNames::preserveAlphaAttr:
        // Any value other than "true", including an absent attribute, is the initial value false.
        m_preserveAlpha->setBaseValInternal(newValue == trueAtom());
        if (!newValue.isNull() && newValue != trueAtom() && newValue != falseAtom())
            protect(protect(document())->svgExtensions())->reportWarning(makeString("feConvolveMatrix: problem parsing preserveAlpha=\""_s, newValue, "\". Using the initial value false."_s));
        break;
    default:
        break;
    }

    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFEConvolveMatrixElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    // A pass through effect does not depend on these attributes.
    RefPtr effect = dynamicDowncast<FEConvolveMatrix>(filterEffect);
    if (!effect)
        return false;

    switch (attrName.nodeName()) {
    case AttributeNames::edgeModeAttr:
        return effect->setEdgeMode(edgeMode());
    case AttributeNames::divisorAttr:
        return effect->setDivisor(effectiveDivisor());
    case AttributeNames::biasAttr:
        return effect->setBias(bias());
    case AttributeNames::targetXAttr:
    case AttributeNames::targetYAttr:
        return effect->setTargetOffset(effectiveTargetOffset(effect->kernelSize()));
    case AttributeNames::kernelUnitLengthAttr:
        return effect->setKernelUnitLength(effectiveKernelUnitLength());
    case AttributeNames::preserveAlphaAttr:
        return effect->setPreserveAlpha(preserveAlpha());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

IntSize SVGFEConvolveMatrixElement::effectiveOrder() const
{
    // The spec says the values must be integers greater than zero. Invalid values are replaced by
    // the initial value 3. A single invalid component makes the whole attribute invalid, so both
    // components revert.
    auto x = orderX();
    auto y = orderY();
    if (x < 1 || y < 1)
        return { initialOrderValue, initialOrderValue };
    return { x, y };
}

float SVGFEConvolveMatrixElement::effectiveDivisor() const
{
    // The spec says a divisor of 0 is replaced by the default value: the sum of all values in
    // kernelMatrix, or 1 if the sum is 0. A divisor that does not parse is replaced by the default
    // value too, as if it was not specified.
    if (m_divisor->isAnimating() || parseNumber(getAttribute(SVGNames::divisorAttr))) {
        if (auto divisor = this->divisor())
            return divisor;
    }

    Ref kernelMatrix = this->kernelMatrix();
    float sum = 0;
    for (auto& item : kernelMatrix->items())
        sum += item->value();
    return sum ? sum : initialDivisorValue;
}

FloatPoint SVGFEConvolveMatrixElement::effectiveKernelUnitLength() const
{
    // The spec says a negative or zero kernelUnitLength is replaced by the default value of one
    // pixel. A single non-positive component makes the whole attribute invalid, so both
    // components revert.
    if (!hasAttribute(SVGNames::kernelUnitLengthAttr))
        return { 1, 1 };
    auto x = kernelUnitLengthX();
    auto y = kernelUnitLengthY();
    if (x <= 0 || y <= 0)
        return { 1, 1 };
    return { x, y };
}

IntPoint SVGFEConvolveMatrixElement::effectiveTargetOffset(const IntSize& order) const
{
    // The spec says the target must be such that 0 <= targetX < orderX, and that by default the
    // kernel is centered: targetX = floor(orderX / 2). Likewise for targetY. A target that does not
    // parse or is out of range is replaced by the default value, as if it was not specified.
    auto effectiveTarget = [&](const SVGAnimatedInteger& target, const QualifiedName& attributeName, int orderValue) {
        if (target.isAnimating() || parseInteger<int>(getAttribute(attributeName))) {
            auto value = target.currentValue();
            if (value >= 0 && value < orderValue)
                return value;
        }
        return orderValue / 2;
    };
    return {
        effectiveTarget(m_targetX.get(), SVGNames::targetXAttr, order.width()),
        effectiveTarget(m_targetY.get(), SVGNames::targetYAttr, order.height())
    };
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

void SVGFEConvolveMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    switch (attrName.nodeName()) {
    case AttributeNames::inAttr: {
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        break;
    }
    case AttributeNames::orderAttr:
    case AttributeNames::kernelMatrixAttr: {
        // The kernel size and values cannot be updated in place, and they decide between a
        // convolution and a pass through filter. Rebuilding the effect alone does not reach CSS
        // filter clients with the legacy SVG engine, so also update the renderers.
        InstanceInvalidationGuard guard(*this);
        markFilterEffectForRebuild();
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
    auto order = effectiveOrder();
    Ref kernelMatrix = this->kernelMatrix();

    // The spec says the filter primitive acts as a pass through filter if the number of entries
    // in kernelMatrix is not orderX * orderY. The pass through output is equal to the input.
    if (order.unclampedArea() != kernelMatrix->length())
        return FEOffset::create(0, 0);

    return FEConvolveMatrix::create(order, effectiveDivisor(), bias(), effectiveTargetOffset(order), edgeMode(), effectiveKernelUnitLength(), preserveAlpha(), kernelMatrix.get());
}

} // namespace WebCore
