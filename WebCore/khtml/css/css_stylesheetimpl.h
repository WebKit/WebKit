/*
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
 */
#ifndef _CSS_css_stylesheetimpl_h_
#define _CSS_css_stylesheetimpl_h_

#include <qvaluelist.h>
#include <qptrlist.h>

#include "dom/dom_string.h"
#include "css/cssparser.h"
#include "misc/loader_client.h"

namespace khtml {
    class CachedCSSStyleSheet;
    class DocLoader;
};

namespace DOM {

class StyleSheet;
class CSSStyleSheet;
class MediaListImpl;
class CSSRuleImpl;
class CSSRuleList;
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

    virtual bool deleteMe();

    virtual DOM::DOMString type() const { return DOMString(); }

    bool disabled() const;
    void setDisabled( bool );

    DOM::NodeImpl *ownerNode() const;
    StyleSheetImpl *parentStyleSheet() const;
    DOM::DOMString href() const;
    DOM::DOMString title() const;
    MediaListImpl *media() const;
    void setMedia( MediaListImpl *media );

protected:
    DOM::NodeImpl *m_parentNode;
    DOM::DOMString m_strHref;
    DOM::DOMString m_strTitle;
    MediaListImpl *m_media;
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

    virtual ~CSSStyleSheetImpl();

    virtual bool isCSSStyleSheet() const { return true; }

    virtual DOM::DOMString type() const { return "text/css"; }

    CSSRuleImpl *ownerRule() const;
    CSSRuleList cssRules();
    unsigned long insertRule ( const DOM::DOMString &rule, unsigned long index, int &exceptioncode );
    void deleteRule ( unsigned long index, int &exceptioncode );

    virtual bool parseString( const DOMString &string, bool strict = true );

    bool isLoading();
    void setNonCSSHints();

    virtual void checkLoaded();
    khtml::DocLoader *docLoader();
    DocumentImpl *doc() { return m_doc; }
    bool implicit() { return m_implicit; }
protected:
    DocumentImpl *m_doc;
    bool m_implicit;
};

// ----------------------------------------------------------------------------

class StyleSheetListImpl : public DomShared
{
public:
    StyleSheetListImpl();
    virtual ~StyleSheetListImpl();

    // the following two ignore implicit stylesheets
    unsigned long length() const;
    StyleSheetImpl *item ( unsigned long index );

    void add(StyleSheetImpl* s);
    void remove(StyleSheetImpl* s);

    QPtrList<StyleSheetImpl> styleSheets;
};

// ----------------------------------------------------------------------------

class MediaListImpl : public StyleBaseImpl
{
public:
    MediaListImpl( CSSStyleSheetImpl *parentSheet );
    MediaListImpl( CSSStyleSheetImpl *parentSheet,
                   const DOM::DOMString &media );
    MediaListImpl( CSSRuleImpl *parentRule );
    MediaListImpl( CSSRuleImpl *parentRule, const DOM::DOMString &media );

    virtual ~MediaListImpl();

    virtual bool isMediaList() { return true; }

    CSSStyleSheetImpl *parentStyleSheet() const;
    CSSRuleImpl *parentRule() const;
    unsigned long length() const;
    DOM::DOMString item ( unsigned long index ) const;
    void deleteMedium ( const DOM::DOMString &oldMedium );
    void appendMedium ( const DOM::DOMString &newMedium );

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

