/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the Implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_TagNodeListImpl_H
#define KDOM_TagNodeListImpl_H

#if APPLE_CHANGES
#ifdef check(assertion)
// AssertMacros.h defines check(assertion) which breaks us
#undef check(assertion)
#endif
#endif // APPLE_CHANGES

#include <kdom/DOMString.h>
#include <kdom/impl/NodeListImpl.h>

namespace KDOM
{
	class NodeImpl;
	class DOMStringImpl;
	class TagNodeListImpl : public NodeListImpl
	{
	public:
		TagNodeListImpl(NodeImpl *refNode, DOMStringImpl *name, DOMStringImpl *namespaceURI = 0);
		virtual ~TagNodeListImpl();

		bool check(NodeImpl *node) const;

		// 'NodeListImpl' functions
		virtual NodeImpl *item(unsigned long index) const;
		virtual unsigned long length() const;

	protected:
		NodeImpl *recursiveItem(const NodeImpl *refNode, unsigned long &index) const;
		unsigned long recursiveLength(const NodeImpl *refNode) const;

	protected:
		DOMStringImpl *m_name;
		DOMStringImpl *m_namespaceURI;
	};
};

#endif

// vim:ts=4:noet
