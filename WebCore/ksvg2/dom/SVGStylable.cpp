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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>
#include <kdom/css/CSSValue.h>
#include <kdom/css/CSSStyleDeclaration.h>

#include "SVGElement.h"
#include "SVGStylable.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"
#include "SVGStylableImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGStylable.lut.h"
using namespace KSVG;

/*
@begin SVGStylable::s_hashTable 3
 className			SVGStylableConstants::ClassName		DontDelete|ReadOnly
 style				SVGStylableConstants::Style			DontDelete|ReadOnly
@end
@begin SVGStylableProto::s_hashTable 3
 getPresentationAttribute	SVGStylableConstants::GetPresentationAttribute		DontDelete|Function 1
 getStyle					SVGStylableConstants::GetStyle						DontDelete|Function 0
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGStylable", SVGStylableProto, SVGStylableProtoFunc)

ValueImp *SVGStylable::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGStylableConstants::ClassName:
			return KDOM::safe_cache<SVGAnimatedString>(exec, className());
		case SVGStylableConstants::Style:
			return KDOM::safe_cache<KDOM::CSSStyleDeclaration>(exec, style());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGStylableProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	SVGStylable obj(cast(exec, thisObj));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGStylableConstants::GetPresentationAttribute:
		{
			KDOM::DOMString name = KDOM::toDOMString(exec, args[0]);
			return getDOMCSSValue(exec, obj.getPresentationAttribute(name));
		}
		case SVGStylableConstants::GetStyle:
			return KDOM::safe_cache<KDOM::CSSStyleDeclaration>(exec, obj.style());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGStylable SVGStylable::null;

SVGStylable::SVGStylable() : impl(0)
{
}

SVGStylable::SVGStylable(SVGStylableImpl *i) : impl(i)
{
}

SVGStylable::SVGStylable(const SVGStylable &other) : impl(0)
{
	(*this) = other;
}

SVGStylable::~SVGStylable()
{
}

SVGStylable &SVGStylable::operator=(const SVGStylable &other)
{
	if(impl != other.impl)
		impl = other.impl;

	return *this;
}

SVGStylable &SVGStylable::operator=(SVGStylableImpl *other)
{
	if(impl != other)
		impl = other;

	return *this;
}

SVGAnimatedString SVGStylable::className() const
{
	if(!impl)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->className());
}

KDOM::CSSStyleDeclaration SVGStylable::style() const
{
	if(!impl)
		return KDOM::CSSStyleDeclaration::null;

	return KDOM::CSSStyleDeclaration(impl->style());
}

KDOM::CSSValue SVGStylable::getPresentationAttribute(const KDOM::DOMString &name)
{
	if(!impl)
		return KDOM::CSSValue::null;

	return KDOM::CSSValue(impl->getPresentationAttribute(name));
}

// vim:ts=4:noet
