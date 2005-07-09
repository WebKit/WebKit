/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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
#include "LSInput.h"
#include "DOMString.h"
#include "LSException.h"
#include "DOMException.h"
#include "LSExceptionImpl.h"
#include "DOMExceptionImpl.h"
#include "LSResourceResolver.h"
#include "LSResourceResolverImpl.h"

#include "LSConstants.h"
#include "LSResourceResolver.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSResourceResolver::s_hashTable 2
 dummy				LSResourceResolverConstants::Dummy						DontDelete|ReadOnly
@end
@begin LSResourceResolverProto::s_hashTable 2
 resolveResource	LSResourceResolverConstants::ResolveResource			DontDelete|Function 5
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("LSResourceResolver", LSResourceResolverProto, LSResourceResolverProtoFunc)

Value LSResourceResolver::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
	
	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value LSResourceResolverProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(LSResourceResolver)
	KDOM_ENTER_SAFE
	KDOM_ENTER_SAFE
	
	switch(id)
	{
			case LSResourceResolverConstants::ResolveResource:
		{
			const DOMString type = toDOMString(exec, args[0]);
			const DOMString ns = toDOMString(exec, args[1]);
			const DOMString pub = toDOMString(exec, args[2]);
			const DOMString sys = toDOMString(exec, args[3]);
			const DOMString base = toDOMString(exec, args[4]);

			return safe_cache<LSInput>(exec, obj.resolveResource(type, ns, pub, sys, base));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}
	
	KDOM_LEAVE_CALL_SAFE(DOMException)
	KDOM_LEAVE_CALL_SAFE(LSException)
	return Undefined();
}

LSResourceResolver LSResourceResolver::null;

LSResourceResolver::LSResourceResolver() : d(0)
{
}

LSResourceResolver::LSResourceResolver(LSResourceResolverImpl *i) : d(i)
{
	if(d)
		d->ref();
}

LSResourceResolver::LSResourceResolver(const LSResourceResolver &other) : d(0)
{
	(*this) = other;
}

LSInput LSResourceResolver::resolveResource(const DOMString &type, const DOMString &ns,
											const DOMString &publicId, const DOMString &systemId,
											const DOMString &baseURI)
{
	if(!d)
		return LSInput();

	return LSInput(d->resolveResource(type, ns, publicId, systemId, baseURI));
}

KDOM_IMPL_DTOR_ASSIGN_OP(LSResourceResolver)

// vim:ts=4:noet
