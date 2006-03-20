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
#include <KURL.h>

namespace WebCore {

CSSStyleSheet *CSSRule::parentStyleSheet() const
{
    return (parent() && parent()->isCSSStyleSheet()) ? static_cast<CSSStyleSheet *>(parent()) : 0;
}

CSSRule *CSSRule::parentRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule *>(parent()) : 0;
}

String CSSRule::cssText() const
{
    // ###
    return String();
}

void CSSRule::setCssText(String /*str*/)
{
    // ###
}

// ---------------------------------------------------------------------------

CSSFontFaceRule::CSSFontFaceRule(StyleBase *parent)
    : CSSRule(parent)
{
    m_type = FONT_FACE_RULE;
}

CSSFontFaceRule::~CSSFontFaceRule()
{
}

// --------------------------------------------------------------------------

CSSImportRule::CSSImportRule(StyleBase* parent, const String& href, MediaList* media)
    : CSSRule(parent)
    , m_strHref(href)
    , m_lstMedia(media)
    , m_cachedSheet(0)
    , m_loading(false)
{
    m_type = IMPORT_RULE;

    if (m_lstMedia)
        m_lstMedia->setParent(this);
    else
        m_lstMedia = new MediaList(this, String());
}

CSSImportRule::~CSSImportRule()
{
    if (m_lstMedia)
        m_lstMedia->setParent(0);
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void CSSImportRule::setStyleSheet(const String &url, const String &sheet)
{
    if (m_styleSheet)
        m_styleSheet->setParent(0);
    m_styleSheet = new CSSStyleSheet(this, url);

    CSSStyleSheet *parent = parentStyleSheet();
    m_styleSheet->parseString(sheet, !parent || parent->useStrictParsing());
    m_loading = false;

    checkLoaded();
}

bool CSSImportRule::isLoading() const
{
    return m_loading || (m_styleSheet && m_styleSheet->isLoading());
}

void CSSImportRule::insertedIntoParent()
{
    StyleBase* root = this;
    StyleBase* parent;
    while ((parent = root->parent()))
        root = parent;
    if (!root->isCSSStyleSheet())
        return;
    DocLoader* docLoader = static_cast<CSSStyleSheet*>(root)->docLoader();
    if (!docLoader)
        return;

    String absHref = m_strHref;
    CSSStyleSheet* parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(parentSheet->href().deprecatedString(), m_strHref.deprecatedString()).url();

    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBase*>(this)->parent(); parent; parent = parent->parent())
        if (absHref == parent->baseURL())
            return;
    
    // ### pass correct charset here!!
    m_cachedSheet = docLoader->requestStyleSheet(absHref, DeprecatedString::null);
    if (m_cachedSheet) {
        m_loading = true;
        m_cachedSheet->ref(this);
    }
}

String CSSImportRule::cssText() const
{
    String result = "@import url(\"";
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
CSSMediaRule::CSSMediaRule( StyleBase *parent, MediaList *mediaList, CSSRuleList *ruleList )
    :   CSSRule( parent )
{
    m_type = MEDIA_RULE;
    m_lstMedia = mediaList;
    m_lstCSSRules = ruleList;
}

CSSMediaRule::CSSMediaRule(StyleBase *parent)
    :   CSSRule( parent )
{
    m_type = MEDIA_RULE;
    m_lstMedia = 0;
    m_lstCSSRules = new CSSRuleList();
}

CSSMediaRule::CSSMediaRule( StyleBase *parent, const String &media )
:   CSSRule( parent )
{
    m_type = MEDIA_RULE;
    m_lstMedia = new MediaList( this, media );
    m_lstCSSRules = new CSSRuleList();
}

CSSMediaRule::~CSSMediaRule()
{
    if(m_lstMedia)
        m_lstMedia->setParent(0);

    int length = m_lstCSSRules->length();
    for (int i = 0; i < length; i++)
        m_lstCSSRules->item(i)->setParent(0);
}

unsigned CSSMediaRule::append( CSSRule *rule )
{
    if (!rule) {
        return 0;
    }

    rule->setParent(this);
    return m_lstCSSRules->insertRule( rule, m_lstCSSRules->length() );
}

unsigned CSSMediaRule::insertRule(const String& rule, unsigned index)
{
    CSSParser p(useStrictParsing());
    RefPtr<CSSRule> newRule = p.parseRule(parentStyleSheet(), rule);
    if (!newRule)
        return 0;
    newRule->setParent(this);
    return m_lstCSSRules->insertRule(newRule.get(), index);
}

String CSSMediaRule::cssText() const
{
    String result = "@media ";
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

CSSRuleList::CSSRuleList(StyleList *lst)
{
    if (lst) {
        unsigned len = lst->length();
        for (unsigned i = 0; i < len; ++i) {
            StyleBase *style = lst->item(i);
            if (style->isRule())
                append(static_cast<CSSRule *>(style));
        }
    }
}

CSSRuleList::~CSSRuleList()
{
    CSSRule* rule;
    while ( !m_lstCSSRules.isEmpty() && ( rule = m_lstCSSRules.take( 0 ) ) )
        rule->deref();
}

// ---------------------------------------------------------------------------

CSSPageRule::CSSPageRule(StyleBase *parent)
    : CSSRule(parent)
{
    m_type = PAGE_RULE;
}

CSSPageRule::~CSSPageRule()
{
}

String CSSPageRule::selectorText() const
{
    // ###
    return String();
}

void CSSPageRule::setSelectorText(String /*str*/)
{
    // ###
}

// --------------------------------------------------------------------------

CSSStyleRule::CSSStyleRule(StyleBase *parent)
    : CSSRule(parent)
{
    m_type = STYLE_RULE;
    m_selector = 0;
}

CSSStyleRule::~CSSStyleRule()
{
    if (m_style)
        m_style->setParent(0);
    delete m_selector;
}

String CSSStyleRule::selectorText() const
{
    if (m_selector) {
        String str;
        for (CSSSelector *s = m_selector; s; s = s->next()) {
            if (s != m_selector)
                str += ", ";
            str += s->selectorText();
        }
        return str;
    }
    return String();
}

void CSSStyleRule::setSelectorText(String /*str*/)
{
    // ###
}

String CSSStyleRule::cssText() const
{
    String result = selectorText();
    
    result += " { ";
    result += m_style->cssText();
    result += "}";
    
    return result;
}

bool CSSStyleRule::parseString( const String &/*string*/, bool )
{
    // ###
    return false;
}

void CSSStyleRule::setDeclaration(PassRefPtr<CSSMutableStyleDeclaration> style)
{
    m_style = style;
}

void CSSRuleList::deleteRule ( unsigned index )
{
    CSSRule *rule = m_lstCSSRules.take( index );
    if( rule )
        rule->deref();
    else
        ; // ### Throw INDEX_SIZE_ERR exception here (TODO)
}

void CSSRuleList::append( CSSRule *rule )
{
    insertRule( rule, m_lstCSSRules.count() ) ;
}

unsigned CSSRuleList::insertRule( CSSRule *rule, unsigned index )
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
