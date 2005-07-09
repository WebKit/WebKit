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

#ifndef KDOM_XPathResult_H
#define KDOM_XPathResult_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/Shared.h>

namespace KDOM
{
	class DOMString;
	class Node;
	class XPathResultImpl;

	/**
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPathResult
	{
	public:
		enum XPathResultType
		{
			ANY_TYPE                       = 0,
			NUMBER_TYPE                    = 1,
			STRING_TYPE                    = 2,
			BOOLEAN_TYPE                   = 3,
			UNORDERED_NODE_ITERATOR_TYPE   = 4,
			ORDERED_NODE_ITERATOR_TYPE     = 5,
			UNORDERED_NODE_SNAPSHOT_TYPE   = 6,
			ORDERED_NODE_SNAPSHOT_TYPE     = 7,
			ANY_UNORDERED_NODE_TYPE        = 8,
			FIRST_ORDERED_NODE_TYPE        = 9
		};

		XPathResult();
		explicit XPathResult(XPathResultImpl *i);
		XPathResult(const XPathResult &other);
		virtual ~XPathResult();

		// Operators
		XPathResult &operator=(const XPathResult &other);
		bool operator==(const XPathResult &other) const;
		bool operator!=(const XPathResult &other) const;

		// 'XPathResult' functions
		unsigned short resultType() const;
		
		double numberValue() const;

		DOMString stringValue() const;

		bool booleanValue() const;

		Node singleNodeValue() const;

		bool invalidIteratorState() const;

		unsigned long snapshotLength() const;

		Node iterateNext() const;

		Node snapshotItem(const unsigned long index) const;

		// Internal
		KDOM_INTERNAL_BASE(XPathResult)

	protected:
		XPathResultImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};

	KDOM_DEFINE_CAST(XPathResult)
};

KDOM_DEFINE_PROTOTYPE(XPathResultProto)
KDOM_IMPLEMENT_PROTOFUNC(XPathResultProtoFunc, XPathResult)

#endif

// vim:ts=4:noet
