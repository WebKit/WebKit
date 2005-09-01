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

#include "SVGPathSegCurvetoCubicImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPathSegCurvetoCubicAbsImpl::SVGPathSegCurvetoCubicAbsImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = m_x1 = m_y1 = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicAbsImpl::~SVGPathSegCurvetoCubicAbsImpl()
{
}

void SVGPathSegCurvetoCubicAbsImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicAbsImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicAbsImpl::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicAbsImpl::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicAbsImpl::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicAbsImpl::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicAbsImpl::y2() const
{
    return m_y2;
}




SVGPathSegCurvetoCubicRelImpl::SVGPathSegCurvetoCubicRelImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = m_x1 = m_y1 = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicRelImpl::~SVGPathSegCurvetoCubicRelImpl()
{
}

void SVGPathSegCurvetoCubicRelImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicRelImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicRelImpl::setX1(double x1)
{
    m_x1 = x1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::x1() const
{
    return m_x1;
}

void SVGPathSegCurvetoCubicRelImpl::setY1(double y1)
{
    m_y1 = y1;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::y1() const
{
    return m_y1;
}

void SVGPathSegCurvetoCubicRelImpl::setX2(double x2)
{
    m_x2 = x2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicRelImpl::setY2(double y2)
{
    m_y2 = y2;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicRelImpl::y2() const
{
    return m_y2;
}

// vim:ts=4:noet
