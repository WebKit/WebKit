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
#include "Ecma.h"
#include "Node.h"
#include "XPathConstants.h"
#include "XPathException.h"
#include "XPathResult.h"
#include "XPathResult.lut.h"

#include "NodeImpl.h"
#include "XPathExceptionImpl.h"
#include "XPathResultImpl.h"

using namespace KDOM;
using namespace KJS;

/*
@begin XPathResult::s_hashTable 7
 resultType					XPathResultConstants::ResultType			DontDelete|ReadOnly
 numberValue				XPathResultConstants::NumberValue			DontDelete|ReadOnly
 stringValue				XPathResultConstants::StringValue			DontDelete|ReadOnly
 booleanValue				XPathResultConstants::BooleanValue			DontDelete|ReadOnly
 singleNodeValue			XPathResultConstants::SingleNodeValue		DontDelete|ReadOnly
 invalidIteratorState		XPathResultConstants::InvalidIteratorState	DontDelete|ReadOnly
@end
@begin XPathResultProto::s_hashTable 3
 iterateNext				XPathResultConstants::IterateNext			DontDelete|Function 0
 snapshotItem				XPathResultConstants::SnapshotItem			DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("XPathResult", XPathResultProto, XPathResultProtoFunc)

KJS::ValueImp *XPathResult::getValueProperty(KJS::ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	KDOM_ENTER_SAFE

	switch(token)
	{
		case XPathResultConstants::ResultType:
			return KJS::Number(resultType());
		case XPathResultConstants::NumberValue:
			return KJS::Number(numberValue());
		case XPathResultConstants::StringValue:
			return getDOMString(stringValue());
		case XPathResultConstants::BooleanValue:
			return KJS::Boolean(booleanValue());
		case XPathResultConstants::SingleNodeValue:
			return getDOMNode(exec, singleNodeValue());
		case XPathResultConstants::InvalidIteratorState:
			return KJS::Boolean(invalidIteratorState());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	KDOM_LEAVE_SAFE(XPathException)
	return Undefined();
}

ValueImp *XPathResultProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(XPathResult)
	KDOM_ENTER_SAFE
	KDOM_ENTER_SAFE

	switch(id)
	{
		case XPathResultConstants::IterateNext:
			return getDOMNode(exec, obj.iterateNext());
		case XPathResultConstants::SnapshotItem:
		{
			const unsigned long index = (unsigned long) args[0]->toUInt32(exec);
			return getDOMNode(exec, obj.snapshotItem(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	KDOM_LEAVE_CALL_SAFE(XPathException)
	return Undefined();
}

XPathResult XPathResult::null;

XPathResult::XPathResult() : d(0)
{
}

XPathResult::XPathResult(XPathResultImpl *i) : d(i)
{
	if(d)
		d->ref();
}

XPathResult::XPathResult(const XPathResult &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(XPathResult)

unsigned short XPathResult::resultType() const
{
	if(!d)
		return 0;

	return d->resultType();
}

double XPathResult::numberValue() const
{
	if(!d)
		return 0;

	return d->numberValue();
}

DOMString XPathResult::stringValue() const
{
	if(!d)
		return DOMString();

	return d->stringValue();
}

bool XPathResult::booleanValue() const
{
	if(!d)
		return false;

	return d->booleanValue();
}

Node XPathResult::singleNodeValue() const
{
	if(!d)
		return Node::null;

	return Node(d->singleNodeValue());
}

bool XPathResult::invalidIteratorState() const
{
	if(!d)
		return false;

	return d->invalidIteratorState();
}

unsigned long XPathResult::snapshotLength() const
{
	if(!d)
		return 0;

	return d->snapshotLength();
}

Node XPathResult::iterateNext() const
{
	if(!d)
		return Node::null;

	return Node(d->iterateNext());
}

Node XPathResult::snapshotItem(const unsigned long index) const
{
	if(!d)
		return Node::null;

	return Node(d->snapshotItem(index));
}

// vim:ts=4:noet
