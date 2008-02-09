/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

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
#if ENABLE(SVG)
#include "SVGAnimationElement.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "FloatConversion.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "TimeScheduler.h"
#include "XLinkNames.h"
#include <float.h>
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

SVGAnimationElement::SVGAnimationElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , SVGTests()
    , SVGExternalResourcesRequired()
    , m_targetElement(0)
    , m_connectedToTimer(false)
    , m_currentTime(0.0)
    , m_simpleDuration(0.0)
    , m_fill(FILL_REMOVE)
    , m_restart(RESTART_ALWAYS)
    , m_calcMode(CALCMODE_LINEAR)
    , m_additive(ADDITIVE_REPLACE)
    , m_accumulate(ACCUMULATE_NONE)
    , m_attributeType(ATTRIBUTETYPE_AUTO)
    , m_max(0.0)
    , m_min(0.0)
    , m_end(0.0)
    , m_begin(0.0)
    , m_repetitions(0)
    , m_repeatCount(0)
{

}

SVGAnimationElement::~SVGAnimationElement()
{
}

bool SVGAnimationElement::hasValidTarget() const
{
    return targetElement();
}

SVGElement* SVGAnimationElement::targetElement() const
{
    if (!m_targetElement) {
        Node *target = 0;
        if (!m_href.isEmpty()) {
            target = document()->getElementById(SVGURIReference::getTarget(m_href));
        } else if (parentNode()) {
            // TODO : do we really need to skip non element nodes? Can that happen at all?
            target = parentNode();
            while (target) {
                if (target->nodeType() != ELEMENT_NODE)
                    target = target->parentNode();
                else
                    break;
            }
        }
        if (target && target->isSVGElement())
            m_targetElement = static_cast<SVGElement*>(target);
    }
 
    return m_targetElement;
}

float SVGAnimationElement::getEndTime() const
{
    return narrowPrecisionToFloat(m_end);
}

float SVGAnimationElement::getStartTime() const
{
    return narrowPrecisionToFloat(m_begin);
}

float SVGAnimationElement::getCurrentTime() const
{
    return narrowPrecisionToFloat(m_currentTime);
}

float SVGAnimationElement::getSimpleDuration(ExceptionCode&) const
{
    return narrowPrecisionToFloat(m_simpleDuration);
}

void SVGAnimationElement::parseKeyNumbers(Vector<float>& keyNumbers, const String& value)
{
    float number = 0.0f;
    
    const UChar* ptr = value.characters();
    const UChar* end = ptr + value.length();
    skipOptionalSpaces(ptr, end);
    while (ptr < end) {
        if (!parseNumber(ptr, end, number, false))
            return;
        keyNumbers.append(number);
        
        if (!skipOptionalSpaces(ptr, end) || *ptr != ';')
            return;
        ptr++;
        skipOptionalSpaces(ptr, end);
    }
}

static void parseKeySplines(Vector<SVGAnimationElement::KeySpline>& keySplines, const String& value)
{
    float number = 0.0f;
    SVGAnimationElement::KeySpline keySpline;
    
    const UChar* ptr = value.characters();
    const UChar* end = ptr + value.length();
    skipOptionalSpaces(ptr, end);
    while (ptr < end) {
        if (!(parseNumber(ptr, end, number, false) && skipOptionalSpaces(ptr, end)))
            return;
        keySpline.control1.setX(number);
        if (!(parseNumber(ptr, end, number, false) && skipOptionalSpaces(ptr, end)))
            return;
        keySpline.control1.setY(number);
        if (!(parseNumber(ptr, end, number, false) && skipOptionalSpaces(ptr, end)))
            return;
        keySpline.control2.setX(number);
        if (!parseNumber(ptr, end, number, false))
            return;
        keySpline.control2.setY(number);
        keySplines.append(keySpline);
        
        if (!skipOptionalSpaces(ptr, end) || *ptr != ';')
            return;
        ptr++;
        skipOptionalSpaces(ptr, end);
    }
}

