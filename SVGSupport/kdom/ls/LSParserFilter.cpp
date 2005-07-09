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

#include "kdom.h"
#include "Ecma.h"
#include "Element.h"
#include "NodeImpl.h"
#include "DOMString.h"
#include "ElementImpl.h"
#include "LSException.h"
#include "LSParserFilter.h"
#include "LSExceptionImpl.h"
#include "LSParserFilterImpl.h"

#include "LSConstants.h"
#include "LSParserFilter.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin LSParserFilter::s_hashTable 3
 whatToShow	LSParserFilterConstants::WhatToShow	DontDelete|ReadOnly
@end
@begin LSParserFilterProto::s_hashTable 3
 startElement	LSParserFilterConstants::StartElement	DontDelete|Function 1
 acceptNode	LSParserFilterConstants::AcceptNode	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("LSParserFilter", LSParserFilterProto, LSParserFilterProtoFunc)

Value LSParserFilter::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case LSParserFilterConstants::WhatToShow:
			return Number(whatToShow());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(LSException)
	return Undefined();
}

Value LSParserFilterProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(LSParserFilter)
	KDOM_ENTER_SAFE

	switch(id)
	{
		/*case LSParserFilterConstants::StartElement:
		{
			Element elementArg = ecma_cast<Element>(exec, args[0], &toElement);
			return Number(obj.StartElement(elementArg));
		}*/
		case LSParserFilterConstants::AcceptNode:
		{
			Node nodeArg = ecma_cast<Node>(exec, args[0], &toNode);
			return Number(obj.acceptNode(nodeArg));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(LSException)
	return Undefined();
}

LSParserFilter LSParserFilter::null;

LSParserFilter::LSParserFilter() : d(0)
{
}

LSParserFilter::~LSParserFilter()
{
	if(d)
		d->deref();
}

LSParserFilter::LSParserFilter(LSParserFilterImpl *i) : d(i)
{
}

LSParserFilter::LSParserFilter(const LSParserFilter &other) : d(0)
{
	(*this) = other;
}

LSParserFilter &LSParserFilter::operator=(const LSParserFilter &other)
{
	d = other.d;
	return *this;
}

bool LSParserFilter::operator==(const LSParserFilter &other) const
{
	return d == other.d;
}

bool LSParserFilter::operator!=(const LSParserFilter &other) const
{
	return !operator==(other);
}

unsigned short LSParserFilter::startElement(const Element &elementArg)
{
	if(!d)
		return 0;

	return d->acceptNode(static_cast<ElementImpl *>(elementArg.handle()));
}

unsigned short LSParserFilter::acceptNode(const Node &nodeArg)
{
	if(!d)
		return 0;

	return d->acceptNode(static_cast<NodeImpl *>(nodeArg.handle()));
}

unsigned long LSParserFilter::whatToShow() const
{
	if(!d)
		return 0;

	return d->whatToShow();
}

// vim:ts=4:noet
