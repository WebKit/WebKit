/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2008 Apple Inc. All rights reserved.
    Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG_ANIMATION)
#include "SVGAnimationElement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "FloatConversion.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "SVGUseElement.h"
#include "XLinkNames.h"
#include <math.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {
    
SVGAnimationElement::SVGAnimationElement(const QualifiedName& tagName, Document* doc)
    : SVGSMILElement(tagName, doc)
    , SVGTests()
    , SVGExternalResourcesRequired() 
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
    , m_animationValid(false)
{
}

SVGAnimationElement::~SVGAnimationElement()
{
}
    
static void parseKeyTimes(const String& parse, Vector<float>& result, bool verifyOrder)
{
    result.clear();
    Vector<String> parseList;
    parse.split(';', parseList);
    for (unsigned n = 0; n < parseList.size(); ++n) {
        String timeString = parseList[n];
        bool ok;
        float time = timeString.toFloat(&ok);
        if (!ok || time < 0 || time > 1.f)
            goto fail;
        if (verifyOrder) {
            if (!n) {
                if (time != 0)
                    goto fail;
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
    Vector<String> parseList;
    parse.split(';', parseList);
    for (unsigned n = 0; n < parseList.size(); ++n) {
        Vector<String> parseSpline;
        parseList[n].split(',', parseSpline);
        // The spec says the sepator is a space, all tests use commas. Weird.
        if (parseSpline.size() == 1) 
            parseList[n].split(' ', parseSpline);
        if (parseSpline.size() != 4)
            goto fail;
        double curveValues[4];
        for (unsigned i = 0; i < 4; ++i) {
            String parseNumber = parseSpline[i]; 
            bool ok;
            curveValues[i] = parseNumber.toDouble(&ok);
            if (!ok || curveValues[i] < 0.0 || curveValues[i] > 1.0)
                goto fail;
        }
        result.append(UnitBezier(curveValues[0], curveValues[1], curveValues[2], curveValues[3]));
    }
    return;
fail:
    result.clear();
}

void SVGAnimationElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::valuesAttr)
        attr->value().string().split(';', m_values);
    else if (attr->name() == SVGNames::keyTimesAttr)
        parseKeyTimes(attr->value(), m_keyTimes, true);
    else if (attr->name() == SVGNames::keyPointsAttr && hasTagName(SVGNames::animateMotionTag)) {
        // This is specified to be an animateMotion attribute only but it is simpler to put it here 
        // where the other timing calculatations are.
        parseKeyTimes(attr->value(), m_keyPoints, false);
    } else if (attr->name() == SVGNames::keySplinesAttr)
        parseKeySplines(attr->value(), m_keySplines);
    else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGSMILElement::parseMappedAttribute(attr);
    }
}

void SVGAnimationElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    // Assumptions may not hold after an attribute change.
    m_animationValid = false;
    SVGSMILElement::attributeChanged(attr, preserveDecls);
}

float SVGAnimationElement::getStartTime() const
{
    return narrowPrecisionToFloat(intervalBegin().value());
}

float SVGAnimationElement::getCurrentTime() const
{
    return narrowPrecisionToFloat(elapsed().value());
}

float SVGAnimationElement::getSimpleDuration(ExceptionCode&) const
{
    return narrowPrecisionToFloat(simpleDuration().value());
}    
    
void SVGAnimationElement::beginElement()
{
    beginElementAt(0);
}

void SVGAnimationElement::beginElementAt(float offset)
{
    addBeginTime(elapsed() + offset);
}

void SVGAnimationElement::endElement()
{
    endElementAt(0);
}

void SVGAnimationElement::endElementAt(float offset)
{
    addEndTime(elapsed() + offset);
}

SVGAnimationElement::AnimationMode SVGAnimationElement::animationMode() const
{
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#AnimFuncValues
    if (hasTagName(SVGNames::setTag))
        return ToAnimation;
    if (!animationPath().isEmpty())
        return PathAnimation;
    if (hasAttribute(SVGNames::valuesAttr))
        return ValuesAnimation;
    if (!toValue().isEmpty())
        return fromValue().isEmpty() ? ToAnimation : FromToAnimation;
    if (!byValue().isEmpty())
        return fromValue().isEmpty() ? ByAnimation : FromByAnimation;
    return NoAnimation;
}

