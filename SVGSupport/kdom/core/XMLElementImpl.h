/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#ifndef KDOM_XMLElementImpl_H
#define KDOM_XMLElementImpl_H

#include <kdom/impl/ElementImpl.h>

namespace KDOM
{
	class XMLElementImpl : public ElementImpl
	{
	public:
		XMLElementImpl(DocumentImpl *doc, NodeImpl::Id id);
		XMLElementImpl(DocumentImpl *doc, NodeImpl::Id id, const DOMString &prefix, bool nullNSSpecified = false);
		virtual ~XMLElementImpl();

		virtual DOMString localName() const;
		virtual DOMString tagName() const;

		virtual NodeImpl::Id id() const { return m_id; }

	protected:
		NodeImpl::Id m_id;
	};
};

#endif

// vim:ts=4:noet
