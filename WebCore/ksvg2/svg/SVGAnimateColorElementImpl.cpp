/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include "SVGAnimateColorElementImpl.h"
#include "KSVGTimeScheduler.h"
#include "PlatformString.h"
#include "DocumentImpl.h"
#include "SVGDocumentExtensions.h"

#include <kdebug.h>

using namespace WebCore;

SVGAnimateColorElementImpl::SVGAnimateColorElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
: SVGAnimationElementImpl(tagName, doc)
{
    m_toColor = new SVGColorImpl();
    m_fromColor = new SVGColorImpl();

    m_redDiff = 0;
    m_greenDiff = 0;
    m_blueDiff = 0;

    m_currentItem = -1;
}

SVGAnimateColorElementImpl::~SVGAnimateColorElementImpl()
{
}

void SVGAnimateColorElementImpl::handleTimerEvent(double timePercentage)
{
    // Start condition.
    if(!m_connected)
    {
        // Save initial color... (needed for fill="remove" or additve="sum")
        RefPtr<SVGColorImpl> temp = new SVGColorImpl();
        temp->setRGBColor(targetAttribute().impl());

        m_initialColor = temp->color();

        // Animation mode handling
        switch(detectAnimationMode())
        {
            case TO_ANIMATION:
            case FROM_TO_ANIMATION:
            {
                DOMString toColorString(m_to);
                m_toColor->setRGBColor(toColorString.impl());
    
                DOMString fromColorString;
                if(!m_from.isEmpty()) // from-to animation
                    fromColorString = m_from;
                else // to animation
                    fromColorString = m_initialColor.name();
    
                m_fromColor->setRGBColor(fromColorString.impl());    

                // Calculate color differences, once.
                Color qTo = m_toColor->color();
                Color qFrom = m_fromColor->color();
    
                m_redDiff = qTo.red() - qFrom.red();
                m_greenDiff = qTo.green() - qFrom.green();
                m_blueDiff = qTo.blue() - qFrom.blue();
                
                break;
            }
            case BY_ANIMATION:
            case FROM_BY_ANIMATION:
            {
                DOMString byColorString(m_by);
                m_toColor->setRGBColor(byColorString.impl());

                DOMString fromColorString;
            
                if(!m_from.isEmpty()) // from-by animation
                    fromColorString = m_from;
                else // by animation
                    fromColorString = m_initialColor.name();

                m_fromColor->setRGBColor(fromColorString.impl());

                Color qBy = m_toColor->color();
                Color qFrom = m_fromColor->color();

                // Calculate 'm_toColor' using relative values
                int r = qFrom.red() + qBy.red();
                int g = qFrom.green() + qBy.green();
                int b = qFrom.blue() + qBy.blue();

                Color qTo = clampColor(r, g, b);
            
                DOMString toColorString(qTo.name());
                m_toColor->setRGBColor(toColorString.impl());
            
                m_redDiff = qTo.red() - qFrom.red();
                m_greenDiff = qTo.green() - qFrom.green();
                m_blueDiff = qTo.blue() - qFrom.blue();

                break;
            }
            case VALUES_ANIMATION:
                break;
            default:
            {
                kdError() << k_funcinfo << " Unable to detect animation mode! Aborting creation!" << endl;
                return;
            }
        }

        if (DocumentImpl *doc = getDocument()) {
            doc->accessSVGExtensions()->timeScheduler()->connectIntervalTimer(this);
            m_connected = true;
        }

        return;
    }

    // Calculations...
    if(timePercentage >= 1.0)
        timePercentage = 1.0;

    int r = 0, g = 0, b = 0;
    if((m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0) && !m_values)
        calculateColor(timePercentage, r, g, b);
    else if(m_values)
    {
        int itemByPercentage = calculateCurrentValueItem(timePercentage);

        if(itemByPercentage == -1)
            return;

        if(m_currentItem != itemByPercentage) // Item changed...
        {
            // Extract current 'from' / 'to' values
            DOMString value1 = DOMString(m_values->getItem(itemByPercentage));
            DOMString value2 = DOMString(m_values->getItem(itemByPercentage + 1));

            // Calculate r/g/b shifting values...
            if(!value1.isEmpty() && !value2.isEmpty())
            {
                bool apply = false;
                if(m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0)
                {
                    r = m_toColor->color().red();
                    g = m_toColor->color().green();
                    b = m_toColor->color().blue();

                    apply = true;
                }

                DOMString toColorString(value2);
                m_toColor->setRGBColor(toColorString.impl());
    
                DOMString fromColorString(value1);
                m_fromColor->setRGBColor(fromColorString.impl());    

                Color qTo = m_toColor->color();
                Color qFrom = m_fromColor->color();

                m_redDiff = qTo.red() - qFrom.red();
                m_greenDiff = qTo.green() - qFrom.green();
                m_blueDiff = qTo.blue() - qFrom.blue();

                m_currentItem = itemByPercentage;

                if(!apply)
                    return;
            }
        }
        else if(m_redDiff != 0 || m_greenDiff != 0 || m_blueDiff != 0)
        {
            double relativeTime = calculateRelativeTimePercentage(timePercentage, m_currentItem);
            calculateColor(relativeTime, r, g, b);
        }
    }
    
    if(!isFrozen() && timePercentage == 1.0)
    {
        r = m_initialColor.red();
        g = m_initialColor.green();
        b = m_initialColor.blue();
    }

    if(isAccumulated() && repeations() != 0.0)
    {
        r += m_lastColor.red();
        g += m_lastColor.green();
        b += m_lastColor.blue();
    }

    // Commit changes!
    m_currentColor = clampColor(r, g, b);
    
    // End condition.
    if(timePercentage == 1.0)
    {
        if((m_repeatCount > 0 && m_repeations < m_repeatCount - 1) || isIndefinite(m_repeatCount))
        {
            m_lastColor = m_currentColor;
            m_repeations++;
            return;
        }

        if (DocumentImpl *doc = getDocument()) {
            doc->accessSVGExtensions()->timeScheduler()->disconnectIntervalTimer(this);
            m_connected = false;
        }

        // Reset...
        m_currentItem = -1;

        m_redDiff = 0;
        m_greenDiff = 0;
        m_blueDiff = 0;
    }
}

Color SVGAnimateColorElementImpl::clampColor(int r, int g, int b) const
{
    if(r > 255)
        r = 255;
    else if(r < 0)
        r = 0;

    if(g > 255)
        g = 255;
    else if(g < 0)
        g = 0;

    if(b > 255)
        b = 255;
    else if(b < 0)
        b = 0;

    return Color(r, g, b);
}

void SVGAnimateColorElementImpl::calculateColor(double time, int &r, int &g, int &b) const
{
    r = lround(m_redDiff * time) + m_fromColor->color().red();
    g = lround(m_greenDiff * time) + m_fromColor->color().green();
    b = lround(m_blueDiff * time) + m_fromColor->color().blue();
}

Color SVGAnimateColorElementImpl::color() const
{
    return m_currentColor;
}

Color SVGAnimateColorElementImpl::initialColor() const
{
    return m_initialColor;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

