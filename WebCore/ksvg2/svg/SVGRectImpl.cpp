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

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGRectImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGRectImpl::SVGRectImpl(const SVGStyledElementImpl *context) : KDOM::Shared(true)
{
	m_context = context;
	m_x = m_y = m_width = m_height = 0.0;
}

SVGRectImpl::~SVGRectImpl()
{
}

double SVGRectImpl::x() const
{
	return m_x;
}

void SVGRectImpl::setX(double value)
{
	m_x = value;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGRectImpl::y() const
{
	return m_y;
}

void SVGRectImpl::setY(double value)
{
	m_y = value;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGRectImpl::width() const
{
	return m_width;
}

void SVGRectImpl::setWidth(double value)
{
	m_width = value;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGRectImpl::height() const
{
	return m_height;
}

void SVGRectImpl::setHeight(double value)
{
	m_height = value;

	if(m_context)
		m_context->notifyAttributeChange();
}

// vim:ts=4:noet
