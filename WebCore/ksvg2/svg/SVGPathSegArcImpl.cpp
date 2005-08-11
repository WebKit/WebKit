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

#include "SVGPathSegArcImpl.h"
#include "SVGStyledElementImpl.h"

using namespace KSVG;

SVGPathSegArcAbsImpl::SVGPathSegArcAbsImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
	m_x = m_y = m_r1 = m_r2 = m_angle = 0.0;
	m_largeArcFlag = m_sweepFlag = false;
}

SVGPathSegArcAbsImpl::~SVGPathSegArcAbsImpl()
{
}

void SVGPathSegArcAbsImpl::setX(double x)
{
	m_x = x;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcAbsImpl::x() const
{
	return m_x;
}

void SVGPathSegArcAbsImpl::setY(double y)
{
	m_y = y;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcAbsImpl::y() const
{
	return m_y;
}

void SVGPathSegArcAbsImpl::setR1(double r1)
{
	m_r1 = r1;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcAbsImpl::r1() const
{
	return m_r1;
}

void SVGPathSegArcAbsImpl::setR2(double r2)
{
	m_r2 = r2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcAbsImpl::r2() const
{
	return m_r2;
}

void SVGPathSegArcAbsImpl::setAngle(double angle)
{
	m_angle = angle;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcAbsImpl::angle() const
{
	return m_angle;
}

void SVGPathSegArcAbsImpl::setLargeArcFlag(bool largeArcFlag)
{
	m_largeArcFlag = largeArcFlag;

	if(m_context)
		m_context->notifyAttributeChange();
}

bool SVGPathSegArcAbsImpl::largeArcFlag() const
{
	return m_largeArcFlag;
}

void SVGPathSegArcAbsImpl::setSweepFlag(bool sweepFlag)
{
	m_sweepFlag = sweepFlag;

	if(m_context)
		m_context->notifyAttributeChange();
}

bool SVGPathSegArcAbsImpl::sweepFlag() const
{
	return m_sweepFlag;
}



SVGPathSegArcRelImpl::SVGPathSegArcRelImpl(const SVGStyledElementImpl *context)
: SVGPathSegImpl(context)
{
	m_x = m_y = m_r1 = m_r2 = m_angle = 0.0;
	m_largeArcFlag = m_sweepFlag = false;
}

SVGPathSegArcRelImpl::~SVGPathSegArcRelImpl()
{
}

void SVGPathSegArcRelImpl::setX(double x)
{
	m_x = x;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcRelImpl::x() const
{
	return m_x;
}

void SVGPathSegArcRelImpl::setY(double y)
{
	m_y = y;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcRelImpl::y() const
{
	return m_y;
}

void SVGPathSegArcRelImpl::setR1(double r1)
{
	m_r1 = r1;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcRelImpl::r1() const
{
	return m_r1;
}

void SVGPathSegArcRelImpl::setR2(double r2)
{
	m_r2 = r2;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcRelImpl::r2() const
{
	return m_r2;
}

void SVGPathSegArcRelImpl::setAngle(double angle)
{
	m_angle = angle;

	if(m_context)
		m_context->notifyAttributeChange();
}

double SVGPathSegArcRelImpl::angle() const
{
	return m_angle;
}

void SVGPathSegArcRelImpl::setLargeArcFlag(bool largeArcFlag)
{
	m_largeArcFlag = largeArcFlag;

	if(m_context)
		m_context->notifyAttributeChange();
}

bool SVGPathSegArcRelImpl::largeArcFlag() const
{
	return m_largeArcFlag;
}

void SVGPathSegArcRelImpl::setSweepFlag(bool sweepFlag)
{
	m_sweepFlag = sweepFlag;

	if(m_context)
		m_context->notifyAttributeChange();
}

bool SVGPathSegArcRelImpl::sweepFlag() const
{
	return m_sweepFlag;
}

// vim:ts=4:noet
