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
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGNumber.h"
#include "SVGNumberImpl.h"

#include "SVGConstants.h"
#include "SVGAnimatedNumber.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedNumber::s_hashTable 3
 baseVal	SVGAnimatedNumberConstants::BaseVal	DontDelete
 animVal	SVGAnimatedNumberConstants::AnimVal	DontDelete|ReadOnly
@end
*/

ValueImp *SVGAnimatedNumber::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedNumberConstants::BaseVal:
			return Number(baseVal());
		case SVGAnimatedNumberConstants::AnimVal:
			return Number(animVal());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGAnimatedNumber::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAnimatedNumberConstants::BaseVal:
		{
			if(impl)
				impl->setBaseVal(KDOM::ecma_cast<SVGNumber>(exec, value, &toSVGNumber).handle());

			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGAnimatedNumber SVGAnimatedNumber::null;

SVGAnimatedNumber::SVGAnimatedNumber() : impl(0)
{
}

SVGAnimatedNumber::SVGAnimatedNumber(SVGAnimatedNumberImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAnimatedNumber::SVGAnimatedNumber(const SVGAnimatedNumber &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedNumber)

float SVGAnimatedNumber::baseVal() const
{
	if(!impl)
		return 0.;

	return impl->baseVal()->value();
}

float SVGAnimatedNumber::animVal() const
{
	if(!impl)
		return 0.;

	return impl->animVal()->value();
}

// vim:ts=4:noet
