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

#include <kdebug.h>

#include "SVGPathSegLinetoImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPathSegLinetoAbsImpl::SVGPathSegLinetoAbsImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = 0.0;
}

SVGPathSegLinetoAbsImpl::~SVGPathSegLinetoAbsImpl()
{
}

void SVGPathSegLinetoAbsImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegLinetoAbsImpl::x() const
{
    return m_x;
}

void SVGPathSegLinetoAbsImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegLinetoAbsImpl::y() const
{
    return m_y;
}

SVGPathSegLinetoRelImpl::SVGPathSegLinetoRelImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
    m_x = m_y = 0.0;
}

SVGPathSegLinetoRelImpl::~SVGPathSegLinetoRelImpl()
{
}

void SVGPathSegLinetoRelImpl::setX(double x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegLinetoRelImpl::x() const
{
    return m_x;
}

void SVGPathSegLinetoRelImpl::setY(double y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGPathSegLinetoRelImpl::y() const
{
    return m_y;
}

// vim:ts=4:noet
