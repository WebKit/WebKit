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
#include "SVGPointImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPointImpl::SVGPointImpl(const SVGStyledElementImpl *context) : KDOM::Shared<SVGPointImpl>()
{
    m_x = 0.0;
    m_y = 0.0;
    m_context = context;
}

SVGPointImpl::SVGPointImpl(float x, float y, const SVGStyledElementImpl *context) : KDOM::Shared<SVGPointImpl>()
{
    m_x = x;
    m_y = y;
    m_context = context;
}

SVGPointImpl::SVGPointImpl(const QPoint &p, const SVGStyledElementImpl *context) : KDOM::Shared<SVGPointImpl>()
{
    m_x = p.x();
    m_y = p.y();
    m_context = context;
}

SVGPointImpl::~SVGPointImpl()
{
}

float SVGPointImpl::x() const
{
    return m_x;
}

float SVGPointImpl::y() const
{
    return m_y;
}

void SVGPointImpl::setX(float x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

void SVGPointImpl::setY(float y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

SVGPointImpl *SVGPointImpl::matrixTransform(SVGMatrixImpl * /* matrix */)
{
    // TODO: implement me!
    return 0;
}

// vim:ts=4:noet
