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
#include "SVGPathSegCurvetoQuadraticImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPathSegCurvetoQuadraticAbsImpl::SVGPathSegCurvetoQuadraticAbsImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = m_x1 = m_y1 = 0.0;
}

SVGPathSegCurvetoQuadraticAbsImpl::~SVGPathSegCurvetoQuadraticAbsImpl()
{
}

void SVGPathSegCurvetoQuadraticAbsImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbsImpl::x() const
{
    return m_x;
}

void SVGPathSegCurvetoQuadraticAbsImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbsImpl::y() const
{
    return m_y;
}

void SVGPathSegCurvetoQuadraticAbsImpl::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbsImpl::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoQuadraticAbsImpl::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticAbsImpl::y1() const
{
    return m_y1;
}




SVGPathSegCurvetoQuadraticRelImpl::SVGPathSegCurvetoQuadraticRelImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = m_x1 = m_y1 = 0.0;
}

SVGPathSegCurvetoQuadraticRelImpl::~SVGPathSegCurvetoQuadraticRelImpl()
{
}

void SVGPathSegCurvetoQuadraticRelImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRelImpl::x() const
{
    return m_x;
}

void SVGPathSegCurvetoQuadraticRelImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRelImpl::y() const
{
    return m_y;
}

void SVGPathSegCurvetoQuadraticRelImpl::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRelImpl::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoQuadraticRelImpl::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoQuadraticRelImpl::y1() const
{
    return m_y1;
}

// vim:ts=4:noet
