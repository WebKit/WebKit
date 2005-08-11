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

#ifndef KDOM_MediaListImpl_H
#define KDOM_MediaListImpl_H

#include <kdom/css/impl/StyleBaseImpl.h>
#include <kdom/DOMString.h>

#include <qvaluelist.h>

namespace KDOM
{
	class CSSRuleImpl;
	class CSSStyleSheetImpl;

	class MediaListImpl : public StyleBaseImpl
	{
	public:
		MediaListImpl();
		MediaListImpl(CSSStyleSheetImpl *parentSheet);
		MediaListImpl(CSSStyleSheetImpl *parentSheet, const DOMString &media);
		MediaListImpl(CSSRuleImpl *parentRule, const DOMString &media);
		virtual ~MediaListImpl();

		// 'MediaList' functions
		CSSRuleImpl *parentRule() const;
		CSSStyleSheetImpl *parentStyleSheet() const;

		void setMediaText(const DOMString &mediaText);
		DOMString mediaText() const;

		unsigned long length() const;
		DOMString item(unsigned long index) const;

		void deleteMedium(const DOMString &oldMedium);
		void appendMedium(const DOMString &newMedium);

		virtual bool isMediaList() const { return true; }

		/**
		 * Check if the list contains either the requested medium, or
		 * the catch-all "all" media type. Returns true when found,
		 * false otherwise. Since not specifying media types should be
		 * treated as "all" according * to DOM specs, an empty list
		 * always returns true.
		 *
		 * _NOT_ part of the DOM!
		 */
		bool contains(const DOMString &medium) const;

	protected:
		QValueList<DOMString> m_lstMedia;
	};
};

#endif

// vim:ts=4:noet
