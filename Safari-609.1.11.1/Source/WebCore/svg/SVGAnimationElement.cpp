/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGAnimationElement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "Document.h"
#include "FloatConversion.h"
#include "RenderObject.h"
#include "SVGAnimateColorElement.h"
#include "SVGAnimateElement.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGStringList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimationElement);

SVGAnimationElement::SVGAnimationElement(const QualifiedName& tagName, Document& document)
    : SVGSMILElement(tagName, document)
    , SVGTests(this)
{
}

static void parseKeyTimes(const String& parse, Vector<float>& result, bool verifyOrder)
{
    result.clear();
    bool isFirst = true;
    for (StringView timeString : StringView(parse).split(';')) {
        bool ok;
        float time = timeString.toFloat(ok);
        if (!ok || time < 0 || time > 1)
            goto fail;
        if (verifyOrder) {
            if (isFirst) {
                if (time)
                    goto fail;
                isFirst = false;
            } else if (time < result.last())
                goto fail;
        }
        result.append(time);
    }
    return;
fail:
    result.clear();
}

static void parseKeySplines(const String& parse, Vector<UnitBezier>& result)
{
    result.clear();
    if (parse.isEmpty())
        return;

    auto upconvertedCharacters = StringView(parse).upconvertedCharacters();
    const UChar* cur = upconvertedCharacters;
    const UChar* end = cur + parse.length();

    skipOptionalSVGSpaces(cur, end);

    bool delimParsed = false;
    while (cur < end) {
        delimParsed = false;
        float posA = 0;
        if (!parseNumber(cur, end, posA)) {
            result.clear();
            return;
        }

        float posB = 0;
        if (!parseNumber(cur, end, posB)) {
            result.clear();
            return;
        }

        float posC = 0;
        if (!parseNumber(cur, end, posC)) {
            result.clear();
            return;
        }

        float posD = 0;
        if (!parseNumber(cur, end, posD, false)) {
            result.clear();
            return;
        }

        skipOptionalSVGSpaces(cur, end);

        if (cur < end && *cur == ';') {
            delimParsed = true;
            cur++;
        }
        skipOptionalSVGSpaces(cur, end);

        result.append(UnitBezier(posA, posB, posC, posD));
    }
    if (!(cur == end && !delimParsed))
        result.clear();
}

bool SVGAnimationElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed([] {
        HashSet<QualifiedName> set;
        SVGTests::addSupportedAttributes(set);
        set.add({
            SVGNames::valuesAttr.get(),
            SVGNames::keyTimesAttr.get(),
            SVGNames::keyPointsAttr.get(),
            SVGNames::keySplinesAttr.get(),
            SVGNames::attributeTypeAttr.get(),
            SVGNames::calcModeAttr.get(),
            SVGNames::fromAttr.get(),
            SVGNames::toAttr.get(),
            SVGNames::byAttr.get(),
        });
        return set;
    }());
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGAnimationElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::valuesAttr) {
        // Per the SMIL specification, leading and trailing white space,
        // and white space before and after semicolon separators, is allowed and will be ignored.
        // http://www.w3.org/TR/SVG11/animate.html#ValuesAttribute
        m_values = value.string().split(';');
        for (auto& value : m_values)
            value = value.stripWhiteSpace();

        updateAnimationMode();
        return;
    }

    if (name == SVGNames::keyTimesAttr) {
        parseKeyTimes(value, m_keyTimes, true);
        return;
    }

    if (name == SVGNames::keyPointsAttr) {
        if (hasTagName(SVGNames::animateMotionTag)) {
            // This is specified to be an animateMotion attribute only but it is simpler to put it here 
            // where the other timing calculatations are.
            parseKeyTimes(value, m_keyPoints, false);
        }
        return;
    }

    if (name == SVGNames::keySplinesAttr) {
        parseKeySplines(value, m_keySplines);
        return;
    }

    if (name == SVGNames::attributeTypeAttr) {
        setAttributeType(value);
        return;
    }

    if (name == SVGNames::calcModeAttr) {
        setCalcMode(value);
        return;
    }

    if (name == SVGNames::fromAttr || name == SVGNames::toAttr || name == SVGNames::byAttr) {
        updateAnimationMode();
        return;
    }

    SVGSMILElement::parseAttribute(name, value);
    SVGTests::parseAttribute(name, value);
}

void SVGAnimationElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGSMILElement::svgAttributeChanged(attrName);
        return;
    }

    animationAttributeChanged();
}

void SVGAnimationElement::animationAttributeChanged()
{
    // Assumptions may not hold after an attribute change.
    m_animationValid = false;
    setInactive();
}

float SVGAnimationElement::getStartTime() const
{
    return narrowPrecisionToFloat(intervalBegin().value());
}

float SVGAnimationElement::getCurrentTime() const
{
    return narrowPrecisionToFloat(elapsed().value());
}

float SVGAnimationElement::getSimpleDuration() const
{
    return narrowPrecisionToFloat(simpleDuration().value());
}    
    
void SVGAnimationElement::beginElement()
{
    beginElementAt(0);
}

void SVGAnimationElement::beginElementAt(float offset)
{
    if (std::isnan(offset))
        return;
    SMILTime elapsed = this->elapsed();
    addBeginTime(elapsed, elapsed + offset, SMILTimeWithOrigin::ScriptOrigin);
}

void SVGAnimationElement::endElement()
{
    endElementAt(0);
}

void SVGAnimationElement::endElementAt(float offset)
{
    if (std::isnan(offset))
        return;
    SMILTime elapsed = this->elapsed();
    addEndTime(elapsed, elapsed + offset, SMILTimeWithOrigin::ScriptOrigin);
}

void SVGAnimationElement::updateAnimationMode()
{
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#AnimFuncValues
    if (hasAttribute(SVGNames::valuesAttr))
        setAnimationMode(AnimationMode::Values);
    else if (!toValue().isEmpty())
        setAnimationMode(fromValue().isEmpty() ? AnimationMode::To : AnimationMode::FromTo);
    else if (!byValue().isEmpty())
        setAnimationMode(fromValue().isEmpty() ? AnimationMode::By : AnimationMode::FromBy);
    else
        setAnimationMode(AnimationMode::None);
}

void SVGAnimationElement::setCalcMode(const AtomString& calcMode)
{
    static NeverDestroyed<const AtomString> discrete("discrete", AtomString::ConstructFromLiteral);
    static NeverDestroyed<const AtomString> linear("linear", AtomString::ConstructFromLiteral);
    static NeverDestroyed<const AtomString> paced("paced", AtomString::ConstructFromLiteral);
    static NeverDestroyed<const AtomString> spline("spline", AtomString::ConstructFromLiteral);
    if (calcMode == discrete)
        setCalcMode(CalcMode::Discrete);
    else if (calcMode == linear)
        setCalcMode(CalcMode::Linear);
    else if (calcMode == paced)
        setCalcMode(CalcMode::Paced);
    else if (calcMode == spline)
        setCalcMode(CalcMode::Spline);
    else
        setCalcMode(hasTagName(SVGNames::animateMotionTag) ? CalcMode::Paced : CalcMode::Linear);
}

void SVGAnimationElement::setAttributeType(const AtomString& attributeType)
{
    static NeverDestroyed<const AtomString> css("CSS", AtomString::ConstructFromLiteral);
    static NeverDestroyed<const AtomString> xml("XML", AtomString::ConstructFromLiteral);
    if (attributeType == css)
        m_attributeType = AttributeType::CSS;
    else if (attributeType == xml)
        m_attributeType = AttributeType::XML;
    else
        m_attributeType = AttributeType::Auto;
}

String SVGAnimationElement::toValue() const
{    
    return attributeWithoutSynchronization(SVGNames::toAttr);
}

String SVGAnimationElement::byValue() const
{    
    return attributeWithoutSynchronization(SVGNames::byAttr);
}

String SVGAnimationElement::fromValue() const
{    
    return attributeWithoutSynchronization(SVGNames::fromAttr);
}

bool SVGAnimationElement::isAdditive() const
{
    static NeverDestroyed<const AtomString> sum("sum", AtomString::ConstructFromLiteral);
    const AtomString& value = attributeWithoutSynchronization(SVGNames::additiveAttr);
    return value == sum || animationMode() == AnimationMode::By;
}

