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

#include "DOMObject.h"
#include "Node.h"
#include "XPathEvaluator.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"

#include "NodeImpl.h"
#include "XPathException.h"
#include "XPathExceptionImpl.h"
#include "XPathEvaluatorImpl.h"
#include "XPathNSResolverImpl.h"
#include "XPathResultImpl.h"
#include "XPathConstants.h"
#include "XPathEvaluator.lut.h"

using namespace KDOM;
using namespace KJS;

/*
@begin XPathEvaluator::s_hashTable 2
 dummy				XPathEvaluatorConstants::Dummy				DontDelete|ReadOnly
@end
@begin XPathEvaluatorProto::s_hashTable 3
 createExpression	XPathEvaluatorConstants::CreateExpression	DontDelete|Function 2
 createNSResolver	XPathEvaluatorConstants::CreateNSResolver	DontDelete|Function 1
 evaluate			XPathEvaluatorConstants::Evaluate			DontDelete|Function 5
@end
*/


KDOM_IMPLEMENT_PROTOTYPE("XPathEvaluator", XPathEvaluatorProto, XPathEvaluatorProtoFunc)

Value XPathEvaluator::getValueProperty(ExecState *exec, int token) const
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

Value XPathEvaluatorProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	XPathEvaluator obj(cast(exec, static_cast<KJS::ObjectImp *>(thisObj.imp())));
	Q_ASSERT(obj.d != 0);

	KDOM_ENTER_SAFE

	switch(id)
	{
		case XPathEvaluatorConstants::CreateExpression:
		{
			DOMString expr = toDOMString(exec, args[0]);
			XPathNSResolver resolver = ecma_cast<XPathNSResolver>(exec, args[1], &toXPathNSResolver);

			return safe_cache<XPathExpression>(exec, obj.createExpression(expr, resolver));
		}
		case XPathEvaluatorConstants::CreateNSResolver:
		{
			Node resolver = ecma_cast<Node>(exec, args[0], &toNode);
			return safe_cache<XPathNSResolver>(exec, obj.createNSResolver(resolver));
		}
		case XPathEvaluatorConstants::Evaluate:
		{
			DOMString expression = toDOMString(exec, args[0]);
			Node contextNode = ecma_cast<Node>(exec, args[1], &toNode);
			XPathNSResolver reser = ecma_cast<XPathNSResolver>(exec, args[2], &toXPathNSResolver);
			unsigned int type = args[3].toUInt16(exec);
			
			/* DOMObject is a possible pooling mechanism which we currently don't use and
			 * hence don't know what type of class it should be. */
			// const DOMObject result = ecma_cast<DOMObject>(exec, args[4], &toDOMObject);

			return safe_cache<XPathResult>(exec, obj.evaluate(expression, contextNode, reser, type, DOMObject()));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(XPathException)
	return Undefined();
}

XPathEvaluator XPathEvaluator::null;

XPathEvaluator::XPathEvaluator() : d(0)
{
}

XPathEvaluator::XPathEvaluator(XPathEvaluatorImpl *i) : d(i)
{
}

XPathEvaluator::XPathEvaluator(const XPathEvaluator &other): d(0)
{
	(*this) = other;
}

XPathEvaluator::~XPathEvaluator()
{
}


XPathEvaluator &XPathEvaluator::operator=(const XPathEvaluator &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

XPathEvaluator &XPathEvaluator::operator=(XPathEvaluatorImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

XPathNSResolver XPathEvaluator::createNSResolver(const Node &node) const
{
	if(!d)
		return XPathNSResolver::null;

	return XPathNSResolver(d->createNSResolver(static_cast<NodeImpl *>(node.handle())));
}

XPathExpression XPathEvaluator::createExpression(const DOMString &expression, 
								 				 const XPathNSResolver &resolver) const
{
	if(!d)
		return XPathExpression::null;

	return XPathExpression(d->createExpression(expression,
											   static_cast<XPathNSResolverImpl *>(resolver.handle())));
}


XPathResult XPathEvaluator::evaluate(const DOMString &expression, 
					 				 const Node &contextNode, 
					 				 const XPathNSResolver &resolver, 
					 				 const unsigned short type, 
					 				 const DOMObject &result) const
{
	if(!d)
		return XPathResult::null;

	return XPathResult(d->evaluate(expression,
								   static_cast<NodeImpl *>(contextNode.handle()),
								   static_cast<XPathNSResolverImpl *>(resolver.handle()),
								   type, result.handle()));
}

// vim:ts=4:noet
