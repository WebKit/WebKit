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

#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "DOMLookup.h"
#include "Ecma.h"
#include "TypeInfo.h"
#include "TypeInfoImpl.h"

#include "DOMConstants.h"
#include "TypeInfo.lut.h"

using namespace KDOM;
using namespace KJS;

/*
@begin TypeInfo::s_hashTable 3
 typeName		TypeInfoConstants::TypeName			DontDelete|ReadOnly
 typeNamespace	TypeInfoConstants::TypeNamespace	DontDelete|ReadOnly
@end
@begin TypeInfoProto::s_hashTable 2
 isDerivedFrom	TypeInfoConstants::IsDerivedFrom	DontDelete|Function 3
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("TypeInfo", TypeInfoProto, TypeInfoProtoFunc)

Value TypeInfo::getValueProperty(ExecState *exec, int token) const
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

Value TypeInfoProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(TypeInfo)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case TypeInfoConstants::IsDerivedFrom:
		{
			const DOMString typeNamespaceArg = toDOMString(exec, args[0]);
			const DOMString typeNameArg = toDOMString(exec, args[1]);
			const unsigned long derivationMethod = args[2].toUInt32(exec);

			return KJS::Boolean(obj.isDerivedFrom(typeNamespaceArg,
												  typeNameArg, derivationMethod));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

TypeInfo TypeInfo::null;

TypeInfo::TypeInfo() : d(0)
{
}

TypeInfo::TypeInfo(TypeInfoImpl *i) : d(i)
{
}

TypeInfo::TypeInfo(const TypeInfo &other) : d(0)
{
	(*this) = other;
}

TypeInfo::~TypeInfo()
{
}

TypeInfo &TypeInfo::operator=(const TypeInfo &other)
{
	d = other.d;
	return *this;
}

bool TypeInfo::operator==(const TypeInfo &other) const
{
	return d == other.d;
}

bool TypeInfo::operator!=(const TypeInfo &other) const
{
	return !operator==(other);
}

bool TypeInfo::isDerivedFrom(const DOMString &typeNamespaceArg, const DOMString &typeNameArg,
							 unsigned long derivationMethod) const
{
	if(!d)
		return false;

	return d->isDerivedFrom(typeNamespaceArg, typeNameArg, derivationMethod);
}

DOMString TypeInfo::typeName() const
{
	if(!d)
		return DOMString();

	return d->typeName();
}

DOMString TypeInfo::typeNamespace() const
{
	if(!d)
		return DOMString();

	return d->typeNamespace();
}

// vim:ts=4:noet
