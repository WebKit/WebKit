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
#include "Attr.h"
#include "TypeInfo.h"
#include "DOMException.h"

#include "DOMConstants.h"
#include "Attr.lut.h"
#include "AttrImpl.h"
#include "TypeInfoImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Attr::s_hashTable 7
 name			AttrConstants::Name				DontDelete|ReadOnly
 specified		AttrConstants::Specified		DontDelete|ReadOnly
 value			AttrConstants::Value			DontDelete
 ownerElement	AttrConstants::OwnerElement		DontDelete|ReadOnly
 isId			AttrConstants::IsId				DontDelete|ReadOnly
 schemaTypeInfo	AttrConstants::SchemaTypeInfo	DontDelete|ReadOnly
@end
*/

Value Attr::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case AttrConstants::Name:
			return getDOMString(name());
		case AttrConstants::Specified:
			return KJS::Boolean(specified());
		case AttrConstants::Value:
			return getDOMString(value());
		case AttrConstants::OwnerElement:
			return getDOMNode(exec, ownerElement());
		case AttrConstants::IsId:
			return KJS::Boolean(isId());
		case AttrConstants::SchemaTypeInfo:
			return safe_cache<TypeInfo>(exec, schemaTypeInfo());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

void Attr::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case AttrConstants::Value:
		{
			setValue(toDOMString(exec, value));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

// The qdom way...
#define impl (static_cast<AttrImpl *>(d))

Attr Attr::null;

Attr::Attr() : Node()
{
}

Attr::Attr(AttrImpl *i) : Node(i)
{
}

Attr::Attr(const Attr &other) : Node()
{
	(*this) = other;
}

Attr::Attr(const Node &other) : Node()
{
	(*this)=other;
}

Attr::~Attr()
{
}

Attr &Attr::operator=(const Attr &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(Attr, ATTRIBUTE_NODE)

DOMString Attr::name() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->name();
}

bool Attr::specified() const
{
	if(!d)
		return false;
	
	return impl->specified();
}

void Attr::setValue(const DOMString &value)
{
	if(d)
		impl->setValue(value);
}

DOMString Attr::value() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->value();
}

Element Attr::ownerElement() const
{
	if(!d)
		return Element::null;

	return impl->ownerElement();
}

bool Attr::isId() const
{
	if(!d)
		return false;
	
	return impl->isId();
}

TypeInfo Attr::schemaTypeInfo() const
{
	if(!d)
		return TypeInfo();

	return TypeInfo(impl->schemaTypeInfo());
}

// vim:ts=4:noet
