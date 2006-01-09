/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef _CSS_css_stylesheetimpl_h_
#define _CSS_css_stylesheetimpl_h_

#include <qvaluelist.h>
#include <qptrlist.h>

#include "dom/dom_string.h"
#include "css/css_base.h"

namespace khtml {
    class CachedCSSStyleSheet;
    class DocLoader;
};

namespace DOM {

class CSSParser;
class StyleSheet;
class CSSStyleSheet;
class MediaListImpl;
class CSSRuleImpl;
class CSSRuleListImpl;
class CSSStyleRuleImpl;
class CSSValueImpl;
class NodeImpl;
class DocumentImpl;

class StyleSheetImpl : public StyleListImpl
{
public:
    StyleSheetImpl(DOM::NodeImpl *ownerNode, DOM::DOMString href = DOMString());
    StyleSheetImpl(StyleSheetImpl *parentSheet, DOM::DOMString href = DOMString());
    StyleSheetImpl(StyleBaseImpl *owner, DOM::DOMString href  = DOMString());
    StyleSheetImpl(khtml::CachedCSSStyleSheet *cached, DOM::DOMString href  = DOMString());
    virtual ~StyleSheetImpl();

    virtual bool isStyleSheet() const { return true; }

    virtual DOM::DOMString type() const { return DOMString(); }

    bool disabled() const { return m_disabled; }
    void setDisabled( bool disabled ) { m_disabled = disabled; }

    DOM::NodeImpl *ownerNode() const { return m_parentNode; }
    StyleSheetImpl *parentStyleSheet() const;
    DOM::DOMString href() const { return m_strHref; }
    DOM::DOMString title() const { return m_strTitle; }
    MediaListImpl *media() const { return m_media.get(); }
    void setMedia( MediaListImpl *media );

    virtual bool isLoading() { return false; }

protected:
    DOM::NodeImpl *m_parentNode;
    DOM::DOMString m_strHref;
    DOM::DOMString m_strTitle;
    RefPtr<MediaListImpl> m_media;
    bool m_disabled;
};

class CSSStyleSheetImpl : public StyleSheetImpl
{
public:
    CSSStyleSheetImpl(DOM::NodeImpl *parentNode, DOM::DOMString href = DOMString(), bool _implicit = false);
    CSSStyleSheetImpl(CSSStyleSheetImpl *parentSheet, DOM::DOMString href = DOMString());
    CSSStyleSheetImpl(CSSRuleImpl *ownerRule, DOM::DOMString href = DOMString());
    // clone from a cached version of the sheet
    CSSStyleSheetImpl(DOM::NodeImpl *parentNode, CSSStyleSheetImpl *orig);
    CSSStyleSheetImpl(CSSRuleImpl *ownerRule, CSSStyleSheetImpl *orig);
    
    ~CSSStyleSheetImpl() { delete m_namespaces; }
    
    virtual bool isCSSStyleSheet() const { return true; }

    virtual DOM::DOMString type() const { return "text/css"; }

    CSSRuleImpl *ownerRule() const;
    CSSRuleListImpl *cssRules();
    unsigned insertRule ( const DOM::DOMString &rule, unsigned index, int &exceptioncode );
    void deleteRule ( unsigned index, int &exceptioncode );
    unsigned addRule ( const DOMString &selector, const DOMString &style, int index, int &exceptioncode );

    void addNamespace(CSSParser* p, const AtomicString& prefix, const AtomicString& uri);
    const AtomicString& determineNamespace(const AtomicString& prefix);
    
    virtual bool parseString( const DOMString &string, bool strict = true );

    virtual bool isLoading();

    virtual void checkLoaded();
    khtml::DocLoader *docLoader();
    DocumentImpl *doc() { return m_doc; }
    bool implicit() { return m_implicit; }
protected:
    DocumentImpl *m_doc;
    bool m_implicit;
    CSSNamespace* m_namespaces;
};

// ----------------------------------------------------------------------------

class StyleSheetListImpl : public khtml::Shared<StyleSheetListImpl>
{
public:
    StyleSheetListImpl() {}
    ~StyleSheetListImpl();

    // the following two ignore implicit stylesheets
    unsigned length() const;
    StyleSheetImpl *item ( unsigned index );

    void add(StyleSheetImpl* s);
    void remove(StyleSheetImpl* s);

    QPtrList<StyleSheetImpl> styleSheets;
};

// ----------------------------------------------------------------------------

class MediaListImpl : public StyleBaseImpl
{
public:
    MediaListImpl()
	: StyleBaseImpl( 0 ) {}
    MediaListImpl( CSSStyleSheetImpl *parentSheet )
        : StyleBaseImpl(parentSheet) {}
    MediaListImpl( CSSStyleSheetImpl *parentSheet,
                   const DOM::DOMString &media );
    //MediaListImpl( CSSRuleImpl *parentRule );
    MediaListImpl( CSSRuleImpl *parentRule, const DOM::DOMString &media );

    virtual bool isMediaList() { return true; }

    CSSStyleSheetImpl *parentStyleSheet() const;
    CSSRuleImpl *parentRule() const;
    unsigned length() const { return m_lstMedia.count(); }
    DOM::DOMString item ( unsigned index ) const { return m_lstMedia[index]; }
    void deleteMedium ( const DOM::DOMString &oldMedium );
    void appendMedium ( const DOM::DOMString &newMedium ) { m_lstMedia.append(newMedium); }

    DOM::DOMString mediaText() const;
    void setMediaText(const DOM::DOMString &value);

    /**
     * Check if the list contains either the requested medium, or the
     * catch-all "all" media type. Returns true when found, false otherwise.
     * Since not specifying media types should be treated as "all" according
     * to DOM specs, an empty list always returns true.
     *
     * _NOT_ part of the DOM!
     */
    bool contains( const DOM::DOMString &medium ) const;

protected:
    QValueList<DOM::DOMString> m_lstMedia;
};


}; // namespace

#endif

