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
#include "SVGPathSegCurvetoCubic.h"
#include "SVGStyledElement.h"

using namespace WebCore;

SVGPathSegCurvetoCubicAbs::SVGPathSegCurvetoCubicAbs(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x1 = m_y1 = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicAbs::~SVGPathSegCurvetoCubicAbs()
{
}

void SVGPathSegCurvetoCubicAbs::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicAbs::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicAbs::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicAbs::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicAbs::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicAbs::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbs::y2() const
{
    return m_y2;
}




SVGPathSegCurvetoCubicRel::SVGPathSegCurvetoCubicRel(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x1 = m_y1 = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicRel::~SVGPathSegCurvetoCubicRel()
{
}

void SVGPathSegCurvetoCubicRel::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicRel::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicRel::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicRel::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicRel::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicRel::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRel::y2() const
{
    return m_y2;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

