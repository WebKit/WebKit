/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGAnimateElementBase.h"

#include "QualifiedName.h"
#include "SVGAttributeAnimationController.h"
#include "SVGElement.h"
#include "SVGLegacyAttributeAnimationController.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimateElementBase);

SVGAnimateElementBase::SVGAnimateElementBase(const QualifiedName& tagName, Document& document)
    : SVGAnimationElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::animateTag) || hasTagName(SVGNames::setTag) || hasTagName(SVGNames::animateColorTag) || hasTagName(SVGNames::animateTransformTag));
}

SVGAnimateElementBase::~SVGAnimateElementBase() = default;

SVGAttributeAnimationControllerBase& SVGAnimateElementBase::attributeAnimationController()
{
    ASSERT(targetElement());
    ASSERT(!hasInvalidCSSAttributeType());

    if (!m_attributeAnimationController) {
        if (targetElement()->isAnimatedAttribute(attributeName()))
            m_attributeAnimationController = std::make_unique<SVGAttributeAnimationController>(*this, *targetElement());
        else
            m_attributeAnimationController = std::make_unique<SVGLegacyAttributeAnimationController>(*this, *targetElement());
    }

    return *m_attributeAnimationController;
}

bool SVGAnimateElementBase::hasValidAttributeType() const
{
    if (!targetElement() || hasInvalidCSSAttributeType())
        return false;

    if (determineAnimatedPropertyType(*targetElement()) != AnimatedUnknown)
        return true;

    return targetElement()->isAnimatedAttribute(attributeName());
}

AnimatedPropertyType SVGAnimateElementBase::determineAnimatedPropertyType(SVGElement& targetElement) const
{
    return SVGAttributeAnimationControllerBase::determineAnimatedPropertyType(*this, targetElement, attributeName());
}

bool SVGAnimateElementBase::isDiscreteAnimator() const
{
    return hasValidAttributeType() && attributeAnimationControllerIfExists() && attributeAnimationControllerIfExists()->isDiscreteAnimator();
}

void SVGAnimateElementBase::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement)
{
    if (!targetElement())
        return;
    attributeAnimationController().calculateAnimatedValue(percentage, repeatCount, resultElement);
}

bool SVGAnimateElementBase::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (!targetElement())
        return false;
    return attributeAnimationController().calculateToAtEndOfDurationValue(toAtEndOfDurationString);
}

bool SVGAnimateElementBase::calculateFromAndToValues(const String& fromString, const String& toString)
{
    if (!targetElement())
        return false;
    return attributeAnimationController().calculateFromAndToValues(fromString, toString);
}

bool SVGAnimateElementBase::calculateFromAndByValues(const String& fromString, const String& byString)
{
    if (!this->targetElement())
        return false;
    return attributeAnimationController().calculateFromAndByValues(fromString, byString);
}

void SVGAnimateElementBase::resetAnimatedType()
{
    if (!targetElement())
        return;
    attributeAnimationController().resetAnimatedType();
}

void SVGAnimateElementBase::clearAnimatedType(SVGElement* targetElement)
{
    if (!targetElement)
        return;

    if (auto* controller = attributeAnimationControllerIfExists())
        controller->clearAnimatedType(targetElement);
}

void SVGAnimateElementBase::applyResultsToTarget()
{
    if (!targetElement())
        return;
    attributeAnimationController().applyResultsToTarget();
}

bool SVGAnimateElementBase::isAdditive() const
{
    if (animationMode() == AnimationMode::By || animationMode() == AnimationMode::FromBy) {
        if (auto* controller = attributeAnimationControllerIfExists()) {
            if (!controller->isAdditive())
                return false;
        }
    }

    return SVGAnimationElement::isAdditive();
}

float SVGAnimateElementBase::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    if (!targetElement())
        return -1;

    return attributeAnimationController().calculateDistance(fromString, toString);
}

void SVGAnimateElementBase::setTargetElement(SVGElement* target)
{
    SVGAnimationElement::setTargetElement(target);
    resetAnimation();
}

void SVGAnimateElementBase::setAttributeName(const QualifiedName& attributeName)
{
    SVGSMILElement::setAttributeName(attributeName);
    resetAnimation();
}

void SVGAnimateElementBase::resetAnimation()
{
    SVGAnimationElement::resetAnimation();
    m_attributeAnimationController = nullptr;
    m_hasInvalidCSSAttributeType = WTF::nullopt;
}

bool SVGAnimateElementBase::hasInvalidCSSAttributeType() const
{
    if (!targetElement())
        return false;

    if (!m_hasInvalidCSSAttributeType)
        m_hasInvalidCSSAttributeType = hasValidAttributeName() && attributeType() == AttributeType::CSS && !isTargetAttributeCSSProperty(targetElement(), attributeName());

    return m_hasInvalidCSSAttributeType.value();
}

} // namespace WebCore
