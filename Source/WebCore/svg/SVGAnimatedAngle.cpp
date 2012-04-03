/*
 * Copyright (C) Research In Motion Limited 2011, 2012. All rights reserved.
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
#include "SVGAnimatedAngle.h"

#include "SVGAnimateElement.h"
#include "SVGMarkerElement.h"

namespace WebCore {

SVGAnimatedAngleAnimator::SVGAnimatedAngleAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedAngle, animationElement, contextElement)
{
}

static inline SVGAngle& sharedSVGAngle(const String& valueAsString)
{
    DEFINE_STATIC_LOCAL(SVGAngle, sharedAngle, ());
    sharedAngle.setValueAsString(valueAsString, ASSERT_NO_EXCEPTION);
    return sharedAngle;
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedAngleAnimator::constructFromString(const String& string)
{
    OwnPtr<SVGAnimatedType> animatedType = SVGAnimatedType::createAngleAndEnumeration(new pair<SVGAngle, unsigned>);
    pair<SVGAngle, unsigned>& animatedPair = animatedType->angleAndEnumeration();

    SVGAngle angle;
    SVGMarkerOrientType orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(string,  angle);
    if (orientType > 0)
        animatedPair.second = orientType;
    if (orientType == SVGMarkerOrientAngle)
        animatedPair.first = angle;

    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedAngleAnimator::startAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    return SVGAnimatedType::createAngleAndEnumeration(constructFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(properties));
}

void SVGAnimatedAngleAnimator::stopAnimValAnimation(const Vector<SVGAnimatedProperty*>& properties)
{
    stopAnimValAnimationForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(properties);
}

void SVGAnimatedAngleAnimator::resetAnimValToBaseVal(const Vector<SVGAnimatedProperty*>& properties, SVGAnimatedType* type)
{
    resetFromBaseValues<SVGAnimatedAngle, SVGAnimatedEnumeration>(properties, type, &SVGAnimatedType::angleAndEnumeration);
}

void SVGAnimatedAngleAnimator::animValWillChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValWillChangeForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(properties);
}

void SVGAnimatedAngleAnimator::animValDidChange(const Vector<SVGAnimatedProperty*>& properties)
{
    animValDidChangeForTypes<SVGAnimatedAngle, SVGAnimatedEnumeration>(properties);
}

void SVGAnimatedAngleAnimator::calculateFromAndToValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, toString);

    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedAngleAnimator::calculateFromAndByValues(OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    ASSERT(m_contextElement);
    ASSERT(m_animationElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    animationElement->determinePropertyValueTypes(fromString, byString);
    
    from = constructFromString(fromString);
    to = constructFromString(byString);

    pair<SVGAngle, unsigned>& fromAngleAndEnumeration = from->angleAndEnumeration();
    pair<SVGAngle, unsigned>& toAngleAndEnumeration = to->angleAndEnumeration();
    // Only respect by animations, if from and by are both specified in angles (and not eg. 'auto').
    if (fromAngleAndEnumeration.second != toAngleAndEnumeration.second || fromAngleAndEnumeration.second != SVGMarkerOrientAngle)
        return;
    SVGAngle& fromAngle = fromAngleAndEnumeration.first;
    SVGAngle& toAngle = toAngleAndEnumeration.first;
    toAngle.setValue(toAngle.value() + fromAngle.value());
}

void SVGAnimatedAngleAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount,
                                                      OwnPtr<SVGAnimatedType>& from, OwnPtr<SVGAnimatedType>& to, OwnPtr<SVGAnimatedType>& animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    SVGAnimateElement* animationElement = static_cast<SVGAnimateElement*>(m_animationElement);
    
    AnimationMode animationMode = animationElement->animationMode();

    // To animation uses contributions from the lower priority animations as the base value.
    pair<SVGAngle, unsigned>& animatedAngleAndEnumeration = animated->angleAndEnumeration();
    pair<SVGAngle, unsigned>& fromAngleAndEnumeration = from->angleAndEnumeration();
    if (animationMode == ToAnimation)
        fromAngleAndEnumeration.first = animatedAngleAndEnumeration.first;

    pair<SVGAngle, unsigned>& toAngleAndEnumeration = to->angleAndEnumeration();
    if (fromAngleAndEnumeration.second != toAngleAndEnumeration.second) {
        // Animating from eg. auto to 90deg, or auto to 90deg.
        if (fromAngleAndEnumeration.second == SVGMarkerOrientAngle) {
            // Animating from an angle value to eg. 'auto' - this disabled additive as 'auto' is a keyword..
            if (toAngleAndEnumeration.second == SVGMarkerOrientAuto) {
                if (percentage < 0.5f) {
                    animatedAngleAndEnumeration.first = fromAngleAndEnumeration.first;
                    animatedAngleAndEnumeration.second = SVGMarkerOrientAngle;
                    return;
                }
                animatedAngleAndEnumeration.first.setValue(0);
                animatedAngleAndEnumeration.second = SVGMarkerOrientAuto;
                return;
            }
            animatedAngleAndEnumeration.first.setValue(0);
            animatedAngleAndEnumeration.second = SVGMarkerOrientUnknown;
            return;
        }
    }

    // From 'auto' to 'auto'.
    if (fromAngleAndEnumeration.second == SVGMarkerOrientAuto) {
        animatedAngleAndEnumeration.first.setValue(0);
        animatedAngleAndEnumeration.second = SVGMarkerOrientAuto;
        return;
    }

    // If the enumeration value is not angle or auto, its unknown.
    if (fromAngleAndEnumeration.second != SVGMarkerOrientAngle) {
        animatedAngleAndEnumeration.first.setValue(0);
        animatedAngleAndEnumeration.second = SVGMarkerOrientUnknown;
        return;
    }

    // Regular from angle to angle animation, with all features like additive etc.
    animatedAngleAndEnumeration.second = SVGMarkerOrientAngle;

    // Replace 'inherit' by their computed property values.
    float number;

    float fromAngle = fromAngleAndEnumeration.first.value();
    float toAngle = toAngleAndEnumeration.first.value();
    
    if (animationElement->fromPropertyValueType() == InheritValue) {
        String fromAngleString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), fromAngleString);
        fromAngle = sharedSVGAngle(fromAngleString).value();
    }
    if (animationElement->toPropertyValueType() == InheritValue) {
        String toAngleString;
        animationElement->adjustForInheritance(m_contextElement, animationElement->attributeName(), toAngleString);
        toAngle = sharedSVGAngle(toAngleString).value(); 
    }
    
    if (animationElement->calcMode() == CalcModeDiscrete)
        number = percentage < 0.5f ? fromAngle : toAngle;
    else
        number = (toAngle - fromAngle) * percentage + fromAngle;
    
    // FIXME: This is not correct for values animation.
    if (animationElement->isAccumulated() && repeatCount)
        number += toAngle * repeatCount;
    SVGAngle& animatedSVGAngle = animatedAngleAndEnumeration.first;
    if (animationElement->isAdditive() && animationMode != ToAnimation) {
        float animatedSVGAngleValue = animatedSVGAngle.value();
        animatedSVGAngleValue += number;
        animatedSVGAngle.setValue(animatedSVGAngleValue);
    } else
        animatedSVGAngle.setValue(number);
}

float SVGAnimatedAngleAnimator::calculateDistance(const String& fromString, const String& toString)
{
    SVGAngle from = SVGAngle();
    from.setValueAsString(fromString, ASSERT_NO_EXCEPTION);
    SVGAngle to = SVGAngle();
    to.setValueAsString(toString, ASSERT_NO_EXCEPTION);
    return fabsf(to.value() - from.value());
}

}

#endif // ENABLE(SVG)