SVGAnimationElement::CalcMode SVGAnimationElement::calcMode() const
{    
    DEFINE_STATIC_LOCAL(const AtomicString, discrete, ("discrete"));
    DEFINE_STATIC_LOCAL(const AtomicString, linear, ("linear"));
    DEFINE_STATIC_LOCAL(const AtomicString, paced, ("paced"));
    DEFINE_STATIC_LOCAL(const AtomicString, spline, ("spline"));
    const AtomicString& value = getAttribute(SVGNames::calcModeAttr);
    if (value == discrete)
        return CalcModeDiscrete;
    if (value == linear)
        return CalcModeLinear;
    if (value == paced)
        return CalcModePaced;
    if (value == spline)
        return CalcModeSpline;
    return hasTagName(SVGNames::animateMotionTag) ? CalcModePaced : CalcModeLinear;
}

SVGAnimationElement::AttributeType SVGAnimationElement::attributeType() const
{    
    DEFINE_STATIC_LOCAL(const AtomicString, css, ("CSS"));
    DEFINE_STATIC_LOCAL(const AtomicString, xml, ("XML"));
    const AtomicString& value = getAttribute(SVGNames::attributeTypeAttr);
    if (value == css)
        return AttributeTypeCSS;
    if (value == xml)
        return AttributeTypeXML;
    return AttributeTypeAuto;
}

String SVGAnimationElement::toValue() const
{    
    return getAttribute(SVGNames::toAttr);
}

String SVGAnimationElement::byValue() const
{    
    return getAttribute(SVGNames::byAttr);
}

String SVGAnimationElement::fromValue() const
{    
    return getAttribute(SVGNames::fromAttr);
}

bool SVGAnimationElement::isAdditive() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
    const AtomicString& value = getAttribute(SVGNames::additiveAttr);
    return value == sum || animationMode() == ByAnimation;
}

bool SVGAnimationElement::isAccumulated() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
    const AtomicString& value = getAttribute(SVGNames::accumulateAttr);
    return value == sum && animationMode() != ToAnimation;
}

bool SVGAnimationElement::hasValidTarget() const
{
    return targetElement();
}
    
bool SVGAnimationElement::attributeIsCSS(const String& attributeName)
{
    // FIXME: We should have a map of all SVG properties and their attribute types so we
    // could validate animations better. The spec is very vague about this.
    unsigned id = cssPropertyID(attributeName);
    // SVG range
    if (id >= CSSPropertyClipPath && id <= CSSPropertyWritingMode)
        return true;
    // Regular CSS properties also in SVG
    return id == CSSPropertyColor || id == CSSPropertyDisplay || id == CSSPropertyOpacity
            || (id >= CSSPropertyFont && id <= CSSPropertyFontWeight) 
            || id == CSSPropertyOverflow || id == CSSPropertyVisibility;
}
    
bool SVGAnimationElement::targetAttributeIsCSS() const
{
    AttributeType type = attributeType();
    if (type == AttributeTypeCSS)
        return true;
    if (type == AttributeTypeXML)
        return false;
    return attributeIsCSS(attributeName());
}

void SVGAnimationElement::setTargetAttributeAnimatedValue(const String& value)
{
    if (!hasValidTarget())
        return;
    SVGElement* target = targetElement();
    String attributeName = this->attributeName();
    if (!target || attributeName.isEmpty() || value.isNull())
        return;

    // We don't want the instance tree to get rebuild. Instances are updated in the loop below.
    if (target->isStyled())
        static_cast<SVGStyledElement*>(target)->setInstanceUpdatesBlocked(true);
        
    ExceptionCode ec;
    bool isCSS = targetAttributeIsCSS();
    if (isCSS) {
        // FIXME: This should set the override style, not the inline style.
        // Sadly override styles are not yet implemented.
        target->style()->setProperty(attributeName, value, "", ec);
    } else {
        // FIXME: This should set the 'presentation' value, not the actual 
        // attribute value. Whatever that means in practice.
        target->setAttribute(attributeName, value, ec);
    }
    
    if (target->isStyled())
        static_cast<SVGStyledElement*>(target)->setInstanceUpdatesBlocked(false);
    
    // If the target element is used in an <use> instance tree, update that as well.
    HashSet<SVGElementInstance*> instances = target->instancesForElement();
    HashSet<SVGElementInstance*>::iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::iterator it = instances.begin(); it != end; ++it) {
        SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
        ASSERT(shadowTreeElement);
        if (isCSS)
            shadowTreeElement->style()->setProperty(attributeName, value, "", ec);
        else
            shadowTreeElement->setAttribute(attributeName, value, ec);
        (*it)->correspondingUseElement()->setNeedsStyleRecalc();
    }
}
    
