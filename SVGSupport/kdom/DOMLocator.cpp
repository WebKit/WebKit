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

#include "Ecma.h"
#include "DOMLocator.h"
#include "DOMLocatorImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include "DOMConstants.h"
#include "DOMLocator.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMLocator::s_hashTable 7
 lineNumber	DOMLocatorConstants::LineNumber		DontDelete|ReadOnly
 columnNumber	DOMLocatorConstants::ColumnNumber	DontDelete|ReadOnly
 byteOffset	DOMLocatorConstants::ByteOffset		DontDelete|ReadOnly
 utf16Offset	DOMLocatorConstants::Utf16Offset	DontDelete|ReadOnly
 relatedNode	DOMLocatorConstants::RelatedNode	DontDelete|ReadOnly
 uri		DOMLocatorConstants::Uri		DontDelete|ReadOnly
@end
*/

ValueImp *DOMLocator::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DOMLocatorConstants::LineNumber:
			return Number(lineNumber());
		case DOMLocatorConstants::ColumnNumber:
			return Number(columnNumber());
		case DOMLocatorConstants::ByteOffset:
			return Number(byteOffset());
		case DOMLocatorConstants::Utf16Offset:
			return Number(utf16Offset());
		case DOMLocatorConstants::RelatedNode:
			return getDOMNode(exec, relatedNode());
		case DOMLocatorConstants::Uri:
			return getDOMString(uri());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

DOMLocator DOMLocator::null;

DOMLocator::DOMLocator() : d(0)
{
}

DOMLocator::DOMLocator(DOMLocatorImpl *i) : d(i)
{
	if(d)
		d->ref();
}

DOMLocator::DOMLocator(const DOMLocator &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(DOMLocator)

long DOMLocator::lineNumber() const
{
	if(!d)
		return 0;

	return d->lineNumber();
}

long DOMLocator::columnNumber() const
{
	if(!d)
		return 0;

	return d->columnNumber();
}

long DOMLocator::byteOffset() const
{
	if(!d)
		return 0;

	return d->byteOffset();
}

long DOMLocator::utf16Offset() const
{
	if(!d)
		return 0;

	return d->utf16Offset();
}

Node DOMLocator::relatedNode() const
{
	if(!d)
		return Node::null;

	return Node(d->relatedNode());
}

DOMString DOMLocator::uri() const
{
	if(!d)
		return DOMString();

	return DOMString(d->uri());
}

// vim:ts=4:noet
