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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGURIReference.h"
#include "SVGAnimatedString.h"
#include "SVGURIReferenceImpl.h"

#include "SVGConstants.h"
#include "SVGURIReference.lut.h"
using namespace KSVG;

/*
@begin SVGURIReference::s_hashTable 3
 href	SVGURIReferenceConstants::Href	DontDelete|ReadOnly
@end
*/

ValueImp *SVGURIReference::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGURIReferenceConstants::Href:
			return KDOM::safe_cache<SVGAnimatedString>(exec, href());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGURIReference::SVGURIReference() : impl(0)
{
}

SVGURIReference::SVGURIReference(SVGURIReferenceImpl *i) : impl(i)
{
}

SVGURIReference::SVGURIReference(const SVGURIReference &other) : impl(0)
{
	(*this) = other;
}

SVGURIReference::~SVGURIReference()
{
}

SVGURIReference &SVGURIReference::operator=(const SVGURIReference &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGURIReference &SVGURIReference::operator=(SVGURIReferenceImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGAnimatedString SVGURIReference::href() const
{
	if(!impl)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->href());
}

// vim:ts=4:noet
