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

#ifndef KDOM_StyleSheetImpl_H
#define KDOM_StyleSheetImpl_H

#include <kdom/css/impl/StyleBaseImpl.h>

namespace KDOM
{
	class NodeImpl;
	class MediaListImpl;
	class StyleSheetImpl : public StyleListImpl
	{
	public:
		StyleSheetImpl(NodeImpl *ownerNode, const DOMString &href = DOMString());
		StyleSheetImpl(StyleSheetImpl *parentSheet, const DOMString &href = DOMString());
		StyleSheetImpl(StyleBaseImpl *owner, DOMString href  = DOMString());
		virtual ~StyleSheetImpl();

		// 'StyleSheet' functions
		void setDisabled(bool disabled);
		bool disabled() const;

		NodeImpl *ownerNode() const;
		StyleSheetImpl *parentStyleSheet() const;

		DOMString href() const;
		DOMString title() const;

		MediaListImpl *media() const;

		void setMedia(MediaListImpl *media);
		void setTitle(const DOMString &title);

		virtual bool isStyleSheet() const { return true; }
		virtual DOMString type() const { return DOMString(); }

	protected:
		NodeImpl *m_parentNode;
		DOMString m_strHref;
		DOMString m_strTitle;
		MediaListImpl *m_media;
		bool m_disabled : 1;
	};
};

#endif

// vim:ts=4:noet
