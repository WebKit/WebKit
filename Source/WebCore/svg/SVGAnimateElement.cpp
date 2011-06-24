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

#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGAnimateElement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "QualifiedName.h"
#include "RenderObject.h"
#include "SVGAnimatorFactory.h"
#include "SVGNames.h"
#include "SVGPathParserFactory.h"
#include "SVGStyledElement.h"

using namespace std;

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document* document)
    : SVGAnimationElement(tagName, document)
    , m_animatedAttributeType(AnimatedString)
    , m_fromPropertyValueType(RegularPropertyValue)
    , m_toPropertyValueType(RegularPropertyValue)
    , m_animatedPathPointer(0)
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

void SVGAnimateElement::adjustForCurrentColor(SVGElement* targetElement, Color& color)
{
    ASSERT(targetElement);
    
    if (RenderObject* targetRenderer = targetElement->renderer())
        color = targetRenderer->style()->visitedDependentColor(CSSPropertyColor);
    else
        color = Color();
}

static inline void getPropertyValue(SVGElement* svgParent, const QualifiedName& attributeName, String& value)
{
    ASSERT(svgParent->isStyled());
    value = computedStyle(svgParent)->getPropertyValue(cssPropertyID(attributeName.localName()));
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

bool SVGAnimateElement::hasValidAttributeType() const
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;
    
    return determineAnimatedAttributeType(targetElement) != AnimatedUnknown;
}

AnimatedAttributeType SVGAnimateElement::determineAnimatedAttributeType(SVGElement* targetElement) const
{
    ASSERT(targetElement);

    AnimatedAttributeType type = targetElement->animatedPropertyTypeForAttribute(attributeName());
    if (type == AnimatedUnknown || (hasTagName(SVGNames::animateColorTag) && type != AnimatedColor))
        return AnimatedUnknown;

    // FIXME: We need type specific animations in the future. Many animations marked as AnimatedString today will
    // support continuous animations.
    switch (type) {
    case AnimatedAngle:
        return AnimatedAngle;
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedLengthList:
    case AnimatedPreserveAspectRatio:
    case AnimatedString:
        return AnimatedString;
    case AnimatedColor:
        return AnimatedColor;
    case AnimatedLength:
        return AnimatedLength;
    case AnimatedInteger:
    case AnimatedNumber:
        return AnimatedNumber;
    case AnimatedPath:
        return AnimatedPath;
    case AnimatedPoints:
        return AnimatedPoints;
    case AnimatedRect:
        return AnimatedRect;
    case AnimatedTransformList:
    case AnimatedUnknown:
        // Animations of transform lists are not allowed for <animate> or <set>
        // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
        return AnimatedUnknown;
    }

    ASSERT_NOT_REACHED();
    return AnimatedUnknown;
}