void SVGAnimationElement::calculateKeyTimesForCalcModePaced()
{
    ASSERT(calcMode() == CalcModePaced);
    ASSERT(animationMode() == ValuesAnimation);

    unsigned valuesCount = m_values.size();
    ASSERT(valuesCount > 1);
    Vector<float> keyTimesForPaced;
    float totalDistance = 0;
    keyTimesForPaced.append(0);
    for (unsigned n = 0; n < valuesCount - 1; ++n) {
        // Distance in any units
        float distance = calculateDistance(m_values[n], m_values[n + 1]);
        if (distance < 0)
            return;
        totalDistance += distance;
        keyTimesForPaced.append(distance);
    }
    if (!totalDistance)
        return;

    // Normalize.
    for (unsigned n = 1; n < keyTimesForPaced.size() - 1; ++n)
        keyTimesForPaced[n] = keyTimesForPaced[n - 1] + keyTimesForPaced[n] / totalDistance;
    keyTimesForPaced[keyTimesForPaced.size() - 1] = 1.f;

    // Use key times calculated based on pacing instead of the user provided ones.
    m_keyTimes.swap(keyTimesForPaced);
}

static inline double solveEpsilon(double duration) { return 1. / (200. * duration); }
    
float SVGAnimationElement::calculatePercentForSpline(float percent, unsigned splineIndex) const
{
    ASSERT(calcMode() == CalcModeSpline);
    ASSERT(splineIndex < m_keySplines.size());
    UnitBezier bezier = m_keySplines[splineIndex];
    SMILTime duration = simpleDuration();
    if (!duration.isFinite())
        duration = 100.0;
    return narrowPrecisionToFloat(bezier.solve(percent, solveEpsilon(duration.value())));
}

float SVGAnimationElement::calculatePercentFromKeyPoints(float percent) const
{
    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(calcMode() != CalcModePaced);
    unsigned keyTimesCount = m_keyTimes.size();
    ASSERT(keyTimesCount > 1);
    ASSERT(m_keyPoints.size() == keyTimesCount);

    unsigned index;
    for (index = 1; index < keyTimesCount; ++index) {
        if (m_keyTimes[index] >= percent)
            break;
    }
    --index;

    float fromPercent = m_keyTimes[index];
    float toPercent = m_keyTimes[index + 1];
    float fromKeyPoint = m_keyPoints[index];
    float toKeyPoint = m_keyPoints[index + 1];
    
    if (calcMode() == CalcModeDiscrete)
        return percent == 1.0f ? toKeyPoint : fromKeyPoint;
    
    float keyPointPercent = percent == 1.0f ? 1.0f : (percent - fromPercent) / (toPercent - fromPercent);
    
    if (calcMode() == CalcModeSpline) {
        ASSERT(m_keySplines.size() == m_keyPoints.size() - 1);
        keyPointPercent = calculatePercentForSpline(keyPointPercent, index);
    }
    return (toKeyPoint - fromKeyPoint) * keyPointPercent + fromKeyPoint;
}
    
void SVGAnimationElement::currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const
{
    ASSERT(!m_keyPoints.isEmpty());
    ASSERT(m_keyPoints.size() == m_keyTimes.size());
    ASSERT(calcMode() != CalcModePaced);
    effectivePercent = calculatePercentFromKeyPoints(percent);
    unsigned index = effectivePercent == 1.0f ? m_values.size() - 2 : static_cast<unsigned>(effectivePercent * (m_values.size() - 1));
    from = m_values[index];
    to = m_values[index + 1];
}
    
