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
#include "DOMObject.h"
#include "Ecma.h"
#include "Node.h"
#include "XPathConstants.h"
#include "XPathException.h"
#include "XPathExpression.h"
#include "XPathExpression.lut.h"
#include "XPathResult.h"

#include "NodeImpl.h"
#include "XPathExceptionImpl.h"
#include "XPathExpressionImpl.h"
#include "XPathResultImpl.h"

using namespace KDOM;
using namespace KJS;

/*
@begin XPathExpression::s_hashTable 2
 dummy		XPathExpressionConstants::Dummy		DontDelete|ReadOnly
@end
@begin XPathExpressionProto::s_hashTable 2
 evaluate	XPathExpressionConstants::Evaluate	DontDelete|Function 3
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("XPathExpression", XPathExpressionProto, XPathExpressionProtoFunc)

ValueImp *XPathExpression::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(XPathException)
	return Undefined();
}

ValueImp *XPathExpressionProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(XPathExpression)
	KDOM_ENTER_SAFE
	KDOM_ENTER_SAFE

	switch(id)
	{
		case XPathExpressionConstants::Evaluate:
		{
			Node contextNode =  ecma_cast<Node>(exec, args[0], &toNode);
			unsigned int type = args[3]->toUInt16(exec);
			/* DOMObject is a possible pooling mechanism which we currently don't use and
			 * hence don't know what type of class it should be. */
			//const DOMObject result = ecma_cast<DOMObject>(exec, args[4], &toDOMObject);

			return safe_cache<XPathResult>(exec, obj.evaluate(contextNode, type, DOMObject()));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	KDOM_LEAVE_CALL_SAFE(XPathException)
	return Undefined();
}

XPathExpression XPathExpression::null;

XPathExpression::XPathExpression() : d(0)
{
}

XPathExpression::XPathExpression(XPathExpressionImpl *i) : d(i)
{
	if(d)
		d->ref();
}

XPathExpression::XPathExpression(const XPathExpression &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(XPathExpression)

XPathResult XPathExpression::evaluate(const Node &contextNode, unsigned short type,
					 				  const DOMObject &result) const
{
	if(!d)
		return XPathResult();

	return XPathResult(d->evaluate(static_cast<NodeImpl *>(contextNode.handle()), type, result.handle()));
}

// vim:ts=4:noet
