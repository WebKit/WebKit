/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "dom/css_rule.h"
#include "dom/css_stylesheet.h"
#include "dom/dom_exception.h"
#include "dom/dom_string.h"

#include "css/css_stylesheetimpl.h"
#include "css/css_valueimpl.h"
#include "css/cssparser.h"
#include "css/css_ruleimpl.h"

#include "misc/loader.h"
#include "misc/htmltags.h"
#include "misc/htmlattrs.h"
#include "xml/dom_docimpl.h"

using namespace DOM;

#include <kdebug.h>

CSSStyleSheetImpl *CSSRuleImpl::parentStyleSheet() const
{
    return ( m_parent && m_parent->isCSSStyleSheet() )  ?
	static_cast<CSSStyleSheetImpl *>(m_parent) : 0;
}

CSSRuleImpl *CSSRuleImpl::parentRule() const
{
    return ( m_parent && m_parent->isRule() )  ?
	static_cast<CSSRuleImpl *>(m_parent) : 0;
}

DOM::DOMString CSSRuleImpl::cssText() const
{
    // ###
    return DOMString();
}

void CSSRuleImpl::setCssText(DOM::DOMString /*str*/)
{
    // ###
}

// ---------------------------------------------------------------------------

CSSFontFaceRuleImpl::CSSFontFaceRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::FONT_FACE_RULE;
    m_style = 0;
}

CSSFontFaceRuleImpl::~CSSFontFaceRuleImpl()
{
    if(m_style) m_style->deref();
}

// --------------------------------------------------------------------------

CSSImportRuleImpl::CSSImportRuleImpl( StyleBaseImpl *parent,
                                      const DOM::DOMString &href,
                                      MediaListImpl *media )
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::IMPORT_RULE;

    m_lstMedia = media;
    if ( !m_lstMedia )
	m_lstMedia = new MediaListImpl( this, DOMString() );
    m_lstMedia->setParent( this );
    m_lstMedia->ref();

    m_strHref = href;
    m_styleSheet = 0;

    m_cachedSheet = 0;

    init();
}
CSSImportRuleImpl::CSSImportRuleImpl( StyleBaseImpl *parent,
                                      const DOM::DOMString &href,
                                      const DOM::DOMString &media )
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::IMPORT_RULE;

    m_lstMedia = new MediaListImpl( this, media );
    m_lstMedia->ref();

    m_strHref = href;
    m_styleSheet = 0;

    m_cachedSheet = 0;

    init();
}

CSSImportRuleImpl::~CSSImportRuleImpl()
{
    if( m_lstMedia ) {
 	m_lstMedia->setParent( 0 );
	m_lstMedia->deref();
    }
    if(m_styleSheet) {
        m_styleSheet->setParent(0);
        m_styleSheet->deref();
    }

    if(m_cachedSheet) m_cachedSheet->deref(this);
}

void CSSImportRuleImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet)
{
    if ( m_styleSheet ) {
        m_styleSheet->setParent(0);
        m_styleSheet->deref();
    }
    m_styleSheet = new CSSStyleSheetImpl(this, url);
    m_styleSheet->ref();

    CSSStyleSheetImpl *parent = parentStyleSheet();
    m_styleSheet->parseString( sheet, parent ? parent->useStrictParsing() : true );
    m_loading = false;

    checkLoaded();
}

bool CSSImportRuleImpl::isLoading()
{
    return ( m_loading || (m_styleSheet && m_styleSheet->isLoading()) );
}

void CSSImportRuleImpl::init()
{
    khtml::DocLoader *docLoader = 0;
    StyleBaseImpl *root = this;
    StyleBaseImpl *parent;
    while ( ( parent = root->parent()) )
	root = parent;
    if (root->isCSSStyleSheet())
	docLoader = static_cast<CSSStyleSheetImpl*>(root)->docLoader();

    DOMString absHref = m_strHref;
    CSSStyleSheetImpl *parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull()) {
      // use parent styleheet's URL as the base URL
      absHref = KURL(parentSheet->href().string(),m_strHref.string()).url();
    }
/*
    else {
      // use documents's URL as the base URL
      DocumentImpl *doc = static_cast<CSSStyleSheetImpl*>(root)->doc();
      absHref = KURL(doc->URL(),m_strHref.string()).url();
    }
*/

    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBaseImpl*>(this)->parent();
         parent;
         parent = parent->parent())
        if (absHref == parent->baseURL())
            return;
    
    // ### pass correct charset here!!
    m_cachedSheet = docLoader->requestStyleSheet(absHref, QString::null);

    if (m_cachedSheet)
    {
      m_cachedSheet->ref(this);

      // If the imported sheet is in the cache, then setStyleSheet gets called,
      // and the sheet even gets parsed (via parseString).  In this case we have
      // loaded (even if our subresources haven't), so if we have stylesheet after
      // checking the cache, then we've clearly loaded. -dwh
      if (!m_styleSheet)
          m_loading = true;
    }
}

