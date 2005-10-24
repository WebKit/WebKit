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
#include <math.h>

#include <ksvg2/ksvg.h>
//#include <kdom/ecma/Ecma.h>

#include "SVGAngleImpl.h"
#include "SVGHelper.h"

using namespace KSVG;

const double deg2rad = 0.017453292519943295769; // pi/180
const double deg2grad = 400.0 / 360.0;
const double rad2grad = deg2grad / deg2rad;

SVGAngleImpl::SVGAngleImpl(const SVGStyledElementImpl *context) : KDOM::Shared<SVGAngleImpl>()
{
    m_unitType = SVG_ANGLETYPE_UNKNOWN;
    m_valueInSpecifiedUnits = 0;
    m_value = 0;
    m_context = context;
}

SVGAngleImpl::~SVGAngleImpl()
{
}

unsigned short SVGAngleImpl::unitType() const
{
    return m_unitType;
}

void SVGAngleImpl::setValue(float value)
{
    m_value = value;
}

float SVGAngleImpl::value() const
{
    return m_value;
}

// calc m_value
void SVGAngleImpl::calculate()
{
    if(m_unitType == SVG_ANGLETYPE_GRAD)
        m_value = m_valueInSpecifiedUnits / deg2grad;
    else if(m_unitType == SVG_ANGLETYPE_RAD)
        m_value = m_valueInSpecifiedUnits / deg2rad;
    else if(m_unitType == SVG_ANGLETYPE_UNSPECIFIED || m_unitType == SVG_ANGLETYPE_DEG)
        m_value = m_valueInSpecifiedUnits;
}

void SVGAngleImpl::setValueInSpecifiedUnits(float valueInSpecifiedUnits)
{
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    calculate();
}

float SVGAngleImpl::valueInSpecifiedUnits() const
{
    return m_valueInSpecifiedUnits;
}

void SVGAngleImpl::setValueAsString(KDOM::DOMStringImpl *valueAsString)
{
    m_valueAsString = KDOM::DOMString(valueAsString);

    QString s = m_valueAsString.string();

    bool bOK;
    m_valueInSpecifiedUnits = s.toDouble(&bOK);
    m_unitType = SVG_ANGLETYPE_UNSPECIFIED;

    if(!bOK)
    {
        if(s.endsWith(QString::fromLatin1("deg")))
            m_unitType = SVG_ANGLETYPE_DEG;
        else if(s.endsWith(QString::fromLatin1("grad")))
            m_unitType = SVG_ANGLETYPE_GRAD;
        else if(s.endsWith(QString::fromLatin1("rad")))
            m_unitType = SVG_ANGLETYPE_RAD;
    }
    
    calculate();
}

KDOM::DOMStringImpl *SVGAngleImpl::valueAsString() const
{
    m_valueAsString.string().setNum(m_valueInSpecifiedUnits);

    switch(m_unitType)
    {
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueAsString.string() += QString::fromLatin1("deg");
            break;
        case SVG_ANGLETYPE_RAD:
            m_valueAsString.string() += QString::fromLatin1("rad");
            break;
        case SVG_ANGLETYPE_GRAD:
            m_valueAsString.string() += QString::fromLatin1("grad");
            break;
    }
    
    return m_valueAsString.handle();
}

void SVGAngleImpl::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    m_unitType = unitType;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    calculate();
}

void SVGAngleImpl::convertToSpecifiedUnits(unsigned short unitType)
{
    if(m_unitType == unitType)
        return;

    if(m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits *= deg2rad;
    else if(m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_RAD)
        m_valueInSpecifiedUnits /= rad2grad;
    else if(m_unitType == SVG_ANGLETYPE_DEG && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits *= deg2grad;
    else if(m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_GRAD)
        m_valueInSpecifiedUnits *= rad2grad;
    else if(m_unitType == SVG_ANGLETYPE_RAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits /= deg2rad;
    else if(m_unitType == SVG_ANGLETYPE_GRAD && unitType == SVG_ANGLETYPE_DEG)
        m_valueInSpecifiedUnits /= deg2grad;

    m_unitType = unitType;
}

// Helpers
double SVGAngleImpl::todeg(double rad)
{
    return rad / deg2rad;
}

double SVGAngleImpl::torad(double deg)
{
    return deg * deg2rad;
}

double SVGAngleImpl::shortestArcBisector(double angle1, double angle2)
{
    double bisector = (angle1 + angle2) / 2;

    if(fabs(angle1 - angle2) > 180)
        bisector += 180;

    return bisector;
}

const SVGStyledElementImpl *SVGAngleImpl::context() const
{
    return m_context;
}

void SVGAngleImpl::setContext(const SVGStyledElementImpl *context)
{
    m_context = context;
}


// vim:ts=4:noet