bool SVGAnimationElement::isAccumulated() const
{
    static NeverDestroyed<const AtomString> sum("sum", AtomString::ConstructFromLiteral);
    const AtomString& value = attributeWithoutSynchronization(SVGNames::accumulateAttr);
    return value == sum && animationMode() != AnimationMode::To;
}

bool SVGAnimationElement::isTargetAttributeCSSProperty(SVGElement* targetElement, const QualifiedName& attributeName)
{
    return targetElement->isAnimatedStyleAttribute(attributeName);
}

void SVGAnimationElement::calculateKeyTimesForCalcModePaced()
{
    ASSERT(calcMode() == CalcMode::Paced);
    ASSERT(animationMode() == AnimationMode::Values);

    unsigned valuesCount = m_values.size();
    ASSERT(valuesCount >= 1);
    if (valuesCount == 1)
        return;

    // FIXME, webkit.org/b/109010: m_keyTimes should not be modified in this function.
    m_keyTimes.clear();

    Vector<float> keyTimesForPaced;
    float totalDistance = 0;
    keyTimesForPaced.append(0);
    for (unsigned n = 0; n < valuesCount - 1; ++n) {
        // Distance in any units
        auto distance = calculateDistance(m_values[n], m_values[n + 1]);
        if (!distance)
            return;
        totalDistance += *distance;
        keyTimesForPaced.append(*distance);
    }
    if (!totalDistance)
        return;

    // Normalize.
    for (unsigned n = 1; n < keyTimesForPaced.size() - 1; ++n)
        keyTimesForPaced[n] = keyTimesForPaced[n - 1] + keyTimesForPaced[n] / totalDistance;
    keyTimesForPaced[keyTimesForPaced.size() - 1] = 1;

    // Use key times calculated based on pacing instead of the user provided ones.
    m_keyTimes = keyTimesForPaced;
}

static inline double solveEpsilon(double duration) { return 1 / (200 * duration); }

unsigned SVGAnimationElement::calculateKeyTimesIndex(float percent) const
{
    unsigned index;
    unsigned keyTimesCount = m_keyTimes.size();
    // Compare index + 1 to keyTimesCount because the last keyTimes entry is
    // required to be 1, and percent can never exceed 1; i.e., the second last
    // keyTimes entry defines the beginning of the final interval
    for (index = 1; index + 1 < keyTimesCount; ++index) {
        if (m_keyTimes[index] > percent)
            break;
    }
    return --index;
}

float SVGAnimationElement::calculatePercentForSpline(float percent, unsigned splineIndex) const
{
    ASSERT(calcMode() == CalcMode::Spline);
    ASSERT_WITH_SECURITY_IMPLICATION(splineIndex < m_keySplines.size());
    UnitBezier bezier = m_keySplines[splineIndex];
    SMILTime duration = simpleDuration();
    if (!duration.isFinite())
        duration = 100.0;
    return narrowPrecisionToFloat(bezier.solve(percent, solveEpsilon(duration.value())));
}

float SVGAnimationElement::calculatePercentFromKeyPoints(float percent) const
{
    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(calcMode() != CalcMode::Paced);
    ASSERT(m_keyTimes.size() > 1);
    ASSERT(m_keyPoints.size() == m_keyTimes.size());

    if (percent == 1)
        return m_keyPoints[m_keyPoints.size() - 1];

    unsigned index = calculateKeyTimesIndex(percent);
    float fromPercent = m_keyTimes[index];
    float toPercent = m_keyTimes[index + 1];
    float fromKeyPoint = m_keyPoints[index];
    float toKeyPoint = m_keyPoints[index + 1];
    
    if (calcMode() == CalcMode::Discrete)
        return fromKeyPoint;
    
    float keyPointPercent = (percent - fromPercent) / (toPercent - fromPercent);
    
    if (calcMode() == CalcMode::Spline) {
        ASSERT(m_keySplines.size() == m_keyPoints.size() - 1);
        keyPointPercent = calculatePercentForSpline(keyPointPercent, index);
    }
    return (toKeyPoint - fromKeyPoint) * keyPointPercent + fromKeyPoint;
}

float SVGAnimationElement::calculatePercentForFromTo(float percent) const
{
    if (calcMode() == CalcMode::Discrete && m_keyTimes.size() == 2)
        return percent > m_keyTimes[1] ? 1 : 0;

    return percent;
}
    
