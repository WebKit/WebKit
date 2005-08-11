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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGZoomAndPan.h"
#include "SVGZoomAndPanImpl.h"

#include "SVGConstants.h"
#include "SVGZoomAndPan.lut.h"
using namespace KSVG;

/*
@begin SVGZoomAndPan::s_hashTable 3
 zoomAndPan		SVGZoomAndPanConstants::ZoomAndPan	DontDelete
@end
*/

ValueImp *SVGZoomAndPan::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGZoomAndPanConstants::ZoomAndPan:
			return Number(zoomAndPan());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGZoomAndPan::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGZoomAndPanConstants::ZoomAndPan:
		{
			setZoomAndPan(value->toUInt16(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
}

SVGZoomAndPan::SVGZoomAndPan() : impl(0)
{
}

SVGZoomAndPan::SVGZoomAndPan(SVGZoomAndPanImpl *i) : impl(i)
{
}

SVGZoomAndPan::SVGZoomAndPan(const SVGZoomAndPan &other) : impl(0)
{
	(*this) = other;
}

SVGZoomAndPan::~SVGZoomAndPan()
{
}

SVGZoomAndPan &SVGZoomAndPan::operator=(const SVGZoomAndPan &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGZoomAndPan &SVGZoomAndPan::operator=(SVGZoomAndPanImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

unsigned short SVGZoomAndPan::zoomAndPan() const
{
	if(!impl)
		return 0;

	return impl->zoomAndPan();
}

void SVGZoomAndPan::setZoomAndPan(unsigned short zoomAndPan) const
{
	if(impl)
		impl->setZoomAndPan(zoomAndPan);
}

// vim:ts=4:noet