void SVGAnimationElement::parseBeginOrEndValue(double& number, const String& value)
{
    // TODO: Don't use SVGStringList for parsing.
    AtomicString dummy;
    RefPtr<SVGStringList> valueList = new SVGStringList(QualifiedName(dummy, dummy, dummy));
    valueList->parse(value, ';');
    
    ExceptionCode ec = 0;
    for (unsigned int i = 0; i < valueList->numberOfItems(); i++) {
        String current = valueList->getItem(i, ec);
        
        if (current.startsWith("accessKey")) {
            // Register keyDownEventListener for the character
            String character = current.substring(current.length() - 2, 1);
            // FIXME: Not implemented! Supposed to register accessKey character
        } else if (current.startsWith("wallclock")) {
            int firstBrace = current.find('(');
            int secondBrace = current.find(')');
            
            String wallclockValue = current.substring(firstBrace + 1, secondBrace - firstBrace - 2);
            // FIXME: Not implemented! Supposed to use wallClock value
        } else if (current.contains('.')) {
            int dotPosition = current.find('.');
            
            String element = current.substring(0, dotPosition);
            String clockValue;
            if (current.contains("begin"))
                clockValue = current.substring(dotPosition + 6);
            else if (current.contains("end"))
                clockValue = current.substring(dotPosition + 4);
            else if (current.contains("repeat"))
                clockValue = current.substring(dotPosition + 7);
            else { // DOM2 Event Reference
                int plusMinusPosition = -1;
                
                if (current.contains('+'))
                    plusMinusPosition = current.find('+');
                else if (current.contains('-'))
                    plusMinusPosition = current.find('-');
                
                String event = current.substring(dotPosition + 1, plusMinusPosition - dotPosition - 1);
                clockValue = current.substring(dotPosition + event.length() + 1);
                // FIXME: supposed to use DOM Event
            }
        } else {
            number = parseClockValue(current);
            if (!isIndefinite(number))
                number *= 1000.0;
            // FIXME: supposed to set begin/end time
        }
    }    
}

void SVGAnimationElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name().matches(XLinkNames::hrefAttr))
        m_href = attr->value();
    else if (attr->name() == SVGNames::attributeNameAttr)
        m_attributeName = attr->value();
    else if (attr->name() == SVGNames::attributeTypeAttr) {
        if (attr->value() == "CSS")
            m_attributeType = ATTRIBUTETYPE_CSS;
        else if (attr->value() == "XML")
            m_attributeType = ATTRIBUTETYPE_XML;
        else if (attr->value() == "auto")
            m_attributeType = ATTRIBUTETYPE_AUTO;
    } else if (attr->name() == SVGNames::beginAttr)
        parseBeginOrEndValue(m_begin, attr->value());
    else if (attr->name() == SVGNames::endAttr)
        parseBeginOrEndValue(m_end, attr->value());
    else if (attr->name() == SVGNames::durAttr) {
        m_simpleDuration = parseClockValue(attr->value());
        if (!isIndefinite(m_simpleDuration))
            m_simpleDuration *= 1000.0;
    } else if (attr->name() == SVGNames::minAttr) {
        m_min = parseClockValue(attr->value());
        if (!isIndefinite(m_min))
            m_min *= 1000.0;
    } else if (attr->name() == SVGNames::maxAttr) {
        m_max = parseClockValue(attr->value());
        if (!isIndefinite(m_max))
            m_max *= 1000.0;
    } else if (attr->name() == SVGNames::restartAttr) {
        if (attr->value() == "whenNotActive")
            m_restart = RESTART_WHENNOTACTIVE;
        else if (attr->value() == "never")
            m_restart = RESTART_NEVER;
        else if (attr->value() == "always")
            m_restart = RESTART_ALWAYS;
    } else if (attr->name() == SVGNames::repeatCountAttr) {
        if (attr->value() == "indefinite")
            m_repeatCount = DBL_MAX;
        else
            m_repeatCount = attr->value().toDouble();
    } else if (attr->name() == SVGNames::repeatDurAttr)
        m_repeatDur = attr->value();
    else if (attr->name() == SVGNames::fillAttr) {
        if (attr->value() == "freeze")
            m_fill = FILL_FREEZE;
        else if (attr->value() == "remove")
            m_fill = FILL_REMOVE;
    } else if (attr->name() == SVGNames::calcModeAttr) {
        if (attr->value() == "discrete")
            m_calcMode = CALCMODE_DISCRETE;
        else if (attr->value() == "linear")
            m_calcMode = CALCMODE_LINEAR;
        else if (attr->value() == "spline")
            m_calcMode = CALCMODE_SPLINE;
        else if (attr->value() == "paced")
            m_calcMode = CALCMODE_PACED;
    } else if (attr->name() == SVGNames::valuesAttr) {
        m_values.clear();
        m_values = parseDelimitedString(attr->value(), ';');
    } else if (attr->name() == SVGNames::keyTimesAttr) {
        m_keyTimes.clear();
        parseKeyNumbers(m_keyTimes, attr->value());
    } else if (attr->name() == SVGNames::keySplinesAttr) {
        m_keySplines.clear();
        parseKeySplines(m_keySplines, attr->value());
    } else if (attr->name() == SVGNames::fromAttr)
        m_from = attr->value();
    else if (attr->name() == SVGNames::toAttr)
        m_to = attr->value();
    else if (attr->name() == SVGNames::byAttr)
        m_by = attr->value();
    else if (attr->name() == SVGNames::additiveAttr) {
        if (attr->value() == "sum")
            m_additive = ADDITIVE_SUM;
        else if (attr->value() == "replace")
            m_additive = ADDITIVE_REPLACE;
    } else if (attr->name() == SVGNames::accumulateAttr) {
        if (attr->value() == "sum")
            m_accumulate = ACCUMULATE_SUM;
        else if (attr->value() == "none")
            m_accumulate = ACCUMULATE_NONE;
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGElement::parseMappedAttribute(attr);
    }
}

