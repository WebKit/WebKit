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

using namespace std;

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document* document)
    : SVGAnimationElement(tagName, document)
    , m_animatedPropertyType(AnimatedString)
    , m_fromPropertyValueType(RegularPropertyValue)
    , m_toPropertyValueType(RegularPropertyValue)
{
    ASSERT(hasTagName(SVGNames::animateTag) || hasTagName(SVGNames::setTag) || hasTagName(SVGNames::animateColorTag));
}

PassRefPtr<SVGAnimateElement> SVGAnimateElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGAnimateElement(tagName, document));
}

SVGAnimateElement::~SVGAnimateElement()
{
}

static inline void getPropertyValue(SVGElement* svgParent, const QualifiedName& attributeName, String& value)
{
    ASSERT(svgParent->isStyled());
    value = computedStyle(svgParent)->getPropertyValue(cssPropertyID(attributeName.localName()));
}

static bool inheritsFromProperty(SVGElement* targetElement, const QualifiedName& attributeName, const String& value)
{
    ASSERT(targetElement);
    DEFINE_STATIC_LOCAL(const AtomicString, inherit, ("inherit"));
    
    if (value.isEmpty() || value != inherit || !targetElement->isStyled())
        return false;
    return SVGStyledElement::isAnimatableCSSProperty(attributeName);
}

static bool attributeValueIsCurrentColor(const String& value)
{
    DEFINE_STATIC_LOCAL(const AtomicString, currentColor, ("currentColor"));
    return value == currentColor;
}

void SVGAnimateElement::adjustForCurrentColor(SVGElement* targetElement, Color& color)
{
    ASSERT(targetElement);

    if (RenderObject* targetRenderer = targetElement->renderer())
        color = targetRenderer->style()->visitedDependentColor(CSSPropertyColor);
    else
        color = Color();
}

void SVGAnimateElement::adjustForInheritance(SVGElement* targetElement, const QualifiedName& attributeName, String& value)
{
    // FIXME: At the moment the computed style gets returned as a String and needs to get parsed again.
    // In the future we might want to work with the value type directly to avoid the String parsing.
    ASSERT(targetElement);

    Element* parent = targetElement->parentElement();
    if (!parent || !parent->isSVGElement())
        return;

    SVGElement* svgParent = static_cast<SVGElement*>(parent);
    if (svgParent->isStyled())
        getPropertyValue(svgParent, attributeName, value);
}

bool SVGAnimateElement::hasValidAttributeType()
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;
    
    return determineAnimatedPropertyType(targetElement) != AnimatedUnknown;
}

AnimatedPropertyType SVGAnimateElement::determineAnimatedPropertyType(SVGElement* targetElement) const
{
    ASSERT(targetElement);

    Vector<AnimatedPropertyType> propertyTypes;
    targetElement->animatedPropertyTypeForAttribute(attributeName(), propertyTypes);
    if (propertyTypes.isEmpty())
        return AnimatedUnknown;

    AnimatedPropertyType type = propertyTypes[0];
    if (hasTagName(SVGNames::animateColorTag) && type != AnimatedColor)
        return AnimatedUnknown;

    // FIXME: Animator for AnimatedEnumeration missing. Falling back to String animation.
    if (type == AnimatedEnumeration)
        return AnimatedString;

    // Animations of transform lists are not allowed for <animate> or <set>
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (type == AnimatedTransformList)
        return AnimatedUnknown;

    return type;
}

void SVGAnimateElement::determinePropertyValueTypes(const String& from, const String& to)
{
    SVGElement* targetElement = this->targetElement();
    ASSERT(targetElement);

    if (inheritsFromProperty(targetElement, attributeName(), from))
        m_fromPropertyValueType = InheritValue;
    if (inheritsFromProperty(targetElement, attributeName(), to))
        m_toPropertyValueType = InheritValue;

    if (m_animatedPropertyType != AnimatedColor)
        return;
    
    if (attributeValueIsCurrentColor(from))
        m_fromPropertyValueType = CurrentColorValue;
    if (attributeValueIsCurrentColor(to))
        m_toPropertyValueType = CurrentColorValue;
}

void SVGAnimateElement::calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement)
{
    ASSERT(resultElement);
    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(m_animatedPropertyType != AnimatedEnumeration);
    ASSERT(m_animatedPropertyType != AnimatedTransformList);
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);
    ASSERT(m_fromType);
    ASSERT(m_toType);

    SVGAnimateElement* resultAnimationElement = static_cast<SVGAnimateElement*>(resultElement);
    ASSERT(resultAnimationElement->m_animatedType);
    ASSERT(resultAnimationElement->m_animatedPropertyType == m_animatedPropertyType);
    ASSERT(resultAnimationElement->hasTagName(SVGNames::animateTag)
        || resultAnimationElement->hasTagName(SVGNames::animateColorTag) 
        || resultAnimationElement->hasTagName(SVGNames::setTag));

    if (hasTagName(SVGNames::setTag))
        percentage = 1;

    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return;

    // Target element might have changed.
    m_animator->setContextElement(targetElement);
    m_animator->calculateAnimatedValue(percentage, repeat, m_fromType, m_toType, resultAnimationElement->m_animatedType);
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    m_animatedPropertyType = determineAnimatedPropertyType(targetElement);

    ensureAnimator()->calculateFromAndToValues(m_fromType, m_toType, fromString, toString);
    return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    ASSERT(!hasTagName(SVGNames::setTag));
    m_animatedPropertyType = determineAnimatedPropertyType(targetElement);

    ensureAnimator()->calculateFromAndByValues(m_fromType, m_toType, fromString, byString);
    return true;
}

void SVGAnimateElement::resetToBaseValue(const String& baseString)
{
    SVGElement* targetElement = this->targetElement();
    ASSERT(targetElement);
    m_animatedPropertyType = determineAnimatedPropertyType(targetElement);

    if (!m_animatedType)
        m_animatedType = ensureAnimator()->constructFromString(baseString);
    else
        m_animatedType->setValueAsString(attributeName(), baseString);
}
    
void SVGAnimateElement::applyResultsToTarget()
{
    ASSERT(m_animatedPropertyType != AnimatedEnumeration);
    ASSERT(m_animatedPropertyType != AnimatedTransformList);
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animatedType);

    setTargetAttributeAnimatedValue(m_animatedType->valueAsString());
}
    
float SVGAnimateElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return -1;
    m_animatedPropertyType = determineAnimatedPropertyType(targetElement);
    
    return ensureAnimator()->calculateDistance(fromString, toString);
}

SVGAnimatedTypeAnimator* SVGAnimateElement::ensureAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatorFactory::create(this, targetElement(), m_animatedPropertyType);
    return m_animator.get();
}

}
#endif // ENABLE(SVG)
