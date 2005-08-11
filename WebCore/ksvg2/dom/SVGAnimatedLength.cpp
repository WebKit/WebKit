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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGAnimatedLength.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedLength::s_hashTable 3
 baseVal	SVGAnimatedLengthConstants::BaseVal	DontDelete|ReadOnly
 animVal	SVGAnimatedLengthConstants::AnimVal	DontDelete|ReadOnly
@end
*/

ValueImp *SVGAnimatedLength::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedLengthConstants::BaseVal:
			return KDOM::safe_cache<SVGLength>(exec, baseVal());
		case SVGAnimatedLengthConstants::AnimVal:
			return KDOM::safe_cache<SVGLength>(exec, animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGAnimatedLength SVGAnimatedLength::null;

SVGAnimatedLength::SVGAnimatedLength() : impl(0)
{
}

SVGAnimatedLength::SVGAnimatedLength(SVGAnimatedLengthImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedLength::SVGAnimatedLength(const SVGAnimatedLength &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedLength)

SVGLength SVGAnimatedLength::baseVal() const
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->baseVal());
}

SVGLength SVGAnimatedLength::animVal() const
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->animVal());
}

// vim:ts=4:noet