double SVGAnimationElement::parseClockValue(const String& data)
{
    DeprecatedString parse = data.deprecatedString().stripWhiteSpace();
    
    if (parse == "indefinite") // Saves some time...
        return DBL_MAX;

    double result;

    int doublePointOne = parse.find(':');
    int doublePointTwo = parse.find(':', doublePointOne + 1);

    if (doublePointOne != -1 && doublePointTwo != -1) { // Spec: "Full clock values"
        unsigned int hours = parse.mid(0, 2).toUInt();
        unsigned int minutes = parse.mid(3, 2).toUInt();
        unsigned int seconds = parse.mid(6, 2).toUInt();
        unsigned int milliseconds = 0;

        result = (3600 * hours) + (60 * minutes) + seconds;

        if (parse.find('.') != -1) {
            DeprecatedString temp = parse.mid(9, 2);
            milliseconds = temp.toUInt();
            result += (milliseconds * (1 / pow(10.0, int(temp.length()))));
        }
    } else if (doublePointOne != -1 && doublePointTwo == -1) { // Spec: "Partial clock values"
        unsigned int minutes = parse.mid(0, 2).toUInt();
        unsigned int seconds = parse.mid(3, 2).toUInt();
        unsigned int milliseconds = 0;

        result = (60 * minutes) + seconds;

        if (parse.find('.') != -1) {
            DeprecatedString temp = parse.mid(6, 2);
            milliseconds = temp.toUInt();
            result += (milliseconds * (1 / pow(10.0, int(temp.length()))));
        }
    } else { // Spec: "Timecount values"
        int dotPosition = parse.find('.');

        if (parse.endsWith("h")) {
            if (dotPosition == -1)
                result = parse.mid(0, parse.length() - 1).toUInt() * 3600;
            else {
                result = parse.mid(0, dotPosition).toUInt() * 3600;
                DeprecatedString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 2);
                result += (3600.0 * temp.toUInt()) * (1 / pow(10.0, int(temp.length())));
            }
        } else if (parse.endsWith("min")) {
            if (dotPosition == -1)
                result = parse.mid(0, parse.length() - 3).toUInt() * 60;
            else {
                result = parse.mid(0, dotPosition).toUInt() * 60;
                DeprecatedString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 4);
                result += (60.0 * temp.toUInt()) * (1 / pow(10.0, int(temp.length())));
            }
        } else if (parse.endsWith("ms")) {
            if (dotPosition == -1)
                result = parse.mid(0, parse.length() - 2).toUInt() / 1000.0;
            else {
                result = parse.mid(0, dotPosition).toUInt() / 1000.0;
                DeprecatedString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 3);
                result += (temp.toUInt() / 1000.0) * (1 / pow(10.0, int(temp.length())));
            }
        } else if (parse.endsWith("s")) {
            if (dotPosition == -1)
                result = parse.mid(0, parse.length() - 1).toUInt();
            else {
                result = parse.mid(0, dotPosition).toUInt();
                DeprecatedString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 2);
                result += temp.toUInt() * (1 / pow(10.0, int(temp.length())));
            }
        } else
            result = parse.toDouble();
    }

    return result;
}

void SVGAnimationElement::finishParsingChildren()
{
    if (ownerSVGElement())
        ownerSVGElement()->timeScheduler()->addTimer(this, lround(getStartTime()));
    SVGElement::finishParsingChildren();
}

