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
#include "IntRect.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/RenderPath.h>
#include "KCanvasRenderingStyle.h"

#include "ksvg.h"
#include "svgpathparser.h"
#include "SVGLength.h"
#include "SVGElement.h"
#include "SVGSVGElement.h"
#include "SVGAnimatedRect.h"
#include "SVGAnimatedLength.h"

#include <math.h>

using namespace WebCore;
using namespace std;

// keep track of textual description of the unit type
DeprecatedString UnitText[] =
{
    DeprecatedString::fromLatin1(""), DeprecatedString::fromLatin1(""),
    DeprecatedString::fromLatin1("%"), DeprecatedString::fromLatin1("em"),
    DeprecatedString::fromLatin1("ex"), DeprecatedString::fromLatin1("px"),
    DeprecatedString::fromLatin1("cm"), DeprecatedString::fromLatin1("mm"),
    DeprecatedString::fromLatin1("in"), DeprecatedString::fromLatin1("pt"),
    DeprecatedString::fromLatin1("pc")
};

SVGLength::SVGLength(const SVGStyledElement *context, LengthMode mode, const SVGElement *viewport) : Shared<SVGLength>()
{
    m_mode = mode;
    m_context = context;
    m_viewportElement = viewport;

    m_value = 0;
    m_valueInSpecifiedUnits = 0;

    m_bboxRelative = false;
    m_unitType = SVG_LENGTHTYPE_UNKNOWN;
    
    m_requiresLayout = false;
}

SVGLength::~SVGLength()
{
}

unsigned short SVGLength::unitType() const
{
    return m_unitType;
}

void SVGLength::setValue(float value)
{
    m_value = value;
    updateValueInSpecifiedUnits();
}

float SVGLength::value() const
{
    if (m_requiresLayout)
        const_cast<SVGLength*>(this)->updateValue(false);

    if(m_unitType != SVG_LENGTHTYPE_PERCENTAGE)
        return m_value;

    float value = m_valueInSpecifiedUnits / 100.0;
    if(m_bboxRelative)
        return value;

    // Use the manual override "m_viewportElement" when there is no context element off of which to establish the viewport.
    return SVGHelper::PercentageOfViewport(value, m_context ? m_context->viewportElement() : m_viewportElement, static_cast<LengthMode>(m_mode));
}

void SVGLength::setValueInSpecifiedUnits(float valueInSpecifiedUnits)
{
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    updateValue();
}

float SVGLength::valueInSpecifiedUnits() const
{
    return m_valueInSpecifiedUnits;
}                                                

void SVGLength::setValueAsString(StringImpl* s)
{
    String valueAsString(s);
    if(valueAsString.isEmpty())
        return;

    DeprecatedString valueAsQString = valueAsString.deprecatedString();

    double convertedNumber = 0;
    const char *start = valueAsQString.latin1();
    const char *end = parseCoord(start, convertedNumber);
    m_valueInSpecifiedUnits = convertedNumber;

    unsigned int diff = end - start;
    if(diff < valueAsQString.length())
    {
        if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_PX]))
            m_unitType = SVG_LENGTHTYPE_PX;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_CM]))
            m_unitType = SVG_LENGTHTYPE_CM;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_PC]))
            m_unitType = SVG_LENGTHTYPE_PC;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_MM]))
            m_unitType = SVG_LENGTHTYPE_MM;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_IN]))
            m_unitType = SVG_LENGTHTYPE_IN;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_PT]))
            m_unitType = SVG_LENGTHTYPE_PT;
         else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_PERCENTAGE]))
            m_unitType = SVG_LENGTHTYPE_PERCENTAGE;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_EMS]))
            m_unitType = SVG_LENGTHTYPE_EMS;
        else if(valueAsQString.endsWith(UnitText[SVG_LENGTHTYPE_EXS]))
            m_unitType = SVG_LENGTHTYPE_EXS;
        else if(valueAsQString.isEmpty())
            m_unitType = SVG_LENGTHTYPE_NUMBER;
        else
            m_unitType = SVG_LENGTHTYPE_UNKNOWN;
    }
    else
        m_unitType = SVG_LENGTHTYPE_PX;

    updateValue();
}

StringImpl *SVGLength::valueAsString() const
{
    return new StringImpl(DeprecatedString::number(m_valueInSpecifiedUnits) + UnitText[m_unitType]);
}

void SVGLength::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    m_unitType = unitType;
    updateValue();
}

