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

#ifndef KDOM_ProcessingInstructionImpl_H
#define KDOM_ProcessingInstructionImpl_H

#include <kdom/impl/NodeImpl.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

namespace KDOM
{
	class CSSStyleSheetImpl;
	class CachedStyleSheet;
	class StyleSheetImpl;

	class ProcessingInstructionImpl : public NodeBaseImpl,
									  private CachedObjectClient
	{
	public:
		ProcessingInstructionImpl(DocumentImpl *doc, const DOMString &target, const DOMString &data);
		virtual ~ProcessingInstructionImpl();

		// 'ProcessingInstruction' functions
		DOMString target() const;

		DOMString data() const;
		void setData(const DOMString &data);

		virtual DOMString nodeName() const;
		virtual unsigned short nodeType() const;

		virtual DOMString nodeValue() const;
		virtual void setNodeValue(const DOMString &nodeValue);

		virtual DOMString textContent() const; // DOM3

		// Internal
		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

		virtual DOMString localHref() const;
    		StyleSheetImpl *sheet() const;
		void checkStyleSheet();
		virtual void setStyleSheet(const DOMString &url, const DOMString &sheet);
		virtual void setStyleSheet(CSSStyleSheetImpl *sheet);

	protected:
		DOMStringImpl *m_target;
		DOMStringImpl *m_data;
		DOMStringImpl *m_localHref;
		CachedStyleSheet *m_cachedSheet;
		CSSStyleSheetImpl *m_sheet;
	};
};

#endif

// vim:ts=4:noet
