/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "SVGAttributeAnimator.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimateElementBase);

SVGAnimateElementBase::SVGAnimateElementBase(const QualifiedName& tagName, Document& document)
    : SVGAnimationElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::animateTag)
        || hasTagName(SVGNames::setTag)
        || hasTagName(SVGNames::animateColorTag)
        || hasTagName(SVGNames::animateTransformTag));
}

SVGAttributeAnimator* SVGAnimateElementBase::animator() const
{
    ASSERT(targetElement());
    ASSERT(!hasInvalidCSSAttributeType());

    if (!m_animator)
        m_animator = targetElement()->createAnimator(attributeName(), animationMode(), calcMode(), isAccumulated(), isAdditive());

    return m_animator.get();
}

bool SVGAnimateElementBase::hasValidAttributeType() const
{
    if (!targetElement() || hasInvalidCSSAttributeType())
        return false;

    return targetElement()->isAnimatedAttribute(attributeName());
}

bool SVGAnimateElementBase::hasInvalidCSSAttributeType() const
{
    if (!targetElement())
        return false;

    if (!m_hasInvalidCSSAttributeType)
        m_hasInvalidCSSAttributeType = hasValidAttributeName() && attributeType() == AttributeType::CSS && !isTargetAttributeCSSProperty(targetElement(), attributeName());

    return m_hasInvalidCSSAttributeType.value();
}

bool SVGAnimateElementBase::isDiscreteAnimator() const
{
    if (!hasValidAttributeType())
        return false;

    auto* animator = this->animator();
    return animator && animator->isDiscrete();
}

void SVGAnimateElementBase::setTargetElement(SVGElement* targetElement)
{
    SVGAnimationElement::setTargetElement(targetElement);
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
    m_animator = nullptr;
    m_hasInvalidCSSAttributeType = { };
}

bool SVGAnimateElementBase::setFromAndToValues(const String& fromString, const String& toString)
{
    if (!targetElement())
        return false;

    if (auto* animator = this->animator()) {
        animator->setFromAndToValues(*targetElement(), animateRangeString(fromString), animateRangeString(toString));
        return true;
    }
    return false;
}

bool SVGAnimateElementBase::setFromAndByValues(const String& fromString, const String& byString)
{
    if (!targetElement())
        return false;

    if (animationMode() == AnimationMode::By && (!isAdditive() || isDiscreteAnimator()))
        return false;

    if (animationMode() == AnimationMode::FromBy && isDiscreteAnimator())
        return false;

    if (auto* animator = this->animator()) {
        animator->setFromAndByValues(*targetElement(), animateRangeString(fromString), animateRangeString(byString));
        return true;
    }
    return false;
}

bool SVGAnimateElementBase::setToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (!targetElement() || toAtEndOfDurationString.isEmpty())
        return false;

    if (isDiscreteAnimator())
        return true;

    if (auto* animator = this->animator()) {
        animator->setToAtEndOfDurationValue(animateRangeString(toAtEndOfDurationString));
        return true;
    }
    return false;
}

void SVGAnimateElementBase::startAnimation()
{
    if (!targetElement())
        return;

    if (RefPtr protectedAnimator = this->animator())
        protectedAnimator->start(*targetElement());
}

void SVGAnimateElementBase::calculateAnimatedValue(float progress, unsigned repeatCount)
{
    if (!targetElement())
        return;

    ASSERT(progress >= 0 && progress <= 1);
    if (hasTagName(SVGNames::setTag))
        progress = 1;

    if (calcMode() == CalcMode::Discrete)
        progress = progress < 0.5 ? 0 : 1;

    if (RefPtr protectedAnimator = this->animator())
        protectedAnimator->animate(*targetElement(), progress, repeatCount);
}

void SVGAnimateElementBase::applyResultsToTarget()
{
    if (!targetElement())
        return;

    if (auto* animator = this->animator())
        animator->apply(*targetElement());
}

void SVGAnimateElementBase::stopAnimation(SVGElement* targetElement)
{
    if (!targetElement)
        return;

    if (auto* animator = this->animatorIfExists())
        animator->stop(*targetElement);
}

std::optional<float> SVGAnimateElementBase::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    if (!targetElement())
        return { };

    if (auto* animator = this->animator())
        return animator->calculateDistance(*targetElement(), fromString, toString);

    return { };
}

} // namespace WebCore