String SVGAnimationElement::targetAttributeAnimatedValue() const
{
    // FIXME: This method is not entirely correct
    // It does not properly grab the true "animVal" instead grabs the baseVal (or something very close)

    if (!targetElement())
        return String();
    
    SVGElement* target = targetElement();
    SVGStyledElement* styled = 0;
    if (target && target->isStyled())
        styled = static_cast<SVGStyledElement*>(target);
    
    String ret;

    EAttributeType attributeType = static_cast<EAttributeType>(m_attributeType);
    if (attributeType == ATTRIBUTETYPE_AUTO) {
        attributeType = ATTRIBUTETYPE_XML;

        // Spec: The implementation should match the attributeName to an attribute
        // for the target element. The implementation must first search through the
        // list of CSS properties for a matching property name, and if none is found,
        // search the default XML namespace for the element.
        if (styled && styled->style()) {
            if (styled->style()->getPropertyCSSValue(m_attributeName))
                attributeType = ATTRIBUTETYPE_CSS;
        }
    }
    
    if (attributeType == ATTRIBUTETYPE_CSS) {
        if (styled && styled->style())
            ret = styled->style()->getPropertyValue(m_attributeName);
    }

    if (attributeType == ATTRIBUTETYPE_XML || ret.isEmpty())
        ret = targetElement()->getAttribute(m_attributeName);

    return ret;
}

void SVGAnimationElement::setTargetAttributeAnimatedValue(const String& value)
{
    // FIXME: This method is not entirely correct
    // It does not properly set the "animVal", rather it sets the "baseVal"
    SVGAnimationElement::setTargetAttribute(targetElement(), m_attributeName, value, static_cast<EAttributeType>(m_attributeType));
}

void SVGAnimationElement::setTargetAttribute(SVGElement* target, const String& name, const String& value, EAttributeType type)
{
    if (!target || name.isNull() || value.isNull())
        return;
    
    SVGStyledElement* styled = (target && target->isStyled()) ? static_cast<SVGStyledElement*>(target) : 0;

    EAttributeType attributeType = type;
    if (type == ATTRIBUTETYPE_AUTO) {
        // Spec: The implementation should match the attributeName to an attribute
        // for the target element. The implementation must first search through the
        // list of CSS properties for a matching property name, and if none is found,
        // search the default XML namespace for the element.
        if (styled && styled->style() && styled->style()->getPropertyCSSValue(name))
            attributeType = ATTRIBUTETYPE_CSS;
        else
            attributeType = ATTRIBUTETYPE_XML;
    }
    ExceptionCode ec = 0;
    if (attributeType == ATTRIBUTETYPE_CSS && styled && styled->style())
        styled->style()->setProperty(name, value, "", ec);
    else if (attributeType == ATTRIBUTETYPE_XML)
        target->setAttribute(name, value, ec);
}

String SVGAnimationElement::attributeName() const
{
    return m_attributeName;
}

bool SVGAnimationElement::connectedToTimer() const
{
    return m_connectedToTimer;
}

bool SVGAnimationElement::isFrozen() const
{
    return (m_fill == FILL_FREEZE);
}

bool SVGAnimationElement::isAdditive() const
{
    return (m_additive == ADDITIVE_SUM) || (detectAnimationMode() == BY_ANIMATION);
}

bool SVGAnimationElement::isAccumulated() const
{
    return (m_accumulate == ACCUMULATE_SUM) && (detectAnimationMode() != TO_ANIMATION);
}

EAnimationMode SVGAnimationElement::detectAnimationMode() const
{
    if (hasAttribute(SVGNames::valuesAttr))
        return VALUES_ANIMATION;
    else if ((!m_from.isEmpty() && !m_to.isEmpty()) || (!m_to.isEmpty())) { // to/from-to animation
        if (!m_from.isEmpty()) // from-to animation
            return FROM_TO_ANIMATION;
        else
            return TO_ANIMATION;
    } else if ((m_from.isEmpty() && m_to.isEmpty() && !m_by.isEmpty()) ||
            (!m_from.isEmpty() && !m_by.isEmpty())) { // by/from-by animation
        if (!m_from.isEmpty()) // from-by animation
            return FROM_BY_ANIMATION;
        else
            return BY_ANIMATION;
    }

    return NO_ANIMATION;
}

double SVGAnimationElement::repeations() const
{
    return m_repetitions;
}

bool SVGAnimationElement::isIndefinite(double value)
{
    return (value == DBL_MAX);
}

