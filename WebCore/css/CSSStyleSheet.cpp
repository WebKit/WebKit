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
#include "CSSStyleSheet.h"

#include "CSSImportRule.h"
#include "CSSNamespace.h"
#include "cssparser.h"
#include "CSSRuleList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Node.h"

namespace WebCore {

CSSStyleSheet::CSSStyleSheet(CSSStyleSheet* parentSheet, String href)
    : StyleSheet(parentSheet, href)
    , m_doc(parentSheet ? parentSheet->doc() : 0)
    , m_implicit(false)
    , m_namespaces(0)
{
}

CSSStyleSheet::CSSStyleSheet(Node *parentNode, String href, bool _implicit)
    : StyleSheet(parentNode, href)
    , m_doc(parentNode->document())
    , m_implicit(_implicit) 
    , m_namespaces(0)
{
}

CSSStyleSheet::CSSStyleSheet(CSSRule *ownerRule, String href)
    : StyleSheet(ownerRule, href)
    , m_doc(0)
    , m_implicit(false)
    , m_namespaces(0)
{
}

CSSStyleSheet::~CSSStyleSheet()
{
    delete m_namespaces;
}

CSSRule *CSSStyleSheet::ownerRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule*>(parent()) : 0;
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

unsigned CSSStyleSheet::addRule(const String &selector, const String &style, int index, ExceptionCode& ec)
{
    if (index == -1)
        index = length();
    return insertRule(selector + " { " + style + " }", index, ec);
}

CSSRuleList *CSSStyleSheet::cssRules()
{
    return new CSSRuleList(this);
}

void CSSStyleSheet::deleteRule(unsigned index, ExceptionCode& ec)
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
    CSSParser p(strict);
    p.parseSheet(this, string);
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

DocLoader *CSSStyleSheet::docLoader()
{
    if (!m_doc) // doc is 0 for the user- and default-sheet!
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

}
