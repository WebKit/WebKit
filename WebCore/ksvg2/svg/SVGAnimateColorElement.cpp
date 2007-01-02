/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGAnimateColorElement.h"

#include "Document.h"
#include "TimeScheduler.h"
#include "PlatformString.h"
#include "SVGColor.h"
#include "SVGSVGElement.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGAnimateColorElement::SVGAnimateColorElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
    , m_toColor(new SVGColor())
    , m_fromColor(new SVGColor())
    , m_currentItem(-1)
    , m_redDiff(0)
    , m_greenDiff(0)
    , m_blueDiff(0)
{
}

SVGAnimateColorElement::~SVGAnimateColorElement()
{
}

void calculateColorDifference(const Color& first, const Color& second, int& redDiff, int& greenDiff, int& blueDiff)
{
    redDiff = first.red() - second.red();
    greenDiff = first.green() - second.green();
    blueDiff = first.blue() - second.blue();
}

void SVGAnimateColorElement::storeInitialValue()
{
    m_initialColor = SVGColor::colorFromRGBColorString(targetAttribute());
}

void SVGAnimateColorElement::resetValues()
{
    m_currentItem = -1;
    m_redDiff = 0;
    m_greenDiff = 0;
    m_blueDiff = 0;
}

bool SVGAnimateColorElement::updateCurrentValue(double timePercentage)
{
    int r = 0, g = 0, b = 0;
    if ((m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0) && !m_values)
        calculateColor(timePercentage, r, g, b);
    else if (m_values) {
        int itemByPercentage = calculateCurrentValueItem(timePercentage);
        
        if (itemByPercentage == -1)
            return false;
        
        if (m_currentItem != itemByPercentage) { // Item changed...
            ExceptionCode ec = 0;
            
            // Extract current 'from' / 'to' values
            String value1 = m_values->getItem(itemByPercentage, ec);
            String value2 = m_values->getItem(itemByPercentage + 1, ec);
            
            // Calculate r/g/b shifting values...
            if (!value1.isEmpty() && !value2.isEmpty()) {
                bool apply = false;
                if (m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0) {
                    r = m_toColor->color().red();
                    g = m_toColor->color().green();
                    b = m_toColor->color().blue();
                    
                    apply = true;
                }
                
                m_toColor->setRGBColor(value2);
                m_fromColor->setRGBColor(value1);
                calculateColorDifference(m_toColor->color(), m_fromColor->color(), m_redDiff, m_greenDiff, m_blueDiff);
                
                m_currentItem = itemByPercentage;
                
                if (!apply)
                    return false;
            }
        } else if (m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0) {
            double relativeTime = calculateRelativeTimePercentage(timePercentage, m_currentItem);
            calculateColor(relativeTime, r, g, b);
        }
    }
    
    if (!isFrozen() && timePercentage == 1.0) {
        r = m_initialColor.red();
        g = m_initialColor.green();
        b = m_initialColor.blue();
    }
    
    if (isAccumulated() && repeations() != 0.0) {
        r += m_lastColor.red();
        g += m_lastColor.green();
        b += m_lastColor.blue();
    }
    
    m_currentColor = clampColor(r, g, b);
    return true;
}

bool SVGAnimateColorElement::handleStartCondition()
{
    storeInitialValue();
    
    switch (detectAnimationMode()) {
        case TO_ANIMATION:
        case FROM_TO_ANIMATION:
        {
            m_toColor->setRGBColor(m_to);
            if (!m_from.isEmpty()) // from-to animation
                m_fromColor->setRGBColor(m_from);
            else // to animation
                m_fromColor->setRGBColor(m_initialColor.name());
            
            calculateColorDifference(m_toColor->color(), m_fromColor->color(), m_redDiff, m_greenDiff, m_blueDiff);
            break;
        }
        case BY_ANIMATION:
        case FROM_BY_ANIMATION:
        {
            if (!m_from.isEmpty()) // from-by animation
                m_fromColor->setRGBColor(m_from);
            else // by animation
                m_fromColor->setRGBColor(m_initialColor.name());
            
            m_toColor->setRGBColor(addColorsAndClamp(m_fromColor->color(), SVGColor::colorFromRGBColorString(m_by)).name());
            
            calculateColorDifference(m_toColor->color(), m_fromColor->color(), m_redDiff, m_greenDiff, m_blueDiff);
            break;
        }
        case VALUES_ANIMATION:
            break;
        default:
        {
            //kdError() << k_funcinfo << " Unable to detect animation mode! Aborting creation!" << endl;
            return false;
        }
    }
    
    return true;
}

void SVGAnimateColorElement::updateLastValueWithCurrent()
{
    m_lastColor = m_currentColor;
}

void SVGAnimateColorElement::applyAnimationToValue(Color& currentColor)
{
    if (isAdditive())
        currentColor = addColorsAndClamp(currentColor, color());
    else
        currentColor = color();
}

static inline int clampColorValue(int v)
{
    if (v > 255)
        v = 255;
    else if (v < 0)
        v = 0;
    return v;
}

Color SVGAnimateColorElement::clampColor(int r, int g, int b) const
{
    return Color(clampColorValue(r), clampColorValue(g), clampColorValue(b));
}

Color SVGAnimateColorElement::addColorsAndClamp(const Color& first, const Color& second)
{
    return Color(clampColorValue(first.red() + second.red()),
                 clampColorValue(first.green() + second.green()),
                 clampColorValue(first.blue() + second.blue()));
}

void SVGAnimateColorElement::calculateColor(double time, int &r, int &g, int &b) const
{
    r = lround(m_redDiff * time) + m_fromColor->color().red();
    g = lround(m_greenDiff * time) + m_fromColor->color().green();
    b = lround(m_blueDiff * time) + m_fromColor->color().blue();
}

Color SVGAnimateColorElement::color() const
{
    return m_currentColor;
}

Color SVGAnimateColorElement::initialColor() const
{
    return m_initialColor;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

