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
    class CachedScript;
}


namespace DOM {

class DOMString;
class HTMLFormElementImpl;
class StyleSheetImpl;
class CSSStyleSheetImpl;

class HTMLBaseElementImpl : public HTMLElementImpl
{
public:
    HTMLBaseElementImpl(DocumentImpl *doc);
    ~HTMLBaseElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    DOMString href() const { return m_href; }
    DOMString target() const { return m_target; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    void process();
    
    void setHref(const DOMString &);
    void setTarget(const DOMString &);

protected:
    DOMString m_href;
    DOMString m_target;
};



// -------------------------------------------------------------------------

class HTMLLinkElementImpl : public khtml::CachedObjectClient, public HTMLElementImpl
{
public:
    HTMLLinkElementImpl(DocumentImpl *doc);
    ~HTMLLinkElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    bool disabled() const;
    void setDisabled(bool);

    DOMString charset() const;
    void setCharset(const DOMString &);

    DOMString href() const;
    void setHref(const DOMString &);

    DOMString hreflang() const;
    void setHreflang(const DOMString &);

    DOMString media() const;
    void setMedia(const DOMString &);

    DOMString rel() const;
    void setRel(const DOMString &);

    DOMString rev() const;
    void setRev(const DOMString &);

    DOMString target() const;
    void setTarget(const DOMString &);

    DOMString type() const;
    void setType(const DOMString &);

    StyleSheetImpl* sheet() const { return m_sheet; }

    // overload from HTMLElementImpl
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

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
    HTMLMetaElementImpl(DocumentImpl *doc);
    ~HTMLMetaElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void insertedIntoDocument();

    void process();

    DOMString content() const;
    void setContent(const DOMString &);

    DOMString httpEquiv() const;
    void setHttpEquiv(const DOMString &);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString scheme() const;
    void setScheme(const DOMString &);

protected:
    DOMString m_equiv;
    DOMString m_content;
};

// -------------------------------------------------------------------------

class HTMLScriptElementImpl : public HTMLElementImpl, public khtml::CachedObjectClient
{
public:
    HTMLScriptElementImpl(DocumentImpl *doc);
    ~HTMLScriptElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void notifyFinished(khtml::CachedObject *finishedObj);

    virtual void childrenChanged();

    virtual bool isURLAttribute(AttributeImpl *attr) const;

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    void evaluateScript(const QString &, const DOMString &);

    DOMString text() const;
    void setText(const DOMString &);

    DOMString htmlFor() const;
    void setHtmlFor(const DOMString &);

    DOMString event() const;
    void setEvent(const DOMString &);

    DOMString charset() const;
    void setCharset(const DOMString &);

    bool defer() const;
    void setDefer(bool);

    DOMString src() const;
    void setSrc(const DOMString &);

    DOMString type() const;
    void setType(const DOMString &);

private:
    khtml::CachedScript *m_cachedScript;
    bool m_createdByParser;
    bool m_evaluated;
};

// -------------------------------------------------------------------------

class HTMLStyleElementImpl : public HTMLElementImpl
{
public:
    HTMLStyleElementImpl(DocumentImpl *doc);
    ~HTMLStyleElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    StyleSheetImpl *sheet() const { return m_sheet; }

    // overload from HTMLElementImpl
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

    bool isLoading() const;
    void sheetLoaded();

    bool disabled() const;
    void setDisabled(bool);

    DOMString media() const;
    void setMedia(const DOMString &);

    DOMString type() const;
    void setType(const DOMString &);

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
    HTMLTitleElementImpl(DocumentImpl *doc);
    ~HTMLTitleElementImpl();

    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

    DOMString text() const;
    void setText(const DOMString &);

protected:
    DOMString m_title;
};

} //namespace

#endif
