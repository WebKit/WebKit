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

#include "kdom.h"
#include "NodeImpl.h"
#include "KDOMCSSParser.h"
#include "DocumentImpl.h"
#include "CSSRuleListImpl.h"
#include "CSSStyleRuleImpl.h"
#include "CSSStyleSheetImpl.h"
#include "CSSImportRuleImpl.h"

using namespace KDOM;

CSSStyleSheetImpl::CSSStyleSheetImpl(CSSStyleSheetImpl *parentSheet, DOMStringImpl *href)
: StyleSheetImpl(parentSheet, href)
{
    m_lstChildren = new Q3PtrList<StyleBaseImpl>();

    m_doc = 0;
    m_implicit = false;
    m_namespaces = 0;
}

CSSStyleSheetImpl::CSSStyleSheetImpl(NodeImpl *parentNode, DOMStringImpl *href, bool _implicit)
: StyleSheetImpl(parentNode, href)
{
    m_lstChildren = new Q3PtrList<StyleBaseImpl>();

    m_doc = (!isDocumentNode(parentNode) ? parentNode->ownerDocument() : static_cast<DocumentImpl *>(parentNode));
    m_implicit = _implicit;
    m_namespaces = 0;
}

CSSStyleSheetImpl::CSSStyleSheetImpl(CSSRuleImpl *ownerRule, DOMStringImpl *href)
: StyleSheetImpl(ownerRule, href)
{
    m_lstChildren = new Q3PtrList<StyleBaseImpl>();

    m_doc = 0;
    m_implicit = false;
    m_namespaces = 0;
}

CSSStyleSheetImpl::CSSStyleSheetImpl(NodeImpl *parentNode, CSSStyleSheetImpl *orig)
: StyleSheetImpl(parentNode, orig->m_strHref)
{
    m_lstChildren = new Q3PtrList<StyleBaseImpl>();

    StyleBaseImpl *rule;
    for(rule = orig->m_lstChildren->first(); rule != 0; rule = orig->m_lstChildren->next())
    {
        m_lstChildren->append(rule);
        rule->setParent(this);
    }

    m_doc = parentNode->ownerDocument();
    m_implicit = false;
    m_namespaces = 0;
}

CSSStyleSheetImpl::CSSStyleSheetImpl(CSSRuleImpl *ownerRule, CSSStyleSheetImpl *orig)
: StyleSheetImpl(ownerRule, orig->m_strHref)
{
    // m_lstChildren is deleted in StyleListImpl
    m_lstChildren = new Q3PtrList<StyleBaseImpl>();

    StyleBaseImpl *rule;
    for(rule = orig->m_lstChildren->first(); rule != 0; rule = orig->m_lstChildren->next())
    {
        m_lstChildren->append(rule);
        rule->setParent(this);
    }

    m_doc  = 0;
    m_implicit = false;
    m_namespaces = 0;
}

CSSStyleSheetImpl::~CSSStyleSheetImpl()
{
    delete m_namespaces;
}

DOMStringImpl *CSSStyleSheetImpl::type() const
{
    return DOMString("text/css").handle();
}

CSSRuleImpl *CSSStyleSheetImpl::ownerRule() const
{
    if(m_parent && m_parent->isRule())
        return static_cast<CSSRuleImpl *>(m_parent);

    return 0;
}

CSSRuleListImpl *CSSStyleSheetImpl::cssRules()
{
    CSSRuleListImpl *ret = new CSSRuleListImpl();
    ret->ref();

    for(unsigned long i = 0; i < length() ; ++i)
    {
        StyleBaseImpl *style = item(i);
        if(style->isRule())
            ret->insertRule(static_cast<CSSRuleImpl *>(style), length());
    }

    return ret;
}

