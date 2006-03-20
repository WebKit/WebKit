/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
#include "css_stylesheetimpl.h"

#include "ExceptionCode.h"
#include "PlatformString.h"
#include "css_ruleimpl.h"
#include "css_valueimpl.h"
#include "cssparser.h"
#include "HTMLDocument.h"
#include "loader.h"

namespace WebCore {

// --------------------------------------------------------------------------------

StyleSheet::StyleSheet(StyleSheet *parentSheet, String href)
    : StyleList(parentSheet)
{
    m_disabled = false;
    m_parentNode = 0;
    m_strHref = href;
}


StyleSheet::StyleSheet(WebCore::Node *parentNode, String href)
    : StyleList(0)
{
    m_parentNode = parentNode;
    m_disabled = false;
    m_strHref = href;
}

StyleSheet::StyleSheet(StyleBase *owner, String href)
    : StyleList(owner)
{
    m_disabled = false;
    m_parentNode = 0;
    m_strHref = href;
}

StyleSheet::~StyleSheet()
{
    if (m_media)
        m_media->setParent(0);
}

StyleSheet *StyleSheet::parentStyleSheet() const
{
    return (parent() && parent()->isStyleSheet()) ? static_cast<StyleSheet *>(parent()) : 0;
}

void StyleSheet::setMedia(MediaList *media)
{
    m_media = media;
}

// -----------------------------------------------------------------------


CSSStyleSheet::CSSStyleSheet(CSSStyleSheet *parentSheet, String href)
    : StyleSheet(parentSheet, href)
{
    m_doc = parentSheet ? parentSheet->doc() : 0;
    m_implicit = false;
    m_namespaces = 0;
}

CSSStyleSheet::CSSStyleSheet(Node *parentNode, String href, bool _implicit)
    : StyleSheet(parentNode, href)
{
    m_doc = parentNode->getDocument();
    m_implicit = _implicit; 
    m_namespaces = 0;
}

CSSStyleSheet::CSSStyleSheet(CSSRule *ownerRule, String href)
    : StyleSheet(ownerRule, href)
{
    m_doc = 0;
    m_implicit = false;
    m_namespaces = 0;
}

CSSRule *CSSStyleSheet::ownerRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule *>(parent()) : 0;
}

unsigned CSSStyleSheet::insertRule(const String& rule, unsigned index, ExceptionCode& ec)
{
    ec = 0;
    if (index > length()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    CSSParser p(useStrictParsing());
    RefPtr<CSSRule> r = p.parseRule(this, rule);

    if (!r) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // ###
    // HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted at the specified index e.g. if an
    //@import rule is inserted after a standard rule set or other at-rule.
    insert(index, r.release());
    
    styleSheetChanged();
    
    return index;
}

unsigned CSSStyleSheet::addRule( const String &selector, const String &style, int index, ExceptionCode& ec)
{
    if (index == -1)
        index = length();
    return insertRule(selector + " { " + style + " }", index, ec);
}

CSSRuleList *CSSStyleSheet::cssRules()
{
    return new CSSRuleList(this);
}

void CSSStyleSheet::deleteRule( unsigned index, ExceptionCode& ec)
{
    if (index >= length()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ec = 0;
    remove(index);
    styleSheetChanged();
}

void CSSStyleSheet::addNamespace(CSSParser* p, const AtomicString& prefix, const AtomicString& uri)
{
    if (uri.isEmpty())
        return;

    m_namespaces = new CSSNamespace(prefix, uri, m_namespaces);
    
    if (prefix.isEmpty())
        // Set the default namespace on the parser so that selectors that omit namespace info will
        // be able to pick it up easily.
        p->defaultNamespace = uri;
}

const AtomicString& CSSStyleSheet::determineNamespace(const AtomicString& prefix)
{
    if (prefix.isEmpty())
        return nullAtom; // No namespace. If an element/attribute has a namespace, we won't match it.
    else if (prefix == starAtom)
        return starAtom; // We'll match any namespace.
    else if (m_namespaces) {
        CSSNamespace* ns = m_namespaces->namespaceForPrefix(prefix);
        if (ns)
            return ns->uri();
    }
    return nullAtom; // Assume we wont match any namespaces.
}

bool CSSStyleSheet::parseString(const String &string, bool strict)
{
    setStrictParsing(strict);
    CSSParser p( strict );
    p.parseSheet( this, string );
    return true;
}

bool CSSStyleSheet::isLoading()
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBase* rule = item(i);
        if (rule->isImportRule() && static_cast<CSSImportRule*>(rule)->isLoading())
            return true;
    }
    return false;
}

