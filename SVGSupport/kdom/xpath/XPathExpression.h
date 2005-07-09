/*
 *
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

#ifndef KDOM_XPathExpression_H
#define KDOM_XPathExpression_H

#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>
#include <kdom/xpath/XPathResult.h>

namespace KDOM
{
	class DOMObject;
	class Node;
	class XPathExpressionImpl;

	/**
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathExpression
	{
	public:
		XPathExpression();
		explicit XPathExpression(XPathExpressionImpl *i);
		XPathExpression(const XPathExpression &other);
		virtual ~XPathExpression();

		// Operators
		XPathExpression &operator=(const XPathExpression &other);
		bool operator==(const XPathExpression &other) const;
		bool operator!=(const XPathExpression &other) const;
		
		// 'XPathExpression' functions
		XPathResult evaluate(const Node &contextNode, unsigned short type,
							 const DOMObject &result) const;
			
		// Internal
		KDOM_INTERNAL_BASE(XPathExpression)

	protected:
		XPathExpressionImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};

};

KDOM_DEFINE_PROTOTYPE(XPathExpressionProto)
KDOM_IMPLEMENT_PROTOFUNC(XPathExpressionProtoFunc, XPathExpression)

#endif

// vim:ts=4:noet
