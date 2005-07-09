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

#ifndef KDOM_XPathResultImpl_H
#define KDOM_XPathResultImpl_H

#include <kdom/Shared.h>

namespace KDOM
{
	class DOMString;
	class NodeImpl;

	class XPathResultImpl : public Shared
	{
	public:
		XPathResultImpl();
		virtual ~XPathResultImpl();

		unsigned short resultType() const;
		
		double numberValue() const;

		DOMString stringValue() const;

		bool booleanValue() const;

		NodeImpl *singleNodeValue() const;

		bool invalidIteratorState() const;

		unsigned long snapshotLength() const;

		NodeImpl *iterateNext() const;

		NodeImpl *snapshotItem(const unsigned long index) const;
	};
};

#endif
// vim:ts=4:noet