void SVGAnimateElement::calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement)
{
    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(resultElement);
    AnimationMode animationMode = this->animationMode();
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return;
    
    if (hasTagName(SVGNames::setTag))
        percentage = 1;
    if (!resultElement->hasTagName(SVGNames::animateTag) && !resultElement->hasTagName(SVGNames::animateColorTag) 
        && !resultElement->hasTagName(SVGNames::setTag))
        return;
    SVGAnimateElement* results = static_cast<SVGAnimateElement*>(resultElement);
    // Can't accumulate over a string property.
    if (results->m_animatedAttributeType == AnimatedString && m_animatedAttributeType != AnimatedString)
        return;
    switch (m_animatedAttributeType) {
    case AnimatedPath: {
        if (animationMode == ToAnimation) {
            ASSERT(results->m_animatedPathPointer);
            m_fromPath = results->m_animatedPathPointer->copy();
        }
        if (!percentage) {
            ASSERT(m_fromPath);
            ASSERT(percentage >= 0);
            results->m_animatedPathPointer = m_fromPath.get();
        } else if (percentage == 1) {
            ASSERT(m_toPath);
            results->m_animatedPathPointer = m_toPath.get();
        } else {
            if (m_fromPath && m_toPath) {
                SVGPathParserFactory* factory = SVGPathParserFactory::self();
                if (!factory->buildAnimatedSVGPathByteStream(m_fromPath.get(), m_toPath.get(), results->m_animatedPath, percentage)) {
                    results->m_animatedPath.clear();
                    results->m_animatedPathPointer = 0;
                } else
                    results->m_animatedPathPointer = results->m_animatedPath.get();
            } else
                results->m_animatedPathPointer = 0;
            // Fall back to discrete animation if the paths are not compatible
            if (!results->m_animatedPathPointer) {
                ASSERT(m_fromPath);
                ASSERT(m_toPath);
                ASSERT(!results->m_animatedPath);
                results->m_animatedPathPointer = ((animationMode == FromToAnimation && percentage > 0.5f) || animationMode == ToAnimation || percentage == 1) 
                    ? m_toPath.get() : m_fromPath.get();
            }
        }
        return;
    }
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString: {
        ASSERT(m_animator);
        ASSERT(results->m_animatedType);
        // Target element might have changed.
        m_animator->setContextElement(targetElement);
        m_animator->calculateAnimatedValue(percentage, repeat, m_fromType, m_toType, results->m_animatedType);
        return;
    }
    default:
        break;
    }
    ASSERT_NOT_REACHED();
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

void SVGAnimateElement::determinePropertyValueTypes(const String& from, const String& to)
{
    SVGElement* targetElement = this->targetElement();
    ASSERT(targetElement);

    if (inheritsFromProperty(targetElement, attributeName(), from))
        m_fromPropertyValueType = InheritValue;
    if (inheritsFromProperty(targetElement, attributeName(), to))
        m_toPropertyValueType = InheritValue;

    if (m_animatedAttributeType != AnimatedColor)
        return;
    
    if (attributeValueIsCurrentColor(from))
        m_fromPropertyValueType = CurrentColorValue;
    if (attributeValueIsCurrentColor(to))
        m_toPropertyValueType = CurrentColorValue;
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    // FIXME: Needs more solid way determine target attribute type.
    m_animatedAttributeType = determineAnimatedAttributeType(targetElement);
    switch (m_animatedAttributeType) {
    case AnimatedPath: {
        SVGPathParserFactory* factory = SVGPathParserFactory::self();
        if (factory->buildSVGPathByteStreamFromString(toString, m_toPath, UnalteredParsing)) {
            // For to-animations the from number is calculated later
            if (animationMode() == ToAnimation || factory->buildSVGPathByteStreamFromString(fromString, m_fromPath, UnalteredParsing))
                return true;
        }
        m_fromPath.clear();
        m_toPath.clear();
        break;
    }
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString:
        ensureAnimator()->calculateFromAndToValues(m_fromType, m_toType, fromString, toString);
        return true;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    ASSERT(!hasTagName(SVGNames::setTag));
    m_animatedAttributeType = determineAnimatedAttributeType(targetElement);
    switch (m_animatedAttributeType) {
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString:
        ensureAnimator()->calculateFromAndByValues(m_fromType, m_toType, fromString, byString);
        return true;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void SVGAnimateElement::resetToBaseValue(const String& baseString)
{
    SVGElement* targetElement = this->targetElement();
    ASSERT(targetElement);
    m_animatedAttributeType = determineAnimatedAttributeType(targetElement);
    switch (m_animatedAttributeType) {
    case AnimatedPath: {
        m_animatedPath.clear();
        SVGPathParserFactory* factory = SVGPathParserFactory::self();
        factory->buildSVGPathByteStreamFromString(baseString, m_animatedPath, UnalteredParsing);
        m_animatedPathPointer = m_animatedPath.get();
        return;
    }
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString: {
        if (!m_animatedType)
            m_animatedType = ensureAnimator()->constructFromString(baseString);
        else
            m_animatedType->setValueAsString(attributeName(), baseString);
        return;
    }
    default:
        break;
    }
    ASSERT_NOT_REACHED();
}
    
void SVGAnimateElement::applyResultsToTarget()
{
    String valueToApply;
    switch (m_animatedAttributeType) {
    case AnimatedPath: {
        if (!m_animatedPathPointer || m_animatedPathPointer->isEmpty())
            valueToApply = String();
        else {
            // We need to keep going to string and back because we are currently only able to paint
            // "processed" paths where complex shapes are replaced with simpler ones. Path 
            // morphing needs to be done with unprocessed paths.
            // FIXME: This could be optimized if paths were not processed at parse time.
            SVGPathParserFactory* factory = SVGPathParserFactory::self();
            factory->buildStringFromByteStream(m_animatedPathPointer, valueToApply, UnalteredParsing);
        }
        break;
    }
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString:
        valueToApply = m_animatedType->valueAsString();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    setTargetAttributeAnimatedValue(valueToApply);
}
    
float SVGAnimateElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return -1;
    m_animatedAttributeType = determineAnimatedAttributeType(targetElement);
    switch (m_animatedAttributeType) {
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedLength:
    case AnimatedNumber:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedString:
        return ensureAnimator()->calculateDistance(fromString, toString);
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return -1;
}

SVGAnimatedTypeAnimator* SVGAnimateElement::ensureAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatorFactory::create(this, targetElement(), m_animatedAttributeType);
    return m_animator.get();
}

}
#endif // ENABLE(SVG)