void SVGAnimationElement::connectTimer()
{
    ASSERT(!m_connectedToTimer);
    ownerSVGElement()->timeScheduler()->connectIntervalTimer(this);
    m_connectedToTimer = true;
}

void SVGAnimationElement::disconnectTimer()
{
    ASSERT(m_connectedToTimer);
    ownerSVGElement()->timeScheduler()->disconnectIntervalTimer(this);
    m_connectedToTimer = false;
}

static double calculateTimePercentage(double elapsedSeconds, double start, double end, double duration, double repetitions)
{
    double percentage = 0.0;
    double useElapsed = elapsedSeconds - (duration * repetitions);
    
    if (duration > 0.0 && end == 0.0)
        percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    else if (duration > 0.0 && end != 0.0) {
        if (duration > end)
            percentage = 1.0 - (((start + end) - useElapsed) / end);
        else
            percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    } else if (duration == 0.0 && end != 0.0)
        percentage = 1.0 - (((start + end) - useElapsed) / end);
    
    return percentage;
}

static inline void adjustPercentagePastForKeySplines(const Vector<SVGAnimationElement::KeySpline>& keySplines, unsigned valueIndex, float& percentagePast)
{
    if (percentagePast == 0.0f) // values at key times need no spline adjustment
        return;
    const SVGAnimationElement::KeySpline& keySpline = keySplines[valueIndex];
    Path path;
    path.moveTo(FloatPoint());
    path.addBezierCurveTo(keySpline.control1, keySpline.control2, FloatPoint(1.0f, 1.0f));
    // FIXME: This needs to use a y-at-x function on path, to compute the y value then multiply percentagePast by that value
}

void SVGAnimationElement::valueIndexAndPercentagePastForDistance(float distancePercentage, unsigned& valueIndex, float& percentagePast)
{
    // Unspecified: animation elements which do not support CALCMODE_PACED, we just always show the first value
    valueIndex = 0;
    percentagePast = 0;
}

float SVGAnimationElement::calculateTotalDistance()
{
    return 0;
}

static inline void caculateValueIndexForKeyTimes(float timePercentage, const Vector<float>& keyTimes, unsigned& valueIndex, float& lastKeyTime, float& nextKeyTime)
{
    unsigned keyTimesCountMinusOne = keyTimes.size() - 1;
    valueIndex = 0;
    ASSERT(timePercentage >= keyTimes.first());
    while ((valueIndex < keyTimesCountMinusOne) && (timePercentage >= keyTimes[valueIndex + 1]))
        valueIndex++;
    
    lastKeyTime = keyTimes[valueIndex];
    if (valueIndex < keyTimesCountMinusOne)
        nextKeyTime = keyTimes[valueIndex + 1];
    else
        nextKeyTime = lastKeyTime;
}

bool SVGAnimationElement::isValidAnimation() const
{
    EAnimationMode animationMode = detectAnimationMode();
    if (!hasValidTarget() || (animationMode == NO_ANIMATION))
        return false;
    if (animationMode == VALUES_ANIMATION) {
        if (!m_values.size())
            return false;
        if (m_keyTimes.size()) {
            if ((m_values.size() != m_keyTimes.size()) || (m_keyTimes.first() != 0))
                return false;
            if (((m_calcMode == CALCMODE_SPLINE) || (m_calcMode == CALCMODE_LINEAR)) && (m_keyTimes.last() != 1))
                return false;
            float lastKeyTime = 0;
            for (unsigned x = 0; x < m_keyTimes.size(); x++) {
                if (m_keyTimes[x] < lastKeyTime || m_keyTimes[x] > 1)
                    return false;
            }
        }
        if (m_keySplines.size()) {
            if ((m_values.size() - 1) != m_keySplines.size())
                return false;
            for (unsigned x = 0; x < m_keyTimes.size(); x++)
                if (m_keyTimes[x] < 0 || m_keyTimes[x] > 1)
                    return false;
        }
    }
    return true;
}

