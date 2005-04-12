/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef HTML_HEADIMPL_H
#define HTML_HEADIMPL_H

#include "html/html_elementimpl.h"
#include "misc/loader_client.h"
#include "css/css_stylesheetimpl.h"

class KHTMLView;

namespace khtml {
    class CachedCSSStyleSheet;
};


namespace DOM {

class DOMString;
class HTMLFormElementImpl;
class StyleSheetImpl;
class CSSStyleSheetImpl;

class HTMLBaseElementImpl : public HTMLElementImpl
{
public:
    HTMLBaseElementImpl(DocumentPtr *doc);
    ~HTMLBaseElementImpl();

    DOMString href() const { return m_href; }
    DOMString target() const { return m_target; }

    virtual Id id() const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    void process();
    
protected:
    DOMString m_href;
    DOMString m_target;
};



// -------------------------------------------------------------------------

class HTMLLinkElementImpl : public khtml::CachedObjectClient, public HTMLElementImpl
{
public:
    HTMLLinkElementImpl(DocumentPtr *doc);
    ~HTMLLinkElementImpl();

    virtual Id id() const;

    StyleSheetImpl* sheet() const { return m_sheet; }

    // overload from HTMLElementImpl
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    void process();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // from CachedObjectClient
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet);
    bool isLoading() const;
    void sheetLoaded();

    bool isAlternate() const { return m_disabledState == 0 && m_alternate; }
    bool isDisabled() const { return m_disabledState == 2; }
    bool isEnabledViaScript() const { return m_disabledState == 1; }

    int disabledState() { return m_disabledState; }
    void setDisabledState(bool _disabled);

    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
    void tokenizeRelAttribute(const AtomicString& rel);

protected:
    khtml::CachedCSSStyleSheet *m_cachedSheet;
    CSSStyleSheetImpl *m_sheet;
    DOMString m_url;
    DOMString m_type;
    QString m_media;
    int m_disabledState; // 0=unset(default), 1=enabled via script, 2=disabled
    bool m_loading : 1;
    bool m_alternate : 1;
    bool m_isStyleSheet : 1;
    bool m_isIcon : 1;
    QString m_data; // needed for temporarily storing the loaded style sheet data
};

// -------------------------------------------------------------------------

class HTMLMetaElementImpl : public HTMLElementImpl
{
public:
    HTMLMetaElementImpl(DocumentPtr *doc);

    ~HTMLMetaElementImpl();

    virtual Id id() const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual void insertedIntoDocument();

    void process();

protected:
    DOMString m_equiv;
    DOMString m_content;
};

// -------------------------------------------------------------------------

class HTMLScriptElementImpl : public HTMLElementImpl
{
public:
    HTMLScriptElementImpl(DocumentPtr *doc);

    ~HTMLScriptElementImpl();

    virtual Id id() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
};

// -------------------------------------------------------------------------

class HTMLStyleElementImpl : public HTMLElementImpl
{
public:
    HTMLStyleElementImpl(DocumentPtr *doc);
    ~HTMLStyleElementImpl();

    virtual Id id() const;

    StyleSheetImpl *sheet() const { return m_sheet; }

    // overload from HTMLElementImpl
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

    bool isLoading() const;
    void sheetLoaded();

protected:
    CSSStyleSheetImpl *m_sheet;
    bool m_loading;
    DOMString m_type;
    QString m_media;
};

// -------------------------------------------------------------------------

class HTMLTitleElementImpl : public HTMLElementImpl
{
public:
    HTMLTitleElementImpl(DocumentPtr *doc);

    ~HTMLTitleElementImpl();

    virtual Id id() const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

protected:
    DOMString m_title;
};

}; //namespace

#endif
