/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 *
 * $Id$
 */
#include "css_ruleimpl.h"
#include "css_rule.h"
#include "css_stylesheet.h"
#include "css_stylesheetimpl.h"
#include "css_valueimpl.h"
#include "cssparser.h"

#include "misc/loader.h"

#include "dom_exception.h"
#include "dom_string.h"
using namespace DOM;

#include <kdebug.h>

CSSRuleImpl::CSSRuleImpl(StyleBaseImpl *parent)
    : StyleListImpl(parent)
{
    m_type = CSSRule::UNKNOWN_RULE;
}

CSSRuleImpl::~CSSRuleImpl()
{
}

unsigned short CSSRuleImpl::type() const
{
    return m_type;
}

CSSStyleSheetImpl *CSSRuleImpl::parentStyleSheet() const
{
    if( !m_parent ) return 0;
    if( m_parent->isCSSStyleSheet() ) return static_cast<CSSStyleSheetImpl *>(m_parent);
    return 0;
}

CSSRuleImpl *CSSRuleImpl::parentRule() const
{
    if( !m_parent ) return 0;
    if( m_parent->isRule() ) return static_cast<CSSRuleImpl *>(m_parent);
    return 0;
}

DOM::DOMString CSSRuleImpl::cssText() const
{
    // ###
    return 0;
}

void CSSRuleImpl::setCssText(DOM::DOMString /*str*/)
{
    // ###
}

// ---------------------------------------------------------------------------

CSSCharsetRuleImpl::CSSCharsetRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::CHARSET_RULE;
    m_encoding = 0;
}

CSSCharsetRuleImpl::~CSSCharsetRuleImpl()
{
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

CSSStyleDeclarationImpl *CSSFontFaceRuleImpl::style() const
{
    return m_style;
}

// --------------------------------------------------------------------------

CSSImportRuleImpl::CSSImportRuleImpl(StyleBaseImpl *parent, const DOM::DOMString &href,
				     MediaListImpl *media)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::IMPORT_RULE;
    m_lstMedia = media;
    m_strHref = href;
    m_styleSheet = 0;
    kdDebug( 6080 ) << "CSSImportRule: requesting sheet " << href.string() << " " << baseUrl().string() << endl;

    khtml::DocLoader *docLoader = 0;
    StyleBaseImpl *root = this;
    while (root->parent())
	root = root->parent();
    if (root->isCSSStyleSheet())
	docLoader = static_cast<CSSStyleSheetImpl*>(root)->docLoader();

    // we must have a docLoader !
    // ### pass correct charset here!!
    m_cachedSheet = docLoader->requestStyleSheet(href, baseUrl(), QString::null);

    m_cachedSheet->ref(this);
    m_loading = true;
}

CSSImportRuleImpl::~CSSImportRuleImpl()
{
    if(m_lstMedia) m_lstMedia->deref();
    if(m_styleSheet) m_styleSheet->deref();
    m_cachedSheet->deref(this);
}

DOMString CSSImportRuleImpl::href() const
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

void CSSImportRuleImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet)
{
    kdDebug( 6080 ) << "CSSImportRule::setStyleSheet()" << endl;

    m_styleSheet = new CSSStyleSheetImpl(this, url);
    m_styleSheet->ref();
    m_styleSheet->parseString(sheet);
    m_loading = false;

    checkLoaded();
}

bool CSSImportRuleImpl::isLoading()
{
    if(m_loading) return true;
    if(m_styleSheet->isLoading()) return true;
    return false;
}
// --------------------------------------------------------------------------


CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = 0;
}

CSSMediaRuleImpl::~CSSMediaRuleImpl()
{
    if(m_lstMedia) m_lstMedia->deref();
}

MediaListImpl *CSSMediaRuleImpl::media() const
{
    return m_lstMedia;
}

CSSRuleList CSSMediaRuleImpl::cssRules()
{
    return this;
}

unsigned long CSSMediaRuleImpl::insertRule( const DOMString &/*rule*/, unsigned long /*index*/ )
{
    // ###
    return 0;
}

void CSSMediaRuleImpl::deleteRule( unsigned long /*index*/ )
{
    // ###
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

CSSStyleDeclarationImpl *CSSPageRuleImpl::style() const
{
    return m_style;
}

DOM::DOMString CSSPageRuleImpl::selectorText() const
{
    // ###
    return 0;
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

CSSStyleDeclarationImpl *CSSStyleRuleImpl::style() const
{
    return m_style;
}

DOM::DOMString CSSStyleRuleImpl::selectorText() const
{
    // ###
    return 0;
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

void CSSStyleRuleImpl::setSelector( QList<CSSSelector> *selector)
{
    m_selector = selector;
}

void CSSStyleRuleImpl::setDeclaration( CSSStyleDeclarationImpl *style)
{
    if(m_style) m_style->deref();
    m_style = style;
    if(m_style) m_style->ref();
}

void CSSStyleRuleImpl::setNonCSSHints()
{
    CSSSelector *s = m_selector->first();
    while ( s ) {
	s->nonCSSHint = true;
	s = m_selector->next();
    }
}


// --------------------------------------------------------------------------

CSSUnknownRuleImpl::CSSUnknownRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
}

CSSUnknownRuleImpl::~CSSUnknownRuleImpl()
{
}