void SVGAnimationElement::currentValuesForValuesAnimation(float percent, float& effectivePercent, String& from, String& to) const
{
    unsigned valuesCount = m_values.size();
    ASSERT(m_animationValid);
    ASSERT(valuesCount > 1);
    
    CalcMode calcMode = this->calcMode();
    if (!m_keyPoints.isEmpty() && calcMode != CalcModePaced)
        return currentValuesFromKeyPoints(percent, effectivePercent, from, to);
    
    unsigned keyTimesCount = m_keyTimes.size();
    ASSERT(!keyTimesCount || valuesCount == keyTimesCount);
    ASSERT(!keyTimesCount || (keyTimesCount > 1 && m_keyTimes[0] == 0));

    unsigned index;
    for (index = 1; index < keyTimesCount; ++index) {
        if (m_keyTimes[index] >= percent)
            break;
    }
    --index;
    
    if (calcMode == CalcModeDiscrete) {
        if (!keyTimesCount) 
            index = percent == 1.0f ? valuesCount - 1 : static_cast<unsigned>(percent * valuesCount);
        from = m_values[index];
        to = m_values[index];
        effectivePercent = 0.0f;
        return;
    }
    
    float fromPercent;
    float toPercent;
    if (keyTimesCount) {
        fromPercent = m_keyTimes[index];
        toPercent = m_keyTimes[index + 1];
    } else {        
        index = static_cast<unsigned>(percent * (valuesCount - 1));
        fromPercent =  static_cast<float>(index) / (valuesCount - 1);
        toPercent =  static_cast<float>(index + 1) / (valuesCount - 1);
    }
    
    if (index == valuesCount - 1)
        --index;
    from = m_values[index];
    to = m_values[index + 1];
    ASSERT(toPercent > fromPercent);
    effectivePercent = percent == 1.0f ? 1.0f : (percent - fromPercent) / (toPercent - fromPercent);
    
    if (calcMode == CalcModeSpline) {
        ASSERT(m_keySplines.size() == m_values.size() - 1);
        effectivePercent = calculatePercentForSpline(effectivePercent, index);
    }
}
    
void SVGAnimationElement::startedActiveInterval()
{
    m_animationValid = false;

    if (!hasValidTarget())
        return;

    AnimationMode animationMode = this->animationMode();
    if (animationMode == NoAnimation)
        return;
    if (animationMode == FromToAnimation)
        m_animationValid = calculateFromAndToValues(fromValue(), toValue());
    else if (animationMode == ToAnimation) {
        // For to-animations the from value is the current accumulated value from lower priority animations.
        // The value is not static and is determined during the animation.
        m_animationValid = calculateFromAndToValues(String(), toValue());
    } else if (animationMode == FromByAnimation)
        m_animationValid = calculateFromAndByValues(fromValue(), byValue());
    else if (animationMode == ByAnimation)
        m_animationValid = calculateFromAndByValues(String(), byValue());
    else if (animationMode == ValuesAnimation) {
        CalcMode calcMode = this->calcMode();
        m_animationValid = m_values.size() > 1
            && (calcMode == CalcModePaced || !hasAttribute(SVGNames::keyTimesAttr) || hasAttribute(SVGNames::keyPointsAttr) || (m_values.size() == m_keyTimes.size()))
            && (calcMode == CalcModeDiscrete || !m_keyTimes.size() || m_keyTimes.last() == 1.0)
            && (calcMode != CalcModeSpline || ((m_keySplines.size() && (m_keySplines.size() == m_values.size() - 1)) || m_keySplines.size() == m_keyPoints.size() - 1))
            && (!hasAttribute(SVGNames::keyPointsAttr) || (m_keyTimes.size() > 1 && m_keyTimes.size() == m_keyPoints.size()));
        if (calcMode == CalcModePaced && m_animationValid)
            calculateKeyTimesForCalcModePaced();
    } else if (animationMode == PathAnimation)
        m_animationValid = calcMode() == CalcModePaced || !hasAttribute(SVGNames::keyPointsAttr) || (m_keyTimes.size() > 1 && m_keyTimes.size() == m_keyPoints.size());
}
    
void SVGAnimationElement::updateAnimation(float percent, unsigned repeat, SVGSMILElement* resultElement)
{    
    if (!m_animationValid)
        return;
    
    float effectivePercent;
    if (animationMode() == ValuesAnimation) {
        String from;
        String to;
        currentValuesForValuesAnimation(percent, effectivePercent, from, to);
        if (from != m_lastValuesAnimationFrom || to != m_lastValuesAnimationTo ) {
            m_animationValid = calculateFromAndToValues(from, to);
            if (!m_animationValid)
                return;
            m_lastValuesAnimationFrom = from;
            m_lastValuesAnimationTo = to;
        }
    } else if (!m_keyPoints.isEmpty() && calcMode() != CalcModePaced)
        effectivePercent = calculatePercentFromKeyPoints(percent);
    else 
        effectivePercent = percent;

    calculateAnimatedValue(effectivePercent, repeat, resultElement);
}

void SVGAnimationElement::endedActiveInterval()
{
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG_ANIMATION)

