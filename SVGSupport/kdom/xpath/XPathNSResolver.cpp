/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich 	<frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "Ecma.h"
#include "DOMString.h"
#include "Node.h"

#include "XPathException.h"
#include "XPathNSResolver.h"
#include "XPathConstants.h"
#include "XPathException.h"
#include "XPathExceptionImpl.h"
#include "XPathNSResolver.lut.h"
#include "XPathNSResolverImpl.h"

using namespace KDOM;
using namespace KJS;

/*
@begin XPathNSResolver::s_hashTable 2
 dummy				XPathNSResolverConstants::Dummy					DontDelete|ReadOnly
@end
@begin XPathNSResolverProto::s_hashTable 2
 lookupNamespaceURI	XPathNSResolverConstants::LookupNamespaceURI	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("XPathNSResolver", XPathNSResolverProto, XPathNSResolverProtoFunc)

KJS::ValueImp *XPathNSResolver::getValueProperty(KJS::ExecState *, int token) const
{
	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}
	
	return Undefined();
}

ValueImp *XPathNSResolverProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(XPathNSResolver)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case XPathNSResolverConstants::LookupNamespaceURI:
			return getDOMString(obj.lookupNamespaceURI(toDOMString(exec, args[0])));
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(XPathException)
	return Undefined();
}

XPathNSResolver XPathNSResolver::null;

XPathNSResolver::XPathNSResolver(): d(0)
{
}

XPathNSResolver::XPathNSResolver(XPathNSResolverImpl *impl): d(impl)
{
	if(d)
		d->ref();
}

XPathNSResolver::XPathNSResolver(const XPathNSResolver &other): d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(XPathNSResolver)

DOMString XPathNSResolver::lookupNamespaceURI(const DOMString &prefix)
{
	if(!d)
		return DOMString();;
		
	return d->lookupNamespaceURI(prefix);
}

// vim:ts=4:noet