void SVGLength::convertToSpecifiedUnits(unsigned short unitType)
{
    m_unitType = unitType;
    updateValueInSpecifiedUnits();
}

double SVGLength::dpi() const
{
    /* FIXME: DPI detection
    if(context && context->ownerDoc())
    {
        if(mode == LM_WIDTH)
            return 25.4 * context->ownerDoc()->screenPixelsPerMillimeterX();
        else if(mode == LM_HEIGHT)
            return 25.4 * context->ownerDoc()->screenPixelsPerMillimeterY();
        else if(mode == LM_OTHER)
            return 25.4 * context->ownerDoc()->screenPixelsPerMillimeterX();
    }
    */

    return 90.0;
}

void SVGLength::updateValue(bool notify)
{
    switch(m_unitType)
    {
        case SVG_LENGTHTYPE_PX:
            m_value = m_valueInSpecifiedUnits;
            break;
        case SVG_LENGTHTYPE_CM:
            m_value = (m_valueInSpecifiedUnits / 2.54) * dpi();
            break;
        case SVG_LENGTHTYPE_MM:
            m_value = (m_valueInSpecifiedUnits / 25.4) * dpi();
            break;
        case SVG_LENGTHTYPE_IN:
            m_value = m_valueInSpecifiedUnits * dpi();
            break;
        case SVG_LENGTHTYPE_PT:
            m_value = (m_valueInSpecifiedUnits / 72.0) * dpi();
            break;
        case SVG_LENGTHTYPE_PC:
            m_value = (m_valueInSpecifiedUnits / 6.0) * dpi();
            break;
        case SVG_LENGTHTYPE_EMS:
        case SVG_LENGTHTYPE_EXS:
            if (m_context && m_context->renderer()) {
                RenderStyle *style = m_context->renderer()->style();
                float useSize = style->fontSize();
                ASSERT(useSize > 0);
                if (m_unitType == SVG_LENGTHTYPE_EMS)
                    m_value = m_valueInSpecifiedUnits * useSize;
                else {
                    float xHeight = style->font().xHeight();
                    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
                    // if this causes problems in real world cases maybe it would be best to remove this
                    m_value = m_valueInSpecifiedUnits * ceil(xHeight);
                }
                m_requiresLayout = false;
            } else {
                m_requiresLayout = true;
            }
            break;
    }
    if (notify && m_context)
        m_context->notifyAttributeChange();
}

bool SVGLength::updateValueInSpecifiedUnits(bool notify)
{
    if(m_unitType == SVG_LENGTHTYPE_UNKNOWN)
        return false;

    switch(m_unitType)
    {
        case SVG_LENGTHTYPE_PERCENTAGE:
            //kdError() << "updateValueInSpecifiedUnits() SVG_LENGTHTYPE_PERCENTAGE - UNSUPPORTED! Please report!" << endl;
            return false;
        case SVG_LENGTHTYPE_EMS:
            //kdError() << "updateValueInSpecifiedUnits() SVG_LENGTHTYPE_EMS - UNSUPPORTED! Please report!" << endl;
            return false;
        case SVG_LENGTHTYPE_EXS:
            //kdError() << "updateValueInSpecifiedUnits() SVG_LENGTHTYPE_EXS - UNSUPPORTED! Please report!" << endl;
            return false;
        case SVG_LENGTHTYPE_PX:
            m_valueInSpecifiedUnits = m_value;
            break;
        case SVG_LENGTHTYPE_CM:
            m_valueInSpecifiedUnits = m_value / dpi() * 2.54;
            break;
        case SVG_LENGTHTYPE_MM:
            m_valueInSpecifiedUnits = m_value / dpi() * 25.4;
            break;
        case SVG_LENGTHTYPE_IN:
            m_valueInSpecifiedUnits = m_value / dpi();
            break;
        case SVG_LENGTHTYPE_PT:
            m_valueInSpecifiedUnits = m_value / dpi() * 72.0;
            break;
        case SVG_LENGTHTYPE_PC:
            m_valueInSpecifiedUnits = m_value / dpi() * 6.0;
            break;
    };
    
    if (notify && m_context)
        m_context->notifyAttributeChange();

    return true;
}

bool SVGLength::bboxRelative() const
{
    return m_bboxRelative;
}

void SVGLength::setBboxRelative(bool relative)
{
    m_bboxRelative = relative;
}

const SVGStyledElement *SVGLength::context() const
{
    return m_context;
}

void SVGLength::setContext(const SVGStyledElement *context)
{
    m_context = context;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