void CSSStyleSheet::checkLoaded()
{
    if (isLoading())
        return;
    if (parent())
        parent()->checkLoaded();
    if (m_parentNode)
        m_parentNode->sheetLoaded();
}

WebCore::DocLoader *CSSStyleSheet::docLoader()
{
    if ( !m_doc ) // doc is 0 for the user- and default-sheet!
        return 0;

    // ### remove? (clients just use sheet->doc()->docLoader())
    return m_doc->docLoader();
}

void CSSStyleSheet::styleSheetChanged()
{
    /* FIXME: We don't need to do everything updateStyleSelector does,
     * basically we just need to recreate the document's selector with the
     * already existing style sheets.
     */
    if (m_doc)
        m_doc->updateStyleSelector();
}

// ---------------------------------------------------------------------------


StyleSheetList::~StyleSheetList()
{
    for ( DeprecatedPtrListIterator<StyleSheet> it ( styleSheets ); it.current(); ++it )
        it.current()->deref();
}

void StyleSheetList::add( StyleSheet* s )
{
    if ( !styleSheets.containsRef( s ) ) {
        s->ref();
        styleSheets.append( s );
    }
}

void StyleSheetList::remove( StyleSheet* s )
{
    if ( styleSheets.removeRef( s ) )
        s->deref();
}

unsigned StyleSheetList::length() const
{
    // hack so implicit BODY stylesheets don't get counted here
    unsigned l = 0;
    DeprecatedPtrListIterator<StyleSheet> it(styleSheets);
    for (; it.current(); ++it) {
        if (!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheet*>(it.current())->implicit())
            l++;
    }
    return l;
}

StyleSheet *StyleSheetList::item ( unsigned index )
{
    unsigned l = 0;
    DeprecatedPtrListIterator<StyleSheet> it(styleSheets);
    for (; it.current(); ++it) {
        if (!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheet*>(it.current())->implicit()) {
            if (l == index)
                return it.current();
            l++;
        }
    }
    return 0;
}

// --------------------------------------------------------------------------------------------

MediaList::MediaList( CSSStyleSheet *parentSheet,
                              const String &media )
    : StyleBase( parentSheet )
{
    setMediaText( media );
}

MediaList::MediaList( CSSRule *parentRule, const String &media )
    : StyleBase(parentRule)
{
    setMediaText( media );
}

bool MediaList::contains( const String &medium ) const
{
    return m_lstMedia.count() == 0 || m_lstMedia.contains( medium ) ||
            m_lstMedia.contains( "all" );
}

CSSStyleSheet *MediaList::parentStyleSheet() const
{
    return parent()->isCSSStyleSheet() ? static_cast<CSSStyleSheet *>(parent()) : 0;
}

CSSRule *MediaList::parentRule() const
{
    return parent()->isRule() ? static_cast<CSSRule *>(parent()) : 0;
}

void MediaList::deleteMedium( const String &oldMedium )
{
    for ( DeprecatedValueList<String>::Iterator it = m_lstMedia.begin(); it != m_lstMedia.end(); ++it ) {
        if ((*it) == oldMedium) {
            m_lstMedia.remove( it );
            return;
        }
    }
}

WebCore::String MediaList::mediaText() const
{
    String text = "";
    for (DeprecatedValueList<String>::ConstIterator it = m_lstMedia.begin(); it != m_lstMedia.end(); ++it) {
        if (text.length() > 0)
            text += ", ";
        text += *it;
    }
    return text;
}

void MediaList::setMediaText(const WebCore::String &value)
{
    m_lstMedia.clear();
    DeprecatedStringList list = DeprecatedStringList::split( ',', value.deprecatedString() );
    for ( DeprecatedStringList::Iterator it = list.begin(); it != list.end(); ++it )
    {
        String medium = (*it).stripWhiteSpace();
        if (!medium.isEmpty())
            m_lstMedia.append( medium );
    }
}

}
