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

CSSRuleImpl::CSSRuleImpl(StyleBaseImpl *parent)
    : StyleBaseImpl(parent)
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

CSSCharsetRuleImpl::CSSCharsetRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent), m_encoding()
{
    m_type = CSSRule::CHARSET_RULE;
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
    if(m_lstMedia) m_lstMedia->deref();
    if(m_styleSheet) m_styleSheet->deref();
    if(m_cachedSheet) m_cachedSheet->deref(this);
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

    if ( m_styleSheet ) m_styleSheet->deref();
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
    kdDebug( 6080 ) << "CSSImportRule: requesting sheet " << m_strHref.string() << endl;
    kdDebug( 6080 ) << "CSSImportRule: requesting absolute url " << absHref.string() << endl;

    // we must have a docLoader !
    // ### pass correct charset here!!
    m_cachedSheet = docLoader->requestStyleSheet(absHref, QString::null);

    if (m_cachedSheet)
    {
      m_cachedSheet->ref(this);
      m_loading = true;
    }
}

// --------------------------------------------------------------------------


CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent)
    :   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = 0;
    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();
}

CSSMediaRuleImpl::CSSMediaRuleImpl( StyleBaseImpl *parent, const QChar *&curP,
                              const QChar *endP, const DOM::DOMString &media )
:   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = new MediaListImpl( this, media );
    m_lstMedia->ref();
    m_lstCSSRules = new CSSRuleListImpl();
    m_lstCSSRules->ref();

    // Parse CSS data
    while( curP < endP )
    {
//         kdDebug( 6080 ) << "Style rule: '" << QString( curP, endP - curP )
//                         << "'" << endl;
        CSSRuleImpl *rule = parseStyleRule( curP, endP );
        if ( rule ) {
            rule->ref();
            appendRule( rule );
        }
        if (!curP) break;
        while( curP < endP && *curP == QChar( ' ' ) )
            curP++;
    }
}

CSSMediaRuleImpl::~CSSMediaRuleImpl()
{
    if( m_lstMedia ) {
	m_lstMedia->setParent( 0 );
        m_lstMedia->deref();
    }
    m_lstCSSRules->deref();
}

MediaListImpl *CSSMediaRuleImpl::media() const
{
    return m_lstMedia;
}

CSSRuleListImpl *CSSMediaRuleImpl::cssRules()
{
    return m_lstCSSRules;
}

unsigned long CSSMediaRuleImpl::appendRule( CSSRuleImpl *rule )
{
    return rule ? m_lstCSSRules->insertRule( rule, m_lstCSSRules->length() ) : 0;
}

unsigned long CSSMediaRuleImpl::insertRule( const DOMString &rule,
                                            unsigned long index )
{
    const QChar *curP = rule.unicode();
    CSSRuleImpl *newRule = parseRule( curP, curP + rule.length() );

    return newRule ? m_lstCSSRules->insertRule( newRule, index ) : 0;
}

void CSSMediaRuleImpl::deleteRule( unsigned long index )
{
    m_lstCSSRules->deleteRule( index );
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

CSSStyleDeclarationImpl *CSSPageRuleImpl::style() const
{
    return m_style;
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

CSSStyleDeclarationImpl *CSSStyleRuleImpl::style() const
{
    return m_style;
}

DOM::DOMString CSSStyleRuleImpl::selectorText() const
{
    if ( m_selector && m_selector->first() ) {
        // ### m_selector will be a single selector hopefully. so ->first() will disappear
        CSSSelector* cs = m_selector->first();
        //cs->print(); // debug
        return cs->selectorText();
    }
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

void CSSStyleRuleImpl::setSelector( QPtrList<CSSSelector> *selector)
{
    m_selector = selector;
}

void CSSStyleRuleImpl::setDeclaration( CSSStyleDeclarationImpl *style)
{
    if ( m_style != style ) {
    if(m_style) m_style->deref();
    m_style = style;
    if(m_style) m_style->ref();
    }
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

// --------------------------------------------------------------------------

CSSRuleListImpl::CSSRuleListImpl()
{
}

unsigned long CSSRuleListImpl::length() const
{
    return m_lstCSSRules.count();
}

CSSRuleImpl *CSSRuleListImpl::item ( unsigned long index )
{
    return m_lstCSSRules.at( index );
}

void CSSRuleListImpl::deleteRule ( unsigned long index )
{
    CSSRuleImpl *rule = m_lstCSSRules.take( index );
    if( rule )
        rule->deref();
    else
        ; // ### Throw INDEX_SIZE_ERR exception here (TODO)
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

