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
#include "Rect.h"
#include "NodeImpl.h"
#include "RectImpl.h"
#include "CSSPrimitiveValue.h"

#include "kdom/data/CSSConstants.h"
#include "Rect.lut.h"
namespace KDOM
{
    using namespace KJS;

/*
@begin Rect::s_hashTable 5
 top	RectConstants::Top		DontDelete|ReadOnly
 right	RectConstants::Right	DontDelete|ReadOnly
 bottom	RectConstants::Bottom	DontDelete|ReadOnly
 left	RectConstants::Left		DontDelete|ReadOnly
@end
*/

KJS::ValueImp *Rect::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case RectConstants::Top:
			return safe_cache<CSSPrimitiveValue>(exec, top());
		case RectConstants::Right:
			return safe_cache<CSSPrimitiveValue>(exec, right());
		case RectConstants::Bottom:
			return safe_cache<CSSPrimitiveValue>(exec, bottom());
		case RectConstants::Left:
			return safe_cache<CSSPrimitiveValue>(exec, left());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

Rect Rect::null;

Rect::Rect() : d(0)
{
}

Rect::Rect(const Rect &other) : d(other.d)
{
	if(d)
		d->ref();
}

Rect::Rect(RectImpl *i) : d(i)
{
	if(d)
		d->ref();
}

KDOM_IMPL_DTOR_ASSIGN_OP(Rect)

CSSPrimitiveValue Rect::top() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->top());
}

CSSPrimitiveValue Rect::right() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->right());
}

CSSPrimitiveValue Rect::bottom() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->bottom());
}

CSSPrimitiveValue Rect::left() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->left());
}

}

// vim:ts=4:noet
