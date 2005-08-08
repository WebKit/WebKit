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

#include "SVGPathSeg.h"
#include "SVGPathSegImpl.h"

#include "SVGConstants.h"
#include "SVGPathSeg.lut.h"

using namespace KSVG;

/*
@begin SVGPathSeg::s_hashTable 3
 pathSegType		SVGPathSegConstants::PathSegType			DontDelete|ReadOnly
 pathSegTypeAsLetter	SVGPathSegConstants::PathSegTypeAsLetter	DontDelete|ReadOnly
@end
*/

ValueImp *SVGPathSeg::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
	case SVGPathSegConstants::PathSegType:
		return Number(pathSegType());
	case SVGPathSegConstants::PathSegTypeAsLetter:
		return String(pathSegTypeAsLetter().string());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

SVGPathSeg SVGPathSeg::null;

SVGPathSeg::SVGPathSeg() : impl(0)
{
}

SVGPathSeg::SVGPathSeg(const SVGPathSeg &other) : impl(0)
{
	(*this) = other;
}

SVGPathSeg::SVGPathSeg(SVGPathSegImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGPathSeg::~SVGPathSeg()
{
	if(impl)
		impl->deref();
}

SVGPathSeg &SVGPathSeg::operator=(const SVGPathSeg &other)
{
	KDOM_SAFE_SET(impl, other.impl);
	return *this;
}

bool SVGPathSeg::operator==(const SVGPathSeg &other) const
{
	return impl == other.impl;
}

bool SVGPathSeg::operator!=(const SVGPathSeg &other) const
{
	return !operator==(other);
}

unsigned short SVGPathSeg::pathSegType() const
{
	if(!impl)
		return PATHSEG_UNKNOWN;

	return impl->pathSegType();
}

KDOM::DOMString SVGPathSeg::pathSegTypeAsLetter() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->pathSegTypeAsLetter();
}

// vim:ts=4:noet
