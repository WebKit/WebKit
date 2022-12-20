/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "HTMLParserIdioms.h"
#include "RenderObject.h"
#include "SVGAnimateColorElement.h"
#include "SVGAnimateElement.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGStringList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimationElement);

SVGAnimationElement::SVGAnimationElement(const QualifiedName& tagName, Document& document)
    : SVGSMILElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGTests(this)
{
}

static Vector<float> parseKeyTimes(StringView value, bool verifyOrder)
{
    auto keyTimes = value.split(';');
    Vector<float> result;

    for (auto keyTime : keyTimes) {
        keyTime = keyTime.stripWhiteSpace();

        bool ok;
        float time = keyTime.toFloat(ok);

        if (!ok || time < 0 || time > 1)
            return { };

        if (verifyOrder && (result.isEmpty() ? time : time < result.last()))
            return { };

        result.append(time);
    }

    return result;
}

static std::optional<Vector<UnitBezier>> parseKeySplines(StringView string)
{
    if (string.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(string, [&](auto buffer) -> std::optional<Vector<UnitBezier>> {
        skipOptionalSVGSpaces(buffer);

        Vector<UnitBezier> result;

        bool delimParsed = false;
        while (buffer.hasCharactersRemaining()) {
            delimParsed = false;
            auto posA = parseNumber(buffer);
            if (!posA || !isInRange<float>(*posA, 0, 1))
                return std::nullopt;

            auto posB = parseNumber(buffer);
            if (!posB || !isInRange<float>(*posB, 0, 1))
                return std::nullopt;

            auto posC = parseNumber(buffer);
            if (!posC || !isInRange<float>(*posC, 0, 1))
                return std::nullopt;

            auto posD = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
            if (!posD || !isInRange<float>(*posD, 0, 1))
                return std::nullopt;

            skipOptionalSVGSpaces(buffer);

            if (skipExactly(buffer, ';'))
                delimParsed = true;

            skipOptionalSVGSpaces(buffer);

            result.append(UnitBezier { *posA, *posB, *posC, *posD });
        }

        if (!(buffer.atEnd() && !delimParsed))
            return std::nullopt;

        return result;
    });
}

bool SVGAnimationElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed supportedAttributes = [] {
        MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName> set;
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
    }();
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

bool SVGAnimationElement::attributeContainsJavaScriptURL(const Attribute& attribute) const
{
    if (attribute.name() == SVGNames::fromAttr || attribute.name() == SVGNames::toAttr)
        return WTF::protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));

    if (attribute.name() == SVGNames::valuesAttr) {
        for (auto innerValue : StringView(attribute.value()).split(';')) {
            if (WTF::protocolIsJavaScript(innerValue.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>)))
                return true;
        }
        return false;
    }
    return Element::attributeContainsJavaScriptURL(attribute);
}

void SVGAnimationElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::valuesAttr) {
        // Per the SMIL specification, leading and trailing white space,
        // and white space before and after semicolon separators, is allowed and will be ignored.
        // http://www.w3.org/TR/SVG11/animate.html#ValuesAttribute
        m_values.clear();
        value.string().split(';', [this](StringView innerValue) {
            m_values.append(innerValue.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>).toString());
        });

        updateAnimationMode();
        return;
    }

    if (name == SVGNames::keyTimesAttr) {
        m_keyTimesFromAttribute = parseKeyTimes(value, true);
        return;
    }

    if (name == SVGNames::keyPointsAttr) {
        if (hasTagName(SVGNames::animateMotionTag)) {
            // This is specified to be an animateMotion attribute only but it is simpler to put it here 
            // where the other timing calculatations are.
            m_keyPoints = parseKeyTimes(value, false);
        }
        return;
    }

    if (name == SVGNames::keySplinesAttr) {
        if (auto keySplines = parseKeySplines(value))
            m_keySplines = WTFMove(*keySplines);
        else
            m_keySplines.clear();
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
    static MainThreadNeverDestroyed<const AtomString> discrete("discrete"_s);
    static MainThreadNeverDestroyed<const AtomString> linear("linear"_s);
    static MainThreadNeverDestroyed<const AtomString> paced("paced"_s);
    static MainThreadNeverDestroyed<const AtomString> spline("spline"_s);
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
    static MainThreadNeverDestroyed<const AtomString> css("CSS"_s);
    static MainThreadNeverDestroyed<const AtomString> xml("XML"_s);
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

static const AtomString& sumAtom()
{
    static MainThreadNeverDestroyed<const AtomString> sum("sum"_s);
    return sum;
}

bool SVGAnimationElement::isAdditive() const
{
    const AtomString& value = attributeWithoutSynchronization(SVGNames::additiveAttr);
    return value == sumAtom() || animationMode() == AnimationMode::By;
}

bool SVGAnimationElement::isAccumulated() const
{
    const AtomString& value = attributeWithoutSynchronization(SVGNames::accumulateAttr);
    return value == sumAtom() && animationMode() != AnimationMode::To;
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

    m_keyTimesForPaced.clear();

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

    m_keyTimesForPaced = WTFMove(keyTimesForPaced);
}

static inline double solveEpsilon(double duration) { return 1 / (200 * duration); }

const Vector<float>& SVGAnimationElement::keyTimes() const
{
    return calcMode() == CalcMode::Paced ? m_keyTimesForPaced : m_keyTimesFromAttribute;
}

unsigned SVGAnimationElement::calculateKeyTimesIndex(float percent) const
{
    const auto& keyTimes = this->keyTimes();
    unsigned index;
    unsigned keyTimesCount = keyTimes.size();
    // Compare index + 1 to keyTimesCount because the last keyTimes entry is
    // required to be 1, and percent can never exceed 1; i.e., the second last
    // keyTimes entry defines the beginning of the final interval
    for (index = 1; index + 1 < keyTimesCount; ++index) {
        if (keyTimes[index] > percent)
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
    const auto& keyTimes = this->keyTimes();

    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(calcMode() != CalcMode::Paced);
    ASSERT(keyTimes.size() > 1);
    ASSERT(m_keyPoints.size() == keyTimes.size());

    if (percent == 1)
        return m_keyPoints[m_keyPoints.size() - 1];

    unsigned index = calculateKeyTimesIndex(percent);
    float fromPercent = keyTimes[index];
    float toPercent = keyTimes[index + 1];
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
    const auto& keyTimes = this->keyTimes();
    if (calcMode() == CalcMode::Discrete && keyTimes.size() == 2)
        return percent > keyTimes[1] ? 1 : 0;

    return percent;
}
    
void SVGAnimationElement::currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const
{
    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(m_keyPoints.size() == keyTimes().size());
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
    
    const auto& keyTimes = this->keyTimes();
    unsigned keyTimesCount = keyTimes.size();
    ASSERT(!keyTimesCount || valuesCount == keyTimesCount);
    ASSERT(!keyTimesCount || (keyTimesCount > 1 && !keyTimes[0]));

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
        fromPercent = keyTimes[index];
        toPercent = keyTimes[index + 1];
    } else {        
        index = static_cast<unsigned>(floorf(percent * (valuesCount - 1)));
        fromPercent =  static_cast<float>(index) / (valuesCount - 1);
        toPercent =  static_cast<float>(index + 1) / (valuesCount - 1);
    }
    
    if (index == valuesCount - 1)
        --index;
    from = m_values[index];
    to = m_values[index + 1];
    ASSERT(toPercent > fromPercent);
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

    const auto& keyTimes = this->keyTimes();

    // These validations are appropriate for all animation modes.
    if (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && m_keyPoints.size() != keyTimes.size())
        return;

    AnimationMode animationMode = this->animationMode();
    CalcMode calcMode = this->calcMode();
    if (calcMode == CalcMode::Spline) {
        unsigned splinesCount = m_keySplines.size();
        if (!splinesCount
            || (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && m_keyPoints.size() - 1 != splinesCount)
            || (animationMode == AnimationMode::Values && m_values.size() - 1 != splinesCount)
            || (hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) && keyTimes.size() - 1 != splinesCount))
            return;
    }

    String from = fromValue();
    String to = toValue();
    String by = byValue();
    if (animationMode == AnimationMode::None)
        return;
    if ((animationMode == AnimationMode::FromTo || animationMode == AnimationMode::FromBy || animationMode == AnimationMode::To || animationMode == AnimationMode::By)
        && (hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) && hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) && (keyTimes.size() < 2 || keyTimes.size() != m_keyPoints.size())))
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
            && (calcMode == CalcMode::Paced || !hasAttributeWithoutSynchronization(SVGNames::keyTimesAttr) || hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (m_values.size() == keyTimes.size()))
            && (calcMode == CalcMode::Discrete || !keyTimes.size() || keyTimes.last() == 1)
            && (calcMode != CalcMode::Spline || ((m_keySplines.size() && (m_keySplines.size() == m_values.size() - 1)) || m_keySplines.size() == m_keyPoints.size() - 1))
            && (!hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (keyTimes.size() > 1 && keyTimes.size() == m_keyPoints.size()));
        if (m_animationValid)
            m_animationValid = calculateToAtEndOfDurationValue(m_values.last());
        if (calcMode == CalcMode::Paced && m_animationValid)
            calculateKeyTimesForCalcModePaced();
    } else if (animationMode == AnimationMode::Path)
        m_animationValid = calcMode == CalcMode::Paced || !hasAttributeWithoutSynchronization(SVGNames::keyPointsAttr) || (keyTimes.size() > 1 && keyTimes.size() == m_keyPoints.size());
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
    else if (m_keyPoints.isEmpty() && calcMode == CalcMode::Spline && keyTimes().size() > 1)
        effectivePercent = calculatePercentForSpline(percent, calculateKeyTimesIndex(percent));
    else if (animationMode == AnimationMode::FromTo || animationMode == AnimationMode::To)
        effectivePercent = calculatePercentForFromTo(percent);
    else
        effectivePercent = percent;

    calculateAnimatedValue(effectivePercent, repeatCount);
}

void SVGAnimationElement::resetAnimation()
{
    m_lastValuesAnimationFrom = String();
    m_lastValuesAnimationTo = String();
}

}
