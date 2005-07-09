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

#ifndef KDOM_EntityImpl_H
#define KDOM_EntityImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class DOMStringImpl;

	class EntityImpl : public NodeBaseImpl
	{
	public:
		EntityImpl(DocumentImpl *doc, const DOMString &name);
		EntityImpl(DocumentImpl *doc, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName);
		EntityImpl(DocumentImpl *doc, const DOMString &name, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName);
		virtual ~EntityImpl();

		// 'Entity' functions
		virtual DOMString publicId() const;
		virtual DOMString systemId() const;
		virtual DOMString notationName() const;

		virtual DOMString nodeName() const;
		virtual unsigned short nodeType() const;
		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

		DOMString inputEncoding() const;
		DOMString xmlEncoding() const;
		DOMString xmlVersion() const;

		// Internal
		virtual bool childTypeAllowed(unsigned short type) const;

	protected:
		DOMStringImpl *m_name;
		DOMStringImpl *m_publicId; 
		DOMStringImpl *m_systemId; 
		DOMStringImpl *m_notationName;
		DOMStringImpl *m_inputEncoding;
		DOMStringImpl *m_xmlEncoding;
		DOMStringImpl *m_xmlVersion;
	};
};

#endif

// vim:ts=4:noet
