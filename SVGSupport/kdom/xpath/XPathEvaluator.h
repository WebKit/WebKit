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

#ifndef KDOM_XPathEvaluator_H
#define KDOM_XPathEvaluator_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMObject;
	class DOMString;
	class XPathEvaluatorImpl;
	class XPathExpression;
	class XPathNSResolver;
	class XPathResult;

	/**
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathEvaluator
	{
	public:
		XPathEvaluator();
		explicit XPathEvaluator(XPathEvaluatorImpl *i);
		XPathEvaluator(const XPathEvaluator &other);
		virtual ~XPathEvaluator();

		// Operators
		XPathEvaluator &operator=(const XPathEvaluator &other);
		XPathEvaluator &operator=(XPathEvaluatorImpl *other);

		// 'XPathEvaluator' functions
		XPathExpression createExpression(const DOMString &expression,
										 const XPathNSResolver &resolver) const;

		XPathResult evaluate(const DOMString &expression,
							 const Node &contextNode,
							 const XPathNSResolver &resolver,
							 const unsigned short type,
							 const DOMObject &result) const;

		XPathNSResolver createNSResolver(const Node &node) const;

		// Internal
		KDOM_INTERNAL(XPathEvaluator)

	public:
		XPathEvaluatorImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(XPathEvaluatorProto)
KDOM_IMPLEMENT_PROTOFUNC(XPathEvaluatorProtoFunc, XPathEvaluator)

#endif

// vim:ts=4:noet
