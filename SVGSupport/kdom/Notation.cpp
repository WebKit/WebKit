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
#include "Notation.h"
#include "DOMException.h"

#include "DOMConstants.h"
#include "Notation.lut.h"
#include "NotationImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Notation::s_hashTable 3
 publicId	NotationConstants::PublicId		DontDelete|ReadOnly
 systemId	NotationConstants::SystemId		DontDelete|ReadOnly
@end
*/

Value Notation::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NotationConstants::PublicId:
			return getDOMString(publicId());
		case NotationConstants::SystemId:
			return getDOMString(systemId());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<NotationImpl *>(d))

Notation Notation::null;

Notation::Notation() : Node()
{
}

Notation::Notation(NotationImpl *i) : Node(i)
{
}

Notation::Notation(const Notation &other) : Node()
{
	(*this) = other;
}

Notation::Notation(const Node &other) : Node()
{
	(*this) = other;
}

Notation::~Notation()
{
}

Notation &Notation::operator=(const Notation &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(Notation, NOTATION_NODE)

DOMString Notation::publicId() const
{
	if(!d)
		return DOMString();

	return impl->publicId();
}

DOMString Notation::systemId() const
{
	if(!d)
		return DOMString();

	return impl->systemId();
}

// vim:ts=4:noet
