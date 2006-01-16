/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "css_ruleimpl.h"

#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "DocLoader.h"
#include "css_stylesheetimpl.h"
#include "cssparser.h"
#include <kurl.h>

namespace WebCore {

CSSStyleSheetImpl *CSSRuleImpl::parentStyleSheet() const
{
    return (parent() && parent()->isCSSStyleSheet()) ? static_cast<CSSStyleSheetImpl *>(parent()) : 0;
}

CSSRuleImpl *CSSRuleImpl::parentRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRuleImpl *>(parent()) : 0;
}

DOMString CSSRuleImpl::cssText() const
{
    // ###
    return DOMString();
}

void CSSRuleImpl::setCssText(DOMString /*str*/)
{
    // ###
}

// ---------------------------------------------------------------------------

CSSFontFaceRuleImpl::CSSFontFaceRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::FONT_FACE_RULE;
}

CSSFontFaceRuleImpl::~CSSFontFaceRuleImpl()
{
}

// --------------------------------------------------------------------------

CSSImportRuleImpl::CSSImportRuleImpl( StyleBaseImpl *parent,
                                      const DOMString &href,
                                      MediaListImpl *media )
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::IMPORT_RULE;

    m_lstMedia = media;
    if (!m_lstMedia)
        m_lstMedia = new MediaListImpl(this, DOMString());
    m_lstMedia->setParent(this);

    m_strHref = href;
    m_cachedSheet = 0;

    init();
}
CSSImportRuleImpl::CSSImportRuleImpl( StyleBaseImpl *parent,
                                      const DOMString &href,
                                      const DOMString &media )
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::IMPORT_RULE;
    m_lstMedia = new MediaListImpl(this, media);
    m_strHref = href;
    m_cachedSheet = 0;

    init();
}

CSSImportRuleImpl::~CSSImportRuleImpl()
{
    if(m_lstMedia)
        m_lstMedia->setParent( 0 );
    if(m_styleSheet)
        m_styleSheet->setParent(0);
    if(m_cachedSheet)
        m_cachedSheet->deref(this);
}

void CSSImportRuleImpl::setStyleSheet(const DOMString &url, const DOMString &sheet)
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    m_styleSheet = new CSSStyleSheetImpl(this, url);

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
      absHref = KURL(parentSheet->href().qstring(),m_strHref.qstring()).url();
    }

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

DOMString CSSImportRuleImpl::cssText() const
{
    DOMString result = "@import url(\"";
    result += m_strHref;
    result += "\")";

    if (m_lstMedia) {
        result += " ";
        result += m_lstMedia->mediaText();
    }
    result += ";";

    return result;
}

// --------------------------------------------------------------------------
CSSMediaRuleImpl::CSSMediaRuleImpl( StyleBaseImpl *parent, MediaListImpl *mediaList, CSSRuleListImpl *ruleList )
    :   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = mediaList;
    m_lstCSSRules = ruleList;
}

CSSMediaRuleImpl::CSSMediaRuleImpl(StyleBaseImpl *parent)
    :   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = 0;
    m_lstCSSRules = new CSSRuleListImpl();
}

CSSMediaRuleImpl::CSSMediaRuleImpl( StyleBaseImpl *parent, const DOMString &media )
:   CSSRuleImpl( parent )
{
    m_type = CSSRule::MEDIA_RULE;
    m_lstMedia = new MediaListImpl( this, media );
    m_lstCSSRules = new CSSRuleListImpl();
}

CSSMediaRuleImpl::~CSSMediaRuleImpl()
{
    if(m_lstMedia)
        m_lstMedia->setParent(0);

    int length = m_lstCSSRules->length();
    for (int i = 0; i < length; i++)
        m_lstCSSRules->item(i)->setParent(0);
}

unsigned CSSMediaRuleImpl::append( CSSRuleImpl *rule )
{
    if (!rule) {
        return 0;
    }

    rule->setParent(this);
    return m_lstCSSRules->insertRule( rule, m_lstCSSRules->length() );
}

unsigned CSSMediaRuleImpl::insertRule( const DOMString &rule,
                                            unsigned index )
{
    CSSParser p( strictParsing );
    CSSRuleImpl *newRule = p.parseRule( parentStyleSheet(), rule );

    if (!newRule) {
        return 0;
    }

    newRule->setParent(this);
    return m_lstCSSRules->insertRule( newRule, index );
}

DOMString CSSMediaRuleImpl::cssText() const
{
    DOMString result = "@media ";
    if (m_lstMedia) {
        result += m_lstMedia->mediaText();
        result += " ";
    }
    result += "{ \n";
    
    if (m_lstCSSRules) {
        unsigned len = m_lstCSSRules->length();
        for (unsigned i = 0; i < len; i++) {
            result += "  ";
            result += m_lstCSSRules->item(i)->cssText();
            result += "\n";
        }
    }
    
    result += "}";
    return result;
}

// ---------------------------------------------------------------------------

CSSRuleListImpl::CSSRuleListImpl(StyleListImpl *lst)
{
    if (lst) {
        unsigned len = lst->length();
        for (unsigned i = 0; i < len; ++i) {
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
}

CSSPageRuleImpl::~CSSPageRuleImpl()
{
}

DOMString CSSPageRuleImpl::selectorText() const
{
    // ###
    return DOMString();
}

void CSSPageRuleImpl::setSelectorText(DOMString /*str*/)
{
    // ###
}

// --------------------------------------------------------------------------

CSSStyleRuleImpl::CSSStyleRuleImpl(StyleBaseImpl *parent)
    : CSSRuleImpl(parent)
{
    m_type = CSSRule::STYLE_RULE;
    m_selector = 0;
}

CSSStyleRuleImpl::~CSSStyleRuleImpl()
{
    if(m_style)
        m_style->setParent( 0 );
    delete m_selector;
}

DOMString CSSStyleRuleImpl::selectorText() const
{
    if (m_selector) {
        DOMString str;
        for (CSSSelector *s = m_selector; s; s = s->next()) {
            if (s != m_selector)
                str += ", ";
            str += s->selectorText();
        }
        return str;
    }
    return DOMString();
}

void CSSStyleRuleImpl::setSelectorText(DOMString /*str*/)
{
    // ###
}

DOMString CSSStyleRuleImpl::cssText() const
{
    DOMString result = selectorText();
    
    result += " { ";
    result += m_style->cssText();
    result += "}";
    
    return result;
}

bool CSSStyleRuleImpl::parseString( const DOMString &/*string*/, bool )
{
    // ###
    return false;
}

void CSSStyleRuleImpl::setDeclaration( CSSMutableStyleDeclarationImpl *style)
{
    if (m_style != style)
        m_style = style;
}

void CSSRuleListImpl::deleteRule ( unsigned index )
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

unsigned CSSRuleListImpl::insertRule( CSSRuleImpl *rule,
                                           unsigned index )
{
    if( rule && m_lstCSSRules.insert( index, rule ) )
    {
        rule->ref();
        return index;
    }

    // ### Should throw INDEX_SIZE_ERR exception instead! (TODO)
    return 0;
}

}
