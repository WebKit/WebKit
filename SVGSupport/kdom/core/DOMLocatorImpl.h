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

#ifndef KDOM_DOMLocatorImpl_H
#define KDOM_DOMLocatorImpl_H

#include <kdom/Shared.h>

namespace KDOM
{
	class NodeImpl;
	class DOMString;
	class DOMStringImpl;
	// Introduced in DOM Level 3:
	class DOMLocatorImpl : public Shared
	{
	public:
		DOMLocatorImpl();
		virtual ~DOMLocatorImpl();

		// 'DOMLocatorImpl' functions
		long lineNumber() const;
		void setLineNumber(long);

		long columnNumber() const;
		void setColumnNumber(long);

		long byteOffset() const;
		void setByteOffset(long);

		long utf16Offset() const;
		void setUtf16Offset(long);

		NodeImpl *relatedNode() const;
		void setRelatedNode(NodeImpl *);
	
		DOMStringImpl *uri() const;
		void setUri(const DOMString &uri);

	protected:
		long m_lineNumber;
		long m_columnNumber;
		long m_byteOffset;
		long m_utf16Offset;
		NodeImpl *m_relatedNode;
		DOMStringImpl *m_uri;
	};
};

#endif

// vim:ts=4:noet