void SVGAnimationElement::currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const
{
    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(m_keyPoints.size() == m_keyTimes.size());
    ASSERT(calcMode() != CalcMode::Paced);
    effectivePercent = calculatePercentFromKeyPoints(percent);
    unsigned index = effectivePercent == 1 ? m_values.size() - 2 : static_cast<unsigned>(effectivePercent * (m_values.size() - 1));
    from = m_values[index];
    to = m_values[index + 1];
}
    
void SVGAnimationElement::currentValuesForValuesAnimation(float percent, float& effectivePercent, String& from, String& to)
{
    unsigned valuesCount = m_values.size();
    ASSERT(m_animationValid);
    ASSERT(valuesCount >= 1);

    if (percent == 1 || valuesCount == 1) {
        from = m_values[valuesCount - 1];
        to = m_values[valuesCount - 1];
        effectivePercent = 1;
        return;
    }

    CalcMode calcMode = this->calcMode();
    if (is<SVGAnimateElement>(*this) || is<SVGAnimateColorElement>(*this)) {
        ASSERT(targetElement());
        if (downcast<SVGAnimateElementBase>(*this).isDiscreteAnimator())
            calcMode = CalcMode::Discrete;
    }
    if (!m_keyPoints.isEmpty() && calcMode != CalcMode::Paced)
        return currentValuesFromKeyPoints(percent, effectivePercent, from, to);
    
    unsigned keyTimesCount = m_keyTimes.size();
    ASSERT(!keyTimesCount || valuesCount == keyTimesCount);
    ASSERT(!keyTimesCount || (keyTimesCount > 1 && !m_keyTimes[0]));

    unsigned index = calculateKeyTimesIndex(percent);
    if (calcMode == CalcMode::Discrete) {
        if (!keyTimesCount) 
            index = static_cast<unsigned>(percent * valuesCount);
        from = m_values[index];
        to = m_values[index];
        effectivePercent = 0;
        return;
    }
    
    float fromPercent;
    float toPercent;
    if (keyTimesCount) {
        fromPercent = m_keyTimes[index];
        toPercent = m_keyTimes[index + 1];
    } else {        
        index = static_cast<unsigned>(floorf(percent * (valuesCount - 1)));
        fromPercent =  static_cast<float>(index) / (valuesCount - 1);
        toPercent =  static_cast<float>(index + 1) / (valuesCount - 1);
    }
    
    if (index == valuesCount - 1)
        --index;
    from = m_values[index];
    to = m_values[index + 1];
    ASSERT_WITH_SECURITY_IMPLICATION(toPercent > fromPercent);
    effectivePercent = (percent - fromPercent) / (toPercent - fromPercent);

    if (calcMode == CalcMode::Spline) {
        ASSERT(m_keySplines.size() == m_values.size() - 1);
        effectivePercent = calculatePercentForSpline(effectivePercent, index);
    }
}
    
void SVGAnimationElement::startedActiveInterval()
{
    m_animationValid = false;

    if (!hasValidAttributeType())
        return;

    // These validations are appropriate for all animation modes.
    if (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && m_keyPoints.size() != m_keyTimes.size())
        return;

    AnimationMode animationMode = this->animationMode();
    CalcMode calcMode = this->calcMode();
    if (calcMode == CalcMode::Spline) {
        unsigned splinesCount = m_keySplines.size();
        if (!splinesCount
            || (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && m_keyPoints.size() - 1 != splinesCount)
            || (animationMode == AnimationMode::Values && m_values.size() - 1 != splinesCount)
            || (hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) && m_keyTimes.size() - 1 != splinesCount))
            return;
    }

    String from = fromValue();
    String to = toValue();
    String by = byValue();
    if (animationMode == AnimationMode::None)
        return;
    if ((animationMode == AnimationMode::FromTo || animationMode == AnimationMode::FromBy || animationMode == AnimationMode::To || animationMode == AnimationMode::By)
        && (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) && (m_keyTimes.size() < 2 || m_keyTimes.size() != m_keyPoints.size())))
        return;
    if (animationMode == AnimationMode::FromTo)
        m_animationValid = calculateFromAndToValues(from, to);
    else if (animationMode == AnimationMode::To) {
        // For to-animations the from value is the current accumulated value from lower priority animations.
        // The value is not static and is determined during the animation.
        m_animationValid = calculateFromAndToValues(emptyString(), to);
    } else if (animationMode == AnimationMode::FromBy)
        m_animationValid = calculateFromAndByValues(from, by);
    else if (animationMode == AnimationMode::By)
        m_animationValid = calculateFromAndByValues(emptyString(), by);
    else if (animationMode == AnimationMode::Values) {
        m_animationValid = m_values.size() >= 1
            && (calcMode == CalcMode::Paced || !hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) || hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (m_values.size() == m_keyTimes.size()))
            && (calcMode == CalcMode::Discrete || !m_keyTimes.size() || m_keyTimes.last() == 1)
            && (calcMode != CalcMode::Spline || ((m_keySplines.size() && (m_keySplines.size() == m_values.size() - 1)) || m_keySplines.size() == m_keyPoints.size() - 1))
            && (!hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (m_keyTimes.size() > 1 && m_keyTimes.size() == m_keyPoints.size()));
        if (m_animationValid)
            m_animationValid = calculateToAtEndOfDurationValue(m_values.last());
        if (calcMode == CalcMode::Paced && m_animationValid)
            calculateKeyTimesForCalcModePaced();
    } else if (animationMode == AnimationMode::Path)
        m_animationValid = calcMode == CalcMode::Paced || !hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (m_keyTimes.size() > 1 && m_keyTimes.size() == m_keyPoints.size());
}

