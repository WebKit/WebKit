/*
    Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGAngle.h"

#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGAngle::SVGAngle()
    : RefCounted<SVGAngle>()
    , m_unitType(SVG_ANGLETYPE_UNKNOWN)
    , m_value(0)
    , m_valueInSpecifiedUnits(0)
{
}

SVGAngle::~SVGAngle()
{
}

SVGAngle::SVGAngleType SVGAngle::unitType() const
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
        m_value = grad2deg(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_RAD)
        m_value = rad2deg(m_valueInSpecifiedUnits);
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

void SVGAngle::setValueAsString(const String& s)
{
    m_valueAsString = s;

    bool bOK;
    m_valueInSpecifiedUnits = m_valueAsString.toFloat(&bOK);
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

String SVGAngle::valueAsString() const
{
    m_valueAsString = String::number(m_valueInSpecifiedUnits);

    switch (m_unitType) {
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
        case SVG_ANGLETYPE_UNKNOWN:
            break;
    }
    
    return m_valueAsString;
}

void SVGAngle::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    m_unitType = (SVGAngleType)unitType;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    calculate();
}

void SVGAngle::convertToSpecifiedUnits(unsigned short unitType)
{
    if (m_unitType == unitType)
        return;

    if (m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits = deg2rad(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits = grad2rad(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits = deg2grad(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits = rad2grad(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits = rad2deg(m_valueInSpecifiedUnits);
    else if (m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits = grad2deg(m_valueInSpecifiedUnits);

    m_unitType = (SVGAngleType)unitType;
}

// Helpers
double SVGAngle::todeg(double rad)
{
    return rad2deg(rad);
}

double SVGAngle::torad(double deg)
{
    return deg2rad(deg);
}

double SVGAngle::shortestArcBisector(double angle1, double angle2)
{
    double bisector = (angle1 + angle2) / 2;

    if (fabs(angle1 - angle2) > 180)
        bisector += 180;

    return bisector;
}

}

#endif // ENABLE(SVG)
