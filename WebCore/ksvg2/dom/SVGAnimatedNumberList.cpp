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

#include <kdom/ecma/Ecma.h>

#include "SVGElement.h"
#include "SVGAnimatedNumberList.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGNumberList.h"

#include "SVGConstants.h"
#include "SVGAnimatedNumberList.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedNumberList::s_hashTable 3
 baseVal	SVGAnimatedNumberListConstants::BaseVal	DontDelete|ReadOnly
 animVal	SVGAnimatedNumberListConstants::AnimVal	DontDelete|ReadOnly
@end
*/

Value SVGAnimatedNumberList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedNumberListConstants::BaseVal:
			return KDOM::safe_cache<SVGNumberList>(exec, baseVal());
		case SVGAnimatedNumberListConstants::AnimVal:
			return KDOM::safe_cache<SVGNumberList>(exec, animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

SVGAnimatedNumberList SVGAnimatedNumberList::null;

SVGAnimatedNumberList::SVGAnimatedNumberList() : impl(0)
{
}

SVGAnimatedNumberList::SVGAnimatedNumberList(SVGAnimatedNumberListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedNumberList::SVGAnimatedNumberList(const SVGAnimatedNumberList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedNumberList)

SVGNumberList SVGAnimatedNumberList::baseVal() const
{
	if(!impl)
		return SVGNumberList::null;

	return SVGNumberList(impl->baseVal());
}

SVGNumberList SVGAnimatedNumberList::animVal() const
{
	if(!impl)
		return SVGNumberList::null;

	return SVGNumberList(impl->animVal());
}

// vim:ts=4:noet
