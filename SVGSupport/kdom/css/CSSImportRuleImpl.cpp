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

#include <kdom/cache/KDOMLoader.h>
#include <kdom/cache/KDOMCachedStyleSheet.h>

#include "DocumentImpl.h"
#include "MediaListImpl.h"
#include "CSSStyleSheetImpl.h"
#include "CSSImportRuleImpl.h"

using namespace KDOM;

CSSImportRuleImpl::CSSImportRuleImpl(StyleBaseImpl *parent, DOMStringImpl *href, MediaListImpl *media) : CSSRuleImpl(parent)
{
	m_type = IMPORT_RULE;

	m_lstMedia = media;
	if(!m_lstMedia)
		m_lstMedia = new MediaListImpl(this, 0);

	m_lstMedia->setParent(this);
	m_lstMedia->ref();

	m_strHref = href;
	if(m_strHref)
		m_strHref->ref();

	m_styleSheet = 0;
	m_cachedSheet = 0;

	init();
}

CSSImportRuleImpl::CSSImportRuleImpl(StyleBaseImpl *parent, DOMStringImpl *href, DOMStringImpl *media) : CSSRuleImpl(parent)
{
	m_type = IMPORT_RULE;

	m_lstMedia = new MediaListImpl(this, media);
	m_lstMedia->ref();

	m_strHref = href;
	if(m_strHref)
		m_strHref->ref();

	m_styleSheet = 0;
	m_cachedSheet = 0;

	init();
}

CSSImportRuleImpl::~CSSImportRuleImpl()
{
	if(m_lstMedia)
	{
		m_lstMedia->setParent(0);
		m_lstMedia->deref();
	}

	if(m_styleSheet)
	{
		m_styleSheet->setParent(0);
		m_styleSheet->deref();
	}

	if(m_strHref)
		m_strHref->deref();

	if(m_cachedSheet)
		m_cachedSheet->deref(this);
}

DOMStringImpl *CSSImportRuleImpl::href() const
{
	return m_strHref;
}

MediaListImpl *CSSImportRuleImpl::media() const
{
	return m_lstMedia;
}

CSSStyleSheetImpl *CSSImportRuleImpl::styleSheet() const
{
	return m_styleSheet;
}

void CSSImportRuleImpl::setStyleSheet(DOMStringImpl *url, DOMStringImpl *sheet)
{
	if(m_styleSheet)
	{
  		m_styleSheet->setParent(0);
		m_styleSheet->deref();
	}

	CSSStyleSheetImpl *parent = parentStyleSheet();
	m_styleSheet = parent->doc()->createCSSStyleSheet(this, url);
	m_styleSheet->ref();
	m_styleSheet->parseString(sheet, parent ? parent->useStrictParsing() : true);
	m_loading = false;
	m_done = true;

	checkLoaded();
}

void CSSImportRuleImpl::error(int, const QString &)
{
	if(m_styleSheet)
	{
		m_styleSheet->setParent(0);
		m_styleSheet->deref();
	}

	m_styleSheet = 0;

	m_loading = false;
	m_done = true;

	checkLoaded();
}

bool CSSImportRuleImpl::isLoading()
{
	return (m_loading || (m_styleSheet && m_styleSheet->isLoading()));
}

void CSSImportRuleImpl::init()
{
	m_loading = false;
	m_done = false;

	DocumentLoader *docLoader = 0;

	StyleBaseImpl *root = this;
	StyleBaseImpl *parent;
	while((parent = root->parent()))
		root = parent;

	if(root->isCSSStyleSheet())
		docLoader = static_cast<CSSStyleSheetImpl *>(root)->doc()->docLoader();

	KURL absHref;
	CSSStyleSheetImpl *parentSheet = parentStyleSheet();
	if(parentSheet->href())
	{
		// use parent styleheet's URL as the base URL
		absHref = KURL(KURL(parentSheet->href()->string()), (m_strHref ? m_strHref->string() : QString::null));
	}
	else
	{
		// TODO: khtml(1) disabled this - why?? (Niko)
		// use documents's URL as the base URL
		DocumentImpl *doc = static_cast<CSSStyleSheetImpl *>(root)->doc();
		absHref = KURL(doc->documentKURI(), (m_strHref ? m_strHref->string() : QString::null));
	}

	// Check for a cycle in our import chain.  If we encounter a stylesheet
	// in our parent chain with the same URL, then just bail.
	for(parent = static_cast<StyleBaseImpl *>(this)->parent(); parent; parent = parent->parent())
	{
		if(absHref == parent->baseURL().url())
			return;
	}

	// ### pass correct charset here!!
	m_cachedSheet = docLoader->requestStyleSheet(absHref, QString::null);

	if(m_cachedSheet)
	{
		m_cachedSheet->ref(this);

		// If the imported sheet is in the cache, then setStyleSheet gets called,
		// and the sheet even gets parsed (via parseString).  In this case we have
		// loaded (even if our subresources haven't), so if we have stylesheet after
		// checking the cache, then we've clearly loaded. -dwh
		// This can also happen when error() is called from within ref(). In either case,
		// m_done is set to true.
		if(!m_done)
			m_loading = true;
	}
}

// vim:ts=4:noet
