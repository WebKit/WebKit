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
#include "Entity.h"
#include "DOMException.h"

#include "DOMConstants.h"
#include "Entity.lut.h"
#include "EntityImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Entity::s_hashTable 7
 publicId		EntityConstants::PublicId		DontDelete|ReadOnly
 systemId		EntityConstants::SystemId		DontDelete|ReadOnly
 notationName	EntityConstants::NotationName	DontDelete|ReadOnly
 inputEncoding	EntityConstants::InputEncoding	DontDelete|ReadOnly
 xmlEncoding	EntityConstants::XmlEncoding	DontDelete|ReadOnly
 xmlVersion		EntityConstants::XmlVersion		DontDelete|ReadOnly
@end
*/

Value Entity::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case EntityConstants::PublicId:
			return getDOMString(publicId());
		case EntityConstants::SystemId:
			return getDOMString(systemId());
		case EntityConstants::NotationName:
			return getDOMString(notationName());
		case EntityConstants::InputEncoding:
			return getDOMString(inputEncoding());
		case EntityConstants::XmlEncoding:
			return getDOMString(xmlEncoding());
		case EntityConstants::XmlVersion:
			return getDOMString(xmlVersion());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<EntityImpl *>(d))

Entity Entity::null;

Entity::Entity() : Node()
{
}

Entity::Entity(EntityImpl *i) : Node(i)
{
}

Entity::Entity(const Entity &other) : Node()
{
	(*this) = other;
}

Entity::Entity(const Node &other) : Node()
{
	(*this) = other;
}

Entity::~Entity()
{
}

Entity &Entity::operator=(const Entity &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(Entity, ENTITY_NODE)

DOMString Entity::publicId() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);
	
	return impl->publicId();
}

DOMString Entity::systemId() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);
		
	return impl->systemId();
}

DOMString Entity::notationName() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->notationName();
}

DOMString Entity::inputEncoding() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->inputEncoding();
}

DOMString Entity::xmlEncoding() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->xmlEncoding();
}

DOMString Entity::xmlVersion() const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->xmlVersion();
}

// vim:ts=4:noet