void SVGAnimationElement::calculateValueIndexAndPercentagePast(float timePercentage, unsigned& valueIndex, float& percentagePast)
{
    ASSERT(timePercentage <= 1.0f);
    ASSERT(isValidAnimation());
    EAnimationMode animationMode = detectAnimationMode();
    
    // to-animations have their own special handling
    if (animationMode == TO_ANIMATION)
        return;
    
    // paced is special, caculates values based on distance instead of time
    if (m_calcMode == CALCMODE_PACED) {
        float totalDistance = calculateTotalDistance();
        float distancePercentage = totalDistance * timePercentage;
        valueIndexAndPercentagePastForDistance(distancePercentage, valueIndex, percentagePast);
        return;
    }
    
    // Figure out what our current index is based on on time
    // all further calculations are based on time percentages, to allow unifying keyTimes handling & normal animation
    float lastKeyTimePercentage = 0;
    float nextKeyTimePercentage = 0;
    if (m_keyTimes.size() && (m_keyTimes.size() == m_values.size()))
        caculateValueIndexForKeyTimes(timePercentage, m_keyTimes, valueIndex, lastKeyTimePercentage, nextKeyTimePercentage);
    else {
        unsigned lastPossibleIndex = (m_values.size() ? m_values.size() - 1: 1);
        unsigned flooredValueIndex = static_cast<unsigned>(timePercentage * lastPossibleIndex);
        valueIndex = flooredValueIndex;
        lastKeyTimePercentage = flooredValueIndex / (float)lastPossibleIndex;
        nextKeyTimePercentage = (flooredValueIndex + 1) / (float)lastPossibleIndex;
    }
    
    // No further caculation is needed if we're exactly on an index.
    if (timePercentage == lastKeyTimePercentage || lastKeyTimePercentage == nextKeyTimePercentage) {
        percentagePast = 0.0f;
        return;
    }
    
    // otherwise we decide what percent after that index
    if ((m_calcMode == CALCMODE_SPLINE) && (m_keySplines.size() == (m_values.size() - 1)))
        adjustPercentagePastForKeySplines(m_keySplines, valueIndex, percentagePast);
    else if (m_calcMode == CALCMODE_DISCRETE)
        percentagePast = 0.0f;
    else { // default (and fallback) mode: linear
        float keyTimeSpan = nextKeyTimePercentage - lastKeyTimePercentage;
        float timeSinceLastKeyTime = timePercentage - lastKeyTimePercentage;
        percentagePast = (timeSinceLastKeyTime / keyTimeSpan);
    }
}

bool SVGAnimationElement::updateAnimationBaseValueFromElement()
{
    m_baseValue = targetAttributeAnimatedValue();
    return true;
}

void SVGAnimationElement::applyAnimatedValueToElement()
{
    setTargetAttributeAnimatedValue(m_animatedValue);
}

void SVGAnimationElement::handleTimerEvent(double elapsedSeconds, double timePercentage)
{
    timePercentage = min(timePercentage, 1.0);
    if (!connectedToTimer()) {
        connectTimer();
        return;
    }
    
    // FIXME: accumulate="sum" will not work w/o code similar to this:
//    if (isAccumulated() && repeations() != 0.0)
//        accumulateForRepetitions(m_repetitions);
    
    EAnimationMode animationMode = detectAnimationMode();
    
    unsigned valueIndex = 0;
    float percentagePast = 0;
    calculateValueIndexAndPercentagePast(narrowPrecisionToFloat(timePercentage), valueIndex, percentagePast);
        
    calculateFromAndToValues(animationMode, valueIndex);
    
    updateAnimatedValue(animationMode, narrowPrecisionToFloat(timePercentage), valueIndex, percentagePast);
    
    if (timePercentage == 1.0) {
        if ((m_repeatCount > 0 && m_repetitions < m_repeatCount - 1) || isIndefinite(m_repeatCount)) {
            m_repetitions++;
            return;
        } else
            disconnectTimer();
    }
}

bool SVGAnimationElement::updateAnimatedValueForElapsedSeconds(double elapsedSeconds)
{
    // FIXME: fill="freeze" will not work without saving off the m_stoppedTime in a stop() method and having code similar to this:
//    if (isStopped()) {
//        if (m_fill == FILL_FREEZE)
//            elapsedSeconds = m_stoppedTime;
//        else
//            return false;
//    }
    
    // Validate animation timing settings:
    // #1 (duration > 0) -> fine
    // #2 (duration <= 0.0 && end > 0) -> fine
    if ((m_simpleDuration <= 0.0 && m_end <= 0.0) || (isIndefinite(m_simpleDuration) && m_end <= 0.0))
        return false; // Ignore dur="0" or dur="-neg"
    
    double percentage = calculateTimePercentage(elapsedSeconds, m_begin, m_end, m_simpleDuration, m_repetitions);
    
    if (percentage <= 1.0 || connectedToTimer())
        handleTimerEvent(elapsedSeconds, percentage);
    
    return true; // value was updated, need to apply
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

