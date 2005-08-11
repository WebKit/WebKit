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
#include "SVGFitToViewBox.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGAnimatedRect.h"
#include "SVGAnimatedPreserveAspectRatio.h"

#include "SVGConstants.h"
#include "SVGFitToViewBox.lut.h"
using namespace KSVG;

/*
@begin SVGFitToViewBox::s_hashTable 3
 viewBox		SVGFitToViewBoxConstants::ViewBox	DontDelete|ReadOnly
 preserveAspectRatio	SVGFitToViewBoxConstants::PreserveAspectRatio	DontDelete|ReadOnly
@end
*/

ValueImp *SVGFitToViewBox::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFitToViewBoxConstants::ViewBox:
			return KDOM::safe_cache<SVGAnimatedRect>(exec, viewBox());
		case SVGFitToViewBoxConstants::PreserveAspectRatio:
			return KDOM::safe_cache<SVGAnimatedPreserveAspectRatio>(exec, preserveAspectRatio());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGFitToViewBox::SVGFitToViewBox() : impl(0)
{
}

SVGFitToViewBox::SVGFitToViewBox(SVGFitToViewBoxImpl *i) : impl(i)
{
}

SVGFitToViewBox::SVGFitToViewBox(const SVGFitToViewBox &other) : impl(0)
{
	(*this) = other;
}

SVGFitToViewBox::~SVGFitToViewBox()
{
}

SVGFitToViewBox &SVGFitToViewBox::operator=(const SVGFitToViewBox &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGFitToViewBox &SVGFitToViewBox::operator=(SVGFitToViewBoxImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGAnimatedRect SVGFitToViewBox::viewBox() const
{
	if(!impl)
		return SVGAnimatedRect::null;

	return SVGAnimatedRect(impl->viewBox());
}

SVGAnimatedPreserveAspectRatio SVGFitToViewBox::preserveAspectRatio() const
{
	if(!impl)
		return SVGAnimatedPreserveAspectRatio::null;

	return SVGAnimatedPreserveAspectRatio(impl->preserveAspectRatio());
}

// vim:ts=4:noet
