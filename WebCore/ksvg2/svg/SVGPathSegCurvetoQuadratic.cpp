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
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGStyledElement.h"

using namespace WebCore;

SVGPathSegCurvetoQuadraticAbs::SVGPathSegCurvetoQuadraticAbs(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x1 = m_y1 = 0.0;
}

SVGPathSegCurvetoQuadraticAbs::~SVGPathSegCurvetoQuadraticAbs()
{
}

void SVGPathSegCurvetoQuadraticAbs::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoQuadraticAbs::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoQuadraticAbs::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbs::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoQuadraticAbs::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbs::y1() const
{
    return m_y1;
}




SVGPathSegCurvetoQuadraticRel::SVGPathSegCurvetoQuadraticRel(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x1 = m_y1 = 0.0;
}

SVGPathSegCurvetoQuadraticRel::~SVGPathSegCurvetoQuadraticRel()
{
}

void SVGPathSegCurvetoQuadraticRel::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoQuadraticRel::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoQuadraticRel::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRel::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoQuadraticRel::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRel::y1() const
{
    return m_y1;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

