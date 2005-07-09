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

#ifndef KDOM_XPathEvaluatorImpl_H
#define KDOM_XPathEvaluatorImpl_H

namespace KDOM
{
	class DOMString;
	class NodeImpl;
	class XPathNSResolverImpl;
	class XPathExpressionImpl;
	class XPathResultImpl;

	namespace XPath
	{
		class ScopeImpl;
	};

	class XPathEvaluatorImpl
	{
	public:
		XPathEvaluatorImpl();
		virtual ~XPathEvaluatorImpl();

		virtual XPathNSResolverImpl* createNSResolver(NodeImpl *node);

		virtual XPathExpressionImpl *createExpression(const DOMString &expression,
								 				 	  XPathNSResolverImpl *nsResolver,
													  XPath::ScopeImpl *scope = 0);

		virtual XPathResultImpl *evaluate(const DOMString &expression, NodeImpl *contextNode,
							 			  XPathNSResolverImpl *nsResolver, unsigned short type,
							 			  void *result);
	};
};

#endif
// vim:ts=4:noet
