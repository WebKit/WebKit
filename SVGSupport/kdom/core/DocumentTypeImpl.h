/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KDOM_DocumentTypeImpl_H
#define KDOM_DocumentTypeImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class NamedNodeMapImpl;
	class DocumentTypeImpl : public NodeImpl
	{
	public:
		DocumentTypeImpl(DocumentImpl *doc, const DOMString &qualifiedName, const DOMString &publicId, const DOMString &systemId);
		virtual ~DocumentTypeImpl();

		virtual DOMString nodeName() const;
		virtual unsigned short nodeType() const;
		DOMString publicId() const;
		DOMString systemId() const;

		virtual DOMString textContent() const; // DOM3

		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

		NamedNodeMapImpl *entities() const;
		NamedNodeMapImpl *notations() const;

		// Other methods (not part of DOM)
		void setName(const DOMString &n) { m_qualifiedName = n; }
		void setPublicId(const DOMString &publicId) { m_publicId = publicId; }
		void setSystemId(const DOMString &systemId) { m_systemId = systemId; }

	protected:
		mutable NamedNodeMapImpl *m_entities;
		mutable NamedNodeMapImpl *m_notations;

		DOMString m_qualifiedName;
		DOMString m_publicId;
		DOMString m_systemId;
	};
};

#endif

// vim:ts=4:noet
