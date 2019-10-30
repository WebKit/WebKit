/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "SVGSMILElement.h"
#include "SVGTests.h"
#include "UnitBezier.h"

namespace WebCore {

class ConditionEventListener;
class TimeContainer;

// If we have 'currentColor' or 'inherit' as animation value, we need to grab
// the value during the animation since the value can be animated itself.
enum AnimatedPropertyValueType { RegularPropertyValue, CurrentColorValue, InheritValue };

class SVGAnimationElement : public SVGSMILElement, public SVGTests {
    WTF_MAKE_ISO_ALLOCATED(SVGAnimationElement);
public:
    float getStartTime() const;
    float getCurrentTime() const;
    float getSimpleDuration() const;

    void beginElement();
    void beginElementAt(float offset);
    void endElement();
    void endElementAt(float offset);

    static bool isTargetAttributeCSSProperty(SVGElement*, const QualifiedName&);

    bool isAdditive() const override;
    bool isAccumulated() const;
    AnimationMode animationMode() const { return m_animationMode; }
    CalcMode calcMode() const { return m_calcMode; }

    AnimatedPropertyValueType fromPropertyValueType() const { return m_fromPropertyValueType; }
    AnimatedPropertyValueType toPropertyValueType() const { return m_toPropertyValueType; }

    void animateAdditiveNumber(float percentage, unsigned repeatCount, float fromNumber, float toNumber, float toAtEndOfDurationNumber, float& animatedNumber)
    {
        float number;
        if (calcMode() == CalcMode::Discrete)
            number = percentage < 0.5 ? fromNumber : toNumber;
        else
            number = (toNumber - fromNumber) * percentage + fromNumber;

        if (isAccumulated() && repeatCount)
            number += toAtEndOfDurationNumber * repeatCount;

        if (isAdditive() && animationMode() != AnimationMode::To)
            animatedNumber += number;
        else
            animatedNumber = number;
    }

    enum class AttributeType : uint8_t { CSS, XML, Auto };
    AttributeType attributeType() const { return m_attributeType; }

    void computeCSSPropertyValue(SVGElement*, CSSPropertyID, String& value);
    virtual void determinePropertyValueTypes(const String& from, const String& to);

protected:
    SVGAnimationElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGAnimationElement, SVGElement, SVGTests>;
    const SVGPropertyRegistry& propertyRegistry() const override { return m_propertyRegistry; }

    virtual void resetAnimation();

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    String toValue() const;
    String byValue() const;
    String fromValue() const;

    String targetAttributeBaseValue();

    // from SVGSMILElement
    void startedActiveInterval() override;
    void updateAnimation(float percent, unsigned repeat) override;

    AnimatedPropertyValueType m_fromPropertyValueType { RegularPropertyValue };
    AnimatedPropertyValueType m_toPropertyValueType { RegularPropertyValue };

    void setAttributeName(const QualifiedName&) override { }

    virtual void updateAnimationMode();
    void setAnimationMode(AnimationMode animationMode) { m_animationMode = animationMode; }
    void setCalcMode(CalcMode calcMode) { m_calcMode = calcMode; }

private:
    void animationAttributeChanged() override;
    void setAttributeType(const AtomString&);

    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) = 0;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) = 0;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) = 0;
    virtual void calculateAnimatedValue(float percent, unsigned repeatCount) = 0;
    virtual Optional<float> calculateDistance(const String& /*fromString*/, const String& /*toString*/) = 0;

    void currentValuesForValuesAnimation(float percent, float& effectivePercent, String& from, String& to);
    void calculateKeyTimesForCalcModePaced();
    float calculatePercentFromKeyPoints(float percent) const;
    void currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const;
    float calculatePercentForSpline(float percent, unsigned splineIndex) const;
    float calculatePercentForFromTo(float percent) const;
    unsigned calculateKeyTimesIndex(float percent) const;

    void setCalcMode(const AtomString&);

    bool m_animationValid { false };

    AttributeType m_attributeType { AttributeType::Auto };
    Vector<String> m_values;
    Vector<float> m_keyTimes;
    Vector<float> m_keyPoints;
    Vector<UnitBezier> m_keySplines;
    String m_lastValuesAnimationFrom;
    String m_lastValuesAnimationTo;
    CalcMode m_calcMode { CalcMode::Linear };
    AnimationMode m_animationMode { AnimationMode::None };
    PropertyRegistry m_propertyRegistry { *this };
};

} // namespace WebCore
