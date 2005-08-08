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

#include "Ecma.h"
#include "Counter.h"
#include "NodeImpl.h"
#include "CSSHelper.h"
#include "CounterImpl.h"

#include "kdom/data/CSSConstants.h"
#include "Counter.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Counter::s_hashTable 5
 identifier	CounterConstants::Identifier	DontDelete|ReadOnly
 listStyle	CounterConstants::ListStyle		DontDelete|ReadOnly
 separator	CounterConstants::Separator		DontDelete|ReadOnly
@end
*/

ValueImp *Counter::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CounterConstants::Identifier:
			return getDOMString(identifier());
		case CounterConstants::ListStyle:
			return getDOMString(listStyle());
		case CounterConstants::Separator:
			return getDOMString(separator());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

Counter Counter::null;

Counter::Counter() : d(0)
{
}

Counter::Counter(const Counter &other) : d(0)
{
	(*this) = other;
}

Counter::Counter(CounterImpl *i) : d(i)
{
}

KDOM_IMPL_DTOR_ASSIGN_OP(Counter)

DOMString Counter::identifier() const
{
	if(!d)
		return DOMString();

	return d->identifier();
}

DOMString Counter::listStyle() const
{
	if(!d)
		return DOMString();

	return CSSHelper::stringForListStyleType((EListStyleType) d->listStyle());
}

DOMString Counter::separator() const
{
	if(!d)
		return DOMString();

	return d->separator();
}

// vim:ts=4:noet
