/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000 Peter Kelly (pmk@post.com)

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

#ifndef KDOM_NotationImpl_H
#define KDOM_NotationImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class NotationImpl : public NodeBaseImpl
	{
	public:
		NotationImpl(DocumentPtr *doc, DOMStringImpl *publicId, DOMStringImpl *systemId);
		NotationImpl(DocumentPtr *doc, DOMStringImpl *name, DOMStringImpl *publicId, DOMStringImpl *systemId);
		virtual ~NotationImpl();

		// 'Notation' functions
		virtual DOMStringImpl *publicId() const;
		virtual DOMStringImpl *systemId() const;

		virtual DOMStringImpl *nodeName() const;
		virtual unsigned short nodeType() const;
		virtual NodeImpl *cloneNode(bool deep, DocumentPtr *doc) const;

		virtual DOMStringImpl *textContent() const; // DOM3

	protected:
		DOMStringImpl *m_publicId;
		DOMStringImpl *m_systemId;
		DOMStringImpl *m_name;
	};
};

#endif

// vim:ts=4:noet
