/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGAnimateElement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "QualifiedName.h"
#include "RenderObject.h"
#include "SVGAnimatorFactory.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document* document)
    : SVGAnimationElement(tagName, document)
    , m_animatedPropertyType(AnimatedString)
{
    ASSERT(hasTagName(SVGNames::animateTag) || hasTagName(SVGNames::setTag) || hasTagName(SVGNames::animateColorTag) || hasTagName(SVGNames::animateTransformTag));
}

PassRefPtr<SVGAnimateElement> SVGAnimateElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGAnimateElement(tagName, document));
}

SVGAnimateElement::~SVGAnimateElement()
{
}

bool SVGAnimateElement::hasValidAttributeType()
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;
    
    return m_animatedPropertyType != AnimatedUnknown;
}

AnimatedPropertyType SVGAnimateElement::determineAnimatedPropertyType(SVGElement* targetElement) const
{
    ASSERT(targetElement);

    Vector<AnimatedPropertyType> propertyTypes;
    targetElement->animatedPropertyTypeForAttribute(attributeName(), propertyTypes);
    if (propertyTypes.isEmpty())
        return AnimatedUnknown;

    ASSERT(propertyTypes.size() <= 2);
    AnimatedPropertyType type = propertyTypes[0];
    if (hasTagName(SVGNames::animateColorTag) && type != AnimatedColor)
        return AnimatedUnknown;

    // Animations of transform lists are not allowed for <animate> or <set>
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (type == AnimatedTransformList && !hasTagName(SVGNames::animateTransformTag))
        return AnimatedUnknown;

    // Fortunately there's just one special case needed here: SVGMarkerElements orientAttr, which
    // corresponds to SVGAnimatedAngle orientAngle and SVGAnimatedEnumeration orientType. We have to
    // figure out whose value to change here.
    if (targetElement->hasTagName(SVGNames::markerTag) && type == AnimatedAngle) {
        ASSERT(propertyTypes.size() == 2);
        ASSERT(propertyTypes[0] == AnimatedAngle);
        ASSERT(propertyTypes[1] == AnimatedEnumeration);
    } else if (propertyTypes.size() == 2)
        ASSERT(propertyTypes[0] == propertyTypes[1]);

    return type;
}