// --------------------------------------------------------------------------
CSSMediaRuleImpl::CSSMediaRuleImpl( StyleBaseImpl *parent, MediaListImpl *mediaList, CSSRuleListImpl *ruleList )
    :   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = mediaList;
    m_lstMedia->ref();
    m_lstCSSRules = ruleList;
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent)
    :   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = 0;
    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::CSSMediaRuleImpl( StyleBaseImpl *parent, const DOM::DOMString &media )
:   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = new MediaListImpl( this, media );
    m_lstMedia->ref();
    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::~CSSMediaRuleImpl()
{
    if( m_lstMedia ) {
	m_lstMedia->setParent( 0 );
        m_lstMedia->deref();
    }

    int length = m_lstCSSRules->length();
    for (int i = 0; i < length; i++) {
	m_lstCSSRules->item( i )->setParent( 0 );
    }
    m_lstCSSRules->deref();
}

unsigned long CSSMediaRuleImpl::append( CSSRuleImpl *rule )
{
    if (!rule) {
	return 0;
    }

    rule->setParent(this);
    return m_lstCSSRules->insertRule( rule, m_lstCSSRules->length() );
}

unsigned long CSSMediaRuleImpl::insertRule( const DOMString &rule,
                                            unsigned long index )
{
    CSSParser p( strictParsing );
    CSSRuleImpl *newRule = p.parseRule( parentStyleSheet(), rule );

    if (!newRule) {
	return 0;
    }

    newRule->setParent(this);
    return m_lstCSSRules->insertRule( newRule, index );
}

// ---------------------------------------------------------------------------

CSSRuleListImpl::CSSRuleListImpl(StyleListImpl *lst)
{
    if (lst) {
        unsigned long len = lst->length();
        for (unsigned long i = 0; i < len; ++i) {
            StyleBaseImpl *style = lst->item(i);
            if (style->isRule())
                append(static_cast<CSSRuleImpl *>(style));
        }
    }
}

CSSRuleListImpl::~CSSRuleListImpl()
{
    CSSRuleImpl* rule;
    while ( !m_lstCSSRules.isEmpty() && ( rule = m_lstCSSRules.take( 0 ) ) )
        rule->deref();
}

// ---------------------------------------------------------------------------

CSSPageRuleImpl::CSSPageRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::PAGE_RULE;
    m_style = 0;
}

CSSPageRuleImpl::~CSSPageRuleImpl()
{
    if(m_style) m_style->deref();
}

DOM::DOMString CSSPageRuleImpl::selectorText() const
{
    // ###
    return DOMString();
}

void CSSPageRuleImpl::setSelectorText(DOM::DOMString /*str*/)
{
    // ###
}

// --------------------------------------------------------------------------

CSSStyleRuleImpl::CSSStyleRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::STYLE_RULE;
    m_style = 0;
    m_selector = 0;
}

CSSStyleRuleImpl::~CSSStyleRuleImpl()
{
    if(m_style) {
	m_style->setParent( 0 );
	m_style->deref();
    }
    delete m_selector;
}

DOM::DOMString CSSStyleRuleImpl::selectorText() const
{
    // FIXME: Handle all the selectors in the chain for comma-separated selectors.
    if (m_selector)
        return m_selector->selectorText();
    return DOMString();
}

void CSSStyleRuleImpl::setSelectorText(DOM::DOMString /*str*/)
{
    // ###
}

bool CSSStyleRuleImpl::parseString( const DOMString &/*string*/, bool )
{
    // ###
    return false;
}

void CSSStyleRuleImpl::setDeclaration( CSSMutableStyleDeclarationImpl *style)
{
    if ( m_style != style ) {
        if (m_style)
            m_style->deref();
        m_style = style;
        if (m_style)
            m_style->ref();
    }
}

void CSSRuleListImpl::deleteRule ( unsigned long index )
{
    CSSRuleImpl *rule = m_lstCSSRules.take( index );
    if( rule )
        rule->deref();
    else
        ; // ### Throw INDEX_SIZE_ERR exception here (TODO)
}

void CSSRuleListImpl::append( CSSRuleImpl *rule )
{
    insertRule( rule, m_lstCSSRules.count() ) ;
}

unsigned long CSSRuleListImpl::insertRule( CSSRuleImpl *rule,
                                           unsigned long index )
{
    if( rule && m_lstCSSRules.insert( index, rule ) )
    {
        rule->ref();
        return index;
    }

    // ### Should throw INDEX_SIZE_ERR exception instead! (TODO)
    return 0;
}
