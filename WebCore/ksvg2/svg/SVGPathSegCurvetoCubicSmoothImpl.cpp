/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "SVGPathSegCurvetoCubicSmoothImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPathSegCurvetoCubicSmoothAbsImpl::SVGPathSegCurvetoCubicSmoothAbsImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
	m_x = m_y = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicSmoothAbsImpl::~SVGPathSegCurvetoCubicSmoothAbsImpl()
{
}

void SVGPathSegCurvetoCubicSmoothAbsImpl::setX(double x)
{
	m_x = x;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbsImpl::x() const
{
	return m_x;
}

void SVGPathSegCurvetoCubicSmoothAbsImpl::setY(double y)
{
	m_y = y;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbsImpl::y() const
{
	return m_y;
}

void SVGPathSegCurvetoCubicSmoothAbsImpl::setX2(double x2)
{
	m_x2 = x2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbsImpl::x2() const
{
	return m_x2;
}

void SVGPathSegCurvetoCubicSmoothAbsImpl::setY2(double y2)
{
	m_y2 = y2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothAbsImpl::y2() const
{
	return m_y2;
}



SVGPathSegCurvetoCubicSmoothRelImpl::SVGPathSegCurvetoCubicSmoothRelImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
	m_x = m_y = m_x2 = m_y2 = 0.0;
}

SVGPathSegCurvetoCubicSmoothRelImpl::~SVGPathSegCurvetoCubicSmoothRelImpl()
{
}

void SVGPathSegCurvetoCubicSmoothRelImpl::setX(double x)
{
	m_x = x;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRelImpl::x() const
{
	return m_x;
}

void SVGPathSegCurvetoCubicSmoothRelImpl::setY(double y)
{
	m_y = y;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRelImpl::y() const
{
	return m_y;
}

void SVGPathSegCurvetoCubicSmoothRelImpl::setX2(double x2)
{
	m_x2 = x2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRelImpl::x2() const
{
	return m_x2;
}

void SVGPathSegCurvetoCubicSmoothRelImpl::setY2(double y2)
{
	m_y2 = y2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegCurvetoCubicSmoothRelImpl::y2() const
{
	return m_y2;
}

// vim:ts=4:noet