unsigned long CSSStyleSheetImpl::insertRule(DOMStringImpl *rule, unsigned long index)
{
    if(index > m_lstChildren->count())
    {
        throw new DOMExceptionImpl(INDEX_SIZE_ERR);
        return 0;
    }

    CSSParser *p = createCSSParser(m_strictParsing);
    if(!p)
        return 0;
        
    CSSRuleImpl *r = p->parseRule(this, rule);
    delete p;

    if(!r)
    {
        throw new DOMExceptionImpl(SYNTAX_ERR);
        return 0;
    }

    // TODO: HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted at the specified index e.g. if an
    //       @import rule is inserted after a standard rule set or other at-rule.
    m_lstChildren->insert(index, r);
    return index;
}

void CSSStyleSheetImpl::deleteRule(unsigned long index)
{
    StyleBaseImpl *b = m_lstChildren->take(index);
    if(!b)
    {
        throw new DOMExceptionImpl(INDEX_SIZE_ERR);
        return;
    }

    b->deref();
}

void CSSStyleSheetImpl::addNamespace(CSSParser *p, DOMStringImpl *prefix, DOMStringImpl *uri)
{
    if(!uri || uri->isEmpty())
        return;

    m_namespaces = new CSSNamespace(prefix, uri, m_namespaces);

    if(!prefix || prefix->isEmpty())
    {
        // Set the default namespace on the parser so that selectors that
        // omit namespace info will be able to pick it up easily.
        p->defaultNamespace = m_doc->getId(NodeImpl::NamespaceId, uri, false /* false */);
    }
}

void CSSStyleSheetImpl::determineNamespace(Q_UINT32 &id, DOMStringImpl *prefix)
{
    // If the stylesheet has no namespaces we can just return.  There won't be
    // any need to ever check namespace values in selectors.
    if(!m_namespaces)
        return;

    if(!prefix || prefix->isEmpty())// No namespace. If an element/attribute has a namespace, we won't match it.
        id = makeId(noNamespace, localNamePart(id));
    else if(DOMString(prefix) == "*") // We'll match any namespace.
        id = makeId(anyNamespace, localNamePart(id));
    else
    {
        CSSNamespace *ns = m_namespaces->namespaceForPrefix(prefix);
        if(ns)
        {
            // Look up the id for this namespace URI.
            id = makeId(m_doc->getId(NodeImpl::NamespaceId, ns->uri(), false /* false */), localNamePart(id));
        }
    }
}

void CSSStyleSheetImpl::checkLoaded() const
{
    if(isLoading())
        return;

    if(m_parent)
        m_parent->checkLoaded();

    if(m_parentNode)
        m_parentNode->sheetLoaded();
}

void CSSStyleSheetImpl::setNonCSSHints()
{
    StyleBaseImpl *rule = m_lstChildren->first();
    while(rule)
    {
        if(rule->isStyleRule())
            static_cast<CSSStyleRuleImpl *>(rule)->setNonCSSHints();

        rule = m_lstChildren->next();
    }
}

bool CSSStyleSheetImpl::isLoading() const
{
    StyleBaseImpl *rule;
    for(rule = m_lstChildren->first();rule != 0;rule = m_lstChildren->next())
    {
        if(rule->isImportRule())
        {
            CSSImportRuleImpl *import = static_cast<CSSImportRuleImpl *>(rule);
#ifdef CSS_STYLESHEET_DEBUG
            kdDebug( 6080 ) << "found import" << endl;
#endif
            if(import->isLoading())
            {
#ifdef CSS_STYLESHEET_DEBUG
                kdDebug( 6080 ) << "--> not loaded" << endl;
#endif
                return true;
            }
        }
    }

    return false;
}

bool CSSStyleSheetImpl::parseString(DOMStringImpl *string, bool strict)
{
#ifdef CSS_STYLESHEET_DEBUG
    kdDebug(6080) << "parsing sheet, len=" << string.length() << ", sheet is " << string.string() << endl;
#endif

    m_strictParsing = strict;
    
    CSSParser *p = createCSSParser(m_strictParsing);
    if(!p)
        return false;
        
    p->parseSheet(this, string);
    delete p;
    
    return true;
}

// vim:ts=4:noet