void SVGAnimationElement::updateAnimation(float percent, unsigned repeatCount)
{    
    if (!m_animationValid)
        return;

    float effectivePercent;
    CalcMode calcMode = this->calcMode();
    AnimationMode animationMode = this->animationMode();
    if (animationMode == AnimationMode::Values) {
        String from;
        String to;
        currentValuesForValuesAnimation(percent, effectivePercent, from, to);
        if (from != m_lastValuesAnimationFrom || to != m_lastValuesAnimationTo) {
            m_animationValid = calculateFromAndToValues(from, to);
            if (!m_animationValid)
                return;
            m_lastValuesAnimationFrom = from;
            m_lastValuesAnimationTo = to;
        }
    } else if (!m_keyPoints.isEmpty() && calcMode != CalcMode::Paced)
        effectivePercent = calculatePercentFromKeyPoints(percent);
    else if (m_keyPoints.isEmpty() && calcMode == CalcMode::Spline && m_keyTimes.size() > 1)
        effectivePercent = calculatePercentForSpline(percent, calculateKeyTimesIndex(percent));
    else if (animationMode == AnimationMode::FromTo || animationMode == AnimationMode::To)
        effectivePercent = calculatePercentForFromTo(percent);
    else
        effectivePercent = percent;

    calculateAnimatedValue(effectivePercent, repeatCount);
}

void SVGAnimationElement::computeCSSPropertyValue(SVGElement* element, CSSPropertyID id, String& valueString)
{
    ASSERT(element);

    // Don't include any properties resulting from CSS Transitions/Animations or SMIL animations, as we want to retrieve the "base value".
    element->setUseOverrideComputedStyle(true);
    RefPtr<CSSValue> value = ComputedStyleExtractor(element).propertyValue(id);
    valueString = value ? value->cssText() : String();
    element->setUseOverrideComputedStyle(false);
}

static bool inheritsFromProperty(SVGElement* targetElement, const QualifiedName& attributeName, const String& value)
{
    static NeverDestroyed<const AtomString> inherit("inherit", AtomString::ConstructFromLiteral);
    
    if (value.isEmpty() || value != inherit)
        return false;
    return targetElement->isAnimatedStyleAttribute(attributeName);
}

void SVGAnimationElement::determinePropertyValueTypes(const String& from, const String& to)
{
    auto targetElement = makeRefPtr(this->targetElement());
    ASSERT(targetElement);

    const QualifiedName& attributeName = this->attributeName();
    if (inheritsFromProperty(targetElement.get(), attributeName, from))
        m_fromPropertyValueType = InheritValue;
    if (inheritsFromProperty(targetElement.get(), attributeName, to))
        m_toPropertyValueType = InheritValue;
}
void SVGAnimationElement::resetAnimation()
{
    m_lastValuesAnimationFrom = String();
    m_lastValuesAnimationTo = String();
}

}
