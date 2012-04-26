/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron McCormack <cam@mcc.id.au>
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

#ifndef SVGAnimationElement_h
#define SVGAnimationElement_h

#if ENABLE(SVG)
#include "ElementTimeControl.h"
#include "Path.h"
#include "SMILTime.h"
#include "SVGAnimatedBoolean.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGSMILElement.h"
#include "SVGStringList.h"
#include "SVGTests.h"
#include "UnitBezier.h"

namespace WebCore {

enum AnimationMode {
    NoAnimation,
    FromToAnimation,
    FromByAnimation,
    ToAnimation,
    ByAnimation,
    ValuesAnimation,
    PathAnimation // Used by AnimateMotion.
};

// If we have 'currentColor' or 'inherit' as animation value, we need to grab
// the value during the animation since the value can be animated itself.
enum AnimatedPropertyValueType {
    RegularPropertyValue,
    CurrentColorValue,
    InheritValue
};

enum CalcMode {
    CalcModeDiscrete,
    CalcModeLinear,
    CalcModePaced,
    CalcModeSpline
};

class ConditionEventListener;
class TimeContainer;
class SVGAnimatedType;

class SVGAnimationElement : public SVGSMILElement,
                            public SVGTests,
                            public SVGExternalResourcesRequired,
                            public ElementTimeControl {
public:
    // SVGAnimationElement
    float getStartTime() const;
    float getCurrentTime() const;
    float getSimpleDuration(ExceptionCode&) const;

    // ElementTimeControl
    virtual void beginElement();
    virtual void beginElementAt(float offset);
    virtual void endElement();
    virtual void endElementAt(float offset);

    static bool isTargetAttributeCSSProperty(SVGElement*, const QualifiedName&);

    virtual bool isAdditive() const;
    bool isAccumulated() const;
    AnimationMode animationMode() const;
    CalcMode calcMode() const;

    enum ShouldApplyAnimation {
        DontApplyAnimation,
        ApplyCSSAnimation,
        ApplyXMLAnimation
    };

    ShouldApplyAnimation shouldApplyAnimation(SVGElement* targetElement, const QualifiedName& attributeName);

    AnimatedPropertyValueType fromPropertyValueType() const { return m_fromPropertyValueType; }
    AnimatedPropertyValueType toPropertyValueType() const { return m_toPropertyValueType; }

    template<typename AnimatedType>
    void adjustForInheritance(AnimatedType (*parseTypeFromString)(SVGAnimationElement*, const String&),
                              AnimatedPropertyValueType valueType, AnimatedType& animatedType, SVGElement* contextElement)
    {
        if (valueType != InheritValue)
            return;
        // Replace 'inherit' by its computed property value.
        ASSERT(parseTypeFromString);
        String typeString;
        adjustForInheritance(contextElement, attributeName(), typeString);
        animatedType = (*parseTypeFromString)(this, typeString);
    }

    template<typename AnimatedType>
    void adjustFromToValues(AnimatedType (*parseTypeFromString)(SVGAnimationElement*, const String&),
                            AnimatedType& fromType, AnimatedType& toType, AnimatedType& animatedType,
                            float& percentage, SVGElement* contextElement)
    {
        // To-animation uses contributions from the lower priority animations as the base value.
        if (animationMode() == ToAnimation)
            fromType = AnimatedType(animatedType);

        adjustForInheritance<AnimatedType>(parseTypeFromString, m_fromPropertyValueType, fromType, contextElement);
        adjustForInheritance<AnimatedType>(parseTypeFromString, m_toPropertyValueType, toType, contextElement);

        if (calcMode() == CalcModeDiscrete)
            percentage = percentage < 0.5 ? 0 : 1;
    }

    template<typename AnimatedType>
    bool adjustFromToListValues(AnimatedType (*parseTypeFromString)(SVGAnimationElement*, const String&), 
                                AnimatedType& fromList, AnimatedType& toList, AnimatedType& animatedList,
                                float& percentage, SVGElement* contextElement)
    {
        adjustFromToValues(parseTypeFromString, fromList, toList, animatedList, percentage, contextElement);

        // If no 'to' value is given, nothing to animate.
        unsigned toListSize = toList.size();
        if (!toListSize)
            return false;

        // If the 'from' value is given and it's length doesn't match the 'to' value list length, fallback to a discrete animation.
        unsigned fromListSize = fromList.size();
        if (fromListSize != toListSize && fromListSize) {
            if (percentage < 0.5) {
                if (animationMode() != ToAnimation)
                    animatedList = fromList;
            } else
                animatedList = toList;

            return false;
        }

        ASSERT(!fromListSize || fromListSize == toListSize);
        if (animatedList.size() < toListSize)
            animatedList.resize(toListSize);

        return true;
    }

    template<typename AnimatedType>
    void animateDiscreteType(float percentage, AnimatedType& fromType, AnimatedType& toType, AnimatedType& animatedType)
    {
        if ((animationMode() == FromToAnimation && percentage > 0.5) || animationMode() == ToAnimation || percentage == 1) {
            animatedType = AnimatedType(toType);
            return;
        }
        animatedType = AnimatedType(fromType);
    }

    void animateAdditiveNumber(float percentage, unsigned repeatCount, float fromNumber, float toNumber, float& animatedNumber)
    {
        float number;
        if (calcMode() == CalcModeDiscrete)
            number = percentage < 0.5 ? fromNumber : toNumber;
        else
            number = (toNumber - fromNumber) * percentage + fromNumber;

        // FIXME: This is not correct for values animation. Right now we transform values-animation to multiple from-to-animations and
        // accumulate every single value to the previous one. But accumulation should just take into account after a complete cycle
        // of values-animaiton. See example at: http://www.w3.org/TR/2001/REC-smil-animation-20010904/#RepeatingAnim
        if (isAccumulated() && repeatCount)
            number += toNumber * repeatCount;

        if (isAdditive() && animationMode() != ToAnimation)
            animatedNumber += number;
        else
            animatedNumber = number;
    }

protected:
    SVGAnimationElement(const QualifiedName&, Document*);

    virtual void determinePropertyValueTypes(const String& from, const String& to);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(Attribute*) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    enum AttributeType {
        AttributeTypeCSS,
        AttributeTypeXML,
        AttributeTypeAuto
    };
    AttributeType attributeType() const;

    String toValue() const;
    String byValue() const;
    String fromValue() const;

    String targetAttributeBaseValue();
    void setTargetAttributeAnimatedValue(SVGAnimatedType*);

    // from SVGSMILElement
    virtual void startedActiveInterval();
    virtual void updateAnimation(float percent, unsigned repeat, SVGSMILElement* resultElement);

    AnimatedPropertyValueType m_fromPropertyValueType;
    AnimatedPropertyValueType m_toPropertyValueType;

private:
    virtual void animationAttributeChanged() OVERRIDE;

    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) = 0;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) = 0;
    virtual void calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement) = 0;
    virtual float calculateDistance(const String& /*fromString*/, const String& /*toString*/) { return -1.f; }
    virtual Path animationPath() const { return Path(); }

    void currentValuesForValuesAnimation(float percent, float& effectivePercent, String& from, String& to);
    void calculateKeyTimesForCalcModePaced();
    float calculatePercentFromKeyPoints(float percent) const;
    void currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const;
    float calculatePercentForSpline(float percent, unsigned splineIndex) const;
    float calculatePercentForFromTo(float percent) const;
    unsigned calculateKeyTimesIndex(float percent) const;

    void applyAnimatedValue(ShouldApplyAnimation, SVGElement* targetElement, const QualifiedName& attributeName, SVGAnimatedType*);
    void adjustForInheritance(SVGElement* targetElement, const QualifiedName& attributeName, String&);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGAnimationElement)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    // SVGTests
    virtual void synchronizeRequiredFeatures() { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() { SVGTests::synchronizeSystemLanguage(this); }

    bool m_animationValid;

    Vector<String> m_values;
    Vector<float> m_keyTimes;
    Vector<float> m_keyPoints;
    Vector<UnitBezier> m_keySplines;
    String m_lastValuesAnimationFrom;
    String m_lastValuesAnimationTo;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimationElement_h
