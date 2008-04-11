/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2008 Apple Inc. All rights reserved.

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
#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGAnimateElement.h"

#include "ColorDistance.h"
#include "SVGColor.h"
#include "SVGParserUtilities.h"

using namespace std;

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
    , m_propertyType(StringProperty)
    , m_fromNumber(0)
    , m_toNumber(0)
    , m_animatedNumber(numeric_limits<double>::infinity())
{
}

SVGAnimateElement::~SVGAnimateElement()
{
}

static bool parseNumberValueAndUnit(const String& in, double& value, String& unit)
{
    // FIXME: These are from top of my head, figure out all property types that can be animated as numbers.
    unsigned unitLength = 0;
    String parse = in.stripWhiteSpace();
    if (parse.endsWith("%"))
        unitLength = 1;
    else if (parse.endsWith("px") || parse.endsWith("pt") || parse.endsWith("em"))
        unitLength = 2;
    else if (parse.endsWith("deg") || parse.endsWith("rad"))
        unitLength = 3;
    else if (parse.endsWith("grad"))
        unitLength = 4;
    String newUnit = parse.right(unitLength);
    String number = parse.left(parse.length() - unitLength);
    if (!unit.isEmpty() && newUnit != unit || number.isEmpty())
        return false;
    UChar last = number[number.length() - 1];
    if (last < '0' || last > '9')
        return false;
    unit = newUnit;
    bool ok;
    value = number.toDouble(&ok);
    return ok;
}

void SVGAnimateElement::applyAnimatedValueToElement(unsigned repeat)
{
    // FIXME: This is entirely wrong way to do additive. It won't work with
    // multiple animations on the same element.
    // We need to add together all animation contributions per property and
    // then apply in one go.
    String valueToApply;
    if (m_propertyType == NumberProperty) {
        double number = m_animatedNumber;
        // FIXME: This does not work for values animation.
        if (isAccumulated() && repeat)
            number += m_toNumber * repeat;
        if (isAdditive()) {
            double base;
            if (!parseNumberValueAndUnit(m_savedBaseValue, base, m_numberUnit))
                return;
            number += base;
        }
        valueToApply = String::number(number) + m_numberUnit;
    } else if (m_propertyType == ColorProperty) {
        Color base = SVGColor::colorFromRGBColorString(m_savedBaseValue);
        if (isAdditive())
            valueToApply = ColorDistance::addColorsAndClamp(base, m_animatedColor).name();
        else
            valueToApply = m_animatedColor.name();
    } else
        valueToApply = m_animatedString;
    setTargetAttributeAnimatedValue(valueToApply);
}

bool SVGAnimateElement::updateAnimatedValue(float percentage)
{
    if (m_propertyType == NumberProperty) {
        double oldValue = m_animatedNumber;
        m_animatedNumber = (m_toNumber - m_fromNumber) * percentage + m_fromNumber;
        return oldValue == m_animatedNumber;
    } 
    if (m_propertyType == ColorProperty) {
        m_animatedColor = ColorDistance(m_fromColor, m_toColor).scaledDistance(percentage).addToColorAndClamp(m_fromColor);
        return true;
    }
    
    AnimationMode animationMode = this->animationMode();
    ASSERT(animationMode == FromToAnimation || animationMode == ToAnimation || animationMode == ValuesAnimation);
    String oldValue = m_animatedString;
    if ((animationMode == FromToAnimation && percentage > 0.5f) || animationMode == ToAnimation || percentage == 1.0f)
        m_animatedString = m_toString;
    else
        m_animatedString = m_fromString;
    return oldValue == m_animatedString;
}
 
static bool isColorAttribute(const String& attribute)
{
    return attribute == "color" || attribute == "fill" || attribute == "stroke";
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    // FIXME: Needs more solid way determine target attribute type.
    m_propertyType = isColorAttribute(attributeName()) ? ColorProperty : NumberProperty;
    if (m_propertyType == ColorProperty) {
        m_fromColor = SVGColor::colorFromRGBColorString(fromString);
        m_toColor = SVGColor::colorFromRGBColorString(toString);
        if (m_fromColor.isValid() && m_toColor.isValid())
            return true;
    } else {
        m_numberUnit = String();
        if (parseNumberValueAndUnit(fromString, m_fromNumber, m_numberUnit)) {
            if (parseNumberValueAndUnit(toString, m_toNumber, m_numberUnit))
                return true;
        }
    }
    m_fromString = fromString;
    m_toString = toString;
    m_propertyType = StringProperty;
    return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    m_propertyType = isColorAttribute(attributeName()) ? ColorProperty : NumberProperty;
    if (m_propertyType == ColorProperty) {
        m_fromColor = fromString.isEmpty() ? Color() : SVGColor::colorFromRGBColorString(fromString);
        m_toColor = ColorDistance::addColorsAndClamp(m_fromColor, SVGColor::colorFromRGBColorString(byString));
        if (!m_fromColor.isValid() || !m_toColor.isValid())
            return false;
    } else {
        m_numberUnit = String();
        m_fromNumber = 0;
        if (!fromString.isEmpty() && !parseNumberValueAndUnit(fromString, m_fromNumber, m_numberUnit))
            return false;
        if (!parseNumberValueAndUnit(byString, m_toNumber, m_numberUnit))
            return false;
        m_toNumber += m_fromNumber;
    }
    return true;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