void SVGAnimateElement::calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement)
{
    ASSERT(resultElement);
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return;

    ASSERT(m_animatedPropertyType == determineAnimatedPropertyType(targetElement));

    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(m_animatedPropertyType != AnimatedTransformList || hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);
    ASSERT(m_animator->type() == m_animatedPropertyType);
    ASSERT(m_fromType);
    ASSERT(m_fromType->type() == m_animatedPropertyType);
    ASSERT(m_toType);

    ASSERT(resultElement->hasTagName(SVGNames::animateTag)
        || resultElement->hasTagName(SVGNames::animateColorTag)
        || resultElement->hasTagName(SVGNames::animateTransformTag)
        || resultElement->hasTagName(SVGNames::setTag));

    SVGAnimateElement* resultAnimationElement = static_cast<SVGAnimateElement*>(resultElement);
    ASSERT(resultAnimationElement->m_animatedType);
    ASSERT(resultAnimationElement->m_animatedPropertyType == m_animatedPropertyType);

    if (hasTagName(SVGNames::setTag))
        percentage = 1;

    // Target element might have changed.
    m_animator->setContextElement(targetElement);

    // Be sure to detach list wrappers before we modfiy their underlying value. If we'd do
    // if after calculateAnimatedValue() ran the cached pointers in the list propery tear
    // offs would point nowhere, and we couldn't create copies of those values anymore,
    // while detaching. This is covered by assertions, moving this down would fire them.
    if (!m_animatedProperties.isEmpty())
        m_animator->animValWillChange(m_animatedProperties);
    m_animator->calculateAnimatedValue(percentage, repeat, m_fromType, m_toType, resultAnimationElement->m_animatedType);
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    determinePropertyValueTypes(fromString, toString); 
    ensureAnimator()->calculateFromAndToValues(m_fromType, m_toType, fromString, toString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    ASSERT(!hasTagName(SVGNames::setTag));

    determinePropertyValueTypes(fromString, byString); 
    ensureAnimator()->calculateFromAndByValues(m_fromType, m_toType, fromString, byString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

#ifndef NDEBUG
static inline bool propertyTypesAreConsistent(AnimatedPropertyType expectedPropertyType, const Vector<SVGAnimatedProperty*>& properties)
{
    for (size_t i = 0; i < properties.size(); ++i) {
        if (expectedPropertyType != properties[i]->animatedPropertyType()) {
            // This is the only allowed inconsistency. SVGAnimatedAngleAnimator handles both SVGAnimatedAngle & SVGAnimatedEnumeration for markers orient attribute.
            if (expectedPropertyType == AnimatedAngle && properties[i]->animatedPropertyType() == AnimatedEnumeration)
                return true;
            return false;
        }
    }
    return true;
}
#endif

void SVGAnimateElement::resetToBaseValue(const String& baseString)
{
    // If animatedProperty is not null, we're dealing with a SVG DOM primitive animation.
    // In that case we don't need any base value strings, but can directly operate on these
    // SVG DOM primitives, like SVGLength.
    SVGAnimatedTypeAnimator* animator = ensureAnimator();
    ASSERT(m_animatedPropertyType == animator->type());

    SVGElement* targetElement = this->targetElement();
    const QualifiedName& attributeName = this->attributeName();
    ShouldApplyAnimation shouldApply = shouldApplyAnimation(targetElement, attributeName);
    if (shouldApply == ApplyXMLAnimation)
        m_animatedProperties = animator->findAnimatedPropertiesForAttributeName(targetElement, attributeName);
    else
        ASSERT(m_animatedProperties.isEmpty());

    if (m_animatedProperties.isEmpty()) {
        // Legacy fallback code path, uses the passed-in baseString, which is cached.
        if (!m_animatedType)
            m_animatedType = animator->constructFromString(baseString);
        else
            m_animatedType->setValueAsString(attributeName, baseString);
        return;
    }

    ASSERT(propertyTypesAreConsistent(m_animatedPropertyType, m_animatedProperties));
    if (!m_animatedType)
        m_animatedType = animator->startAnimValAnimation(m_animatedProperties);
    else
        animator->resetAnimValToBaseVal(m_animatedProperties, m_animatedType.get());
}

void SVGAnimateElement::applyResultsToTarget()
{
    ASSERT(m_animatedPropertyType != AnimatedTransformList || hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animatedType);
    ASSERT(m_animator);

    if (m_animatedProperties.isEmpty()) {
        // CSS / legacy XML change code-path.
        setTargetAttributeAnimatedValue(m_animatedType.get());
        return;
    }

    // SVG DOM animVal animation code-path.
    m_animator->animValDidChange(m_animatedProperties);
}

float SVGAnimateElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return -1;

    return ensureAnimator()->calculateDistance(fromString, toString);
}

void SVGAnimateElement::targetElementWillChange(SVGElement* currentTarget, SVGElement* newTarget)
{
    SVGSMILElement::targetElementWillChange(currentTarget, newTarget);

    if (!m_animatedProperties.isEmpty()) {
        ensureAnimator()->stopAnimValAnimation(m_animatedProperties);
        m_animatedProperties.clear();
    }

    m_animatedType.clear();
    m_fromType.clear();
    m_toType.clear();
    m_animator.clear();
    m_animatedPropertyType = newTarget ? determineAnimatedPropertyType(newTarget) : AnimatedString;
}

SVGAnimatedTypeAnimator* SVGAnimateElement::ensureAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatorFactory::create(this, targetElement(), m_animatedPropertyType);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return m_animator.get();
}

}

#endif // ENABLE(SVG)
