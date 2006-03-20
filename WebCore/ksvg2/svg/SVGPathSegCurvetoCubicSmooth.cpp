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
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGStyledElement.h"

using namespace WebCore;

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicSmoothAbs::~SVGPathSegCurvetoCubicSmoothAbs()
{
}

void SVGPathSegCurvetoCubicSmoothAbs::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothAbs::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbs::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbs::y2() const
{
    return m_y2;
}



SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(const SVGStyledElement *context)
: SVGPathSeg(context)
{
    m_x = m_y = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicSmoothRel::~SVGPathSegCurvetoCubicSmoothRel()
{
}

void SVGPathSegCurvetoCubicSmoothRel::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothRel::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothRel::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRel::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothRel::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRel::y2() const
{
    return m_y2;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

