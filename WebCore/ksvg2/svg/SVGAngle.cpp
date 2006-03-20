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

//#include "SVGAngle.h"
#include "config.h"
#if SVG_SUPPORT
#include <math.h>

#include <ksvg2/ksvg.h>
//#include <kdom/ecma/Ecma.h>

#include "SVGAngle.h"
#include "SVGHelper.h"

using namespace WebCore;

const double deg2rad = 0.017453292519943295769; // pi/180
const double deg2grad = 400.0 / 360.0;
const double rad2grad = deg2grad / deg2rad;

SVGAngle::SVGAngle(const SVGStyledElement *context) : Shared<SVGAngle>()
{
    m_unitType = SVG_ANGLETYPE_UNKNOWN;
    m_valueInSpecifiedUnits = 0;
    m_value = 0;
    m_context = context;
}

SVGAngle::~SVGAngle()
{
}

unsigned short SVGAngle::unitType() const
{
    return m_unitType;
}

void SVGAngle::setValue(float value)
{
    m_value = value;
}

float SVGAngle::value() const
{
    return m_value;
}

// calc m_value
void SVGAngle::calculate()
{
    if (m_unitType == SVG_ANGLETYPE_GRAD)
        m_value = m_valueInSpecifiedUnits / deg2grad;
    else if (m_unitType == SVG_ANGLETYPE_RAD)
        m_value = m_valueInSpecifiedUnits / deg2rad;
    else if (m_unitType == SVG_ANGLETYPE_UNSPECIFIED || m_unitType == SVG_ANGLETYPE_DEG)
        m_value = m_valueInSpecifiedUnits;
}

void SVGAngle::setValueInSpecifiedUnits(float valueInSpecifiedUnits)
{
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    calculate();
}

float SVGAngle::valueInSpecifiedUnits() const
{
    return m_valueInSpecifiedUnits;
}

void SVGAngle::setValueAsString(StringImpl *valueAsString)
{
    m_valueAsString = String(valueAsString);

    bool bOK;
    m_valueInSpecifiedUnits = m_valueAsString.deprecatedString().toDouble(&bOK);
    m_unitType = SVG_ANGLETYPE_UNSPECIFIED;

    if (!bOK) {
        if (m_valueAsString.endsWith("deg"))
            m_unitType = SVG_ANGLETYPE_DEG;
        else if (m_valueAsString.endsWith("grad"))
            m_unitType = SVG_ANGLETYPE_GRAD;
        else if (m_valueAsString.endsWith("rad"))
            m_unitType = SVG_ANGLETYPE_RAD;
    }
    
    calculate();
}

StringImpl *SVGAngle::valueAsString() const
{
    m_valueAsString = DeprecatedString::number(m_valueInSpecifiedUnits);

    switch(m_unitType) {
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueAsString += "deg";
            break;
        case SVG_ANGLETYPE_RAD:
            m_valueAsString += "rad";
            break;
        case SVG_ANGLETYPE_GRAD:
            m_valueAsString += "grad";
            break;
    }
    
    return m_valueAsString.impl();
}

void SVGAngle::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    m_unitType = unitType;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    calculate();
}

void SVGAngle::convertToSpecifiedUnits(unsigned short unitType)
{
    if (m_unitType == unitType)
        return;

    if (m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits *= deg2rad;
    else if (m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits /= rad2grad;
    else if (m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits *= deg2grad;
    else if (m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits *= rad2grad;
    else if (m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits /= deg2rad;
    else if (m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits /= deg2grad;

    m_unitType = unitType;
}

// Helpers
double SVGAngle::todeg(double rad)
{
    return rad / deg2rad;
}

double SVGAngle::torad(double deg)
{
    return deg * deg2rad;
}

double SVGAngle::shortestArcBisector(double angle1, double angle2)
{
    double bisector = (angle1 + angle2) / 2;

    if (fabs(angle1 - angle2) > 180)
        bisector += 180;

    return bisector;
}

const SVGStyledElement *SVGAngle::context() const
{
    return m_context;
}

void SVGAngle::setContext(const SVGStyledElement *context)
{
    m_context = context;
}


// vim:ts=4:noet
#endif // SVG_SUPPORT

