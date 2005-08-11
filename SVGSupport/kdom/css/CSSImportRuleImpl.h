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

#ifndef KDOM_CSSImportRuleImpl_H
#define KDOM_CSSImportRuleImpl_H

#include <kdom/css/impl/CSSRuleImpl.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

namespace KDOM
{
	class CachedStyleSheet;
	class MediaListImpl;
	class CSSStyleSheetImpl;
	class CSSImportRuleImpl : public CSSRuleImpl,
							  public CachedObjectClient
	{
	public:
		CSSImportRuleImpl(StyleBaseImpl *parent, const DOMString &href, const DOMString &media);
		CSSImportRuleImpl(StyleBaseImpl *parent, const DOMString &href, MediaListImpl *media);
		virtual ~CSSImportRuleImpl();

		// 'CSSImportRule' functions
		DOMString href() const;
		MediaListImpl *media() const;
		CSSStyleSheetImpl *styleSheet() const;

		// 'CSSRule' functions
		virtual bool isImportRule() const { return true; }

		// from CachedObjectClient
		virtual void setStyleSheet(const DOMString &url, const DOMString &sheet);
		virtual void error(int err, const QString &text);

		bool isLoading();
		virtual void init();

	protected:
		DOMString m_strHref;
		MediaListImpl *m_lstMedia;
		CSSStyleSheetImpl *m_styleSheet;
		CachedStyleSheet *m_cachedSheet;

		bool m_done : 1;
		bool m_loading : 1;
	};
};

#endif

// vim:ts=4:noet
