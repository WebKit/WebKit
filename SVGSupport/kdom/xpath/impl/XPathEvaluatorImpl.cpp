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

#include <qmap.h>

#include "DOMString.h"

#include "DocumentImpl.h"
#include "NodeImpl.h"
#include "ScopeImpl.h"
#include "XPathEvaluatorImpl.h"
#include "XPathExpressionImpl.h"
#include "XPathNSResolverImpl.h"
#include "XPathResultImpl.h"

/**
 * Cast away constness and make it a DocumentImpl pointer.
 */
#define THIS (static_cast<DocumentImpl *>(const_cast<XPathEvaluatorImpl *>(this)))

using namespace KDOM;
using namespace KDOM::XPath;

XPathEvaluatorImpl::XPathEvaluatorImpl()
{
}

XPathEvaluatorImpl::~XPathEvaluatorImpl()
{
}

XPathNSResolverImpl* XPathEvaluatorImpl::createNSResolver(NodeImpl *node)
{
	return new XPathNSResolverImpl(node);
}

XPathExpressionImpl *XPathEvaluatorImpl::createExpression(const DOMString &expression,
						 				 				  XPathNSResolverImpl *nsResolver,
														  ScopeImpl *scope)
{
	if(!scope)
		scope = new ScopeImpl(const_cast<XPathEvaluatorImpl *>(this), nsResolver,
							  THIS);

	return new XPathExpressionImpl(expression, scope);
}

XPathResultImpl *XPathEvaluatorImpl::evaluate(const DOMString &expression, NodeImpl *contextNode,
							 				  XPathNSResolverImpl *nsResolver, unsigned short type,
											  void *result)
{
	XPathExpressionImpl *expr = createExpression(expression, nsResolver);
	expr->ref();

	XPathResultImpl *ret = expr->evaluate(contextNode, type, result);
	expr->deref();

	return ret;
}

// vim:ts=4:noet
