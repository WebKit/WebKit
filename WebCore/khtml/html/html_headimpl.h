/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#ifndef HTML_HEADIMPL_H
#define HTML_HEADIMPL_H

#include "html_elementimpl.h"

class KHTMLView;

#include "misc/loader_client.h"
namespace khtml {
    class CachedCSSStyleSheet;
};


namespace DOM {

class DOMString;
class HTMLFormElementImpl;
class StyleSheetImpl;

class HTMLBaseElementImpl : public HTMLElementImpl
{
public:
    HTMLBaseElementImpl(DocumentPtr *doc);

    ~HTMLBaseElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);
    virtual void attach();

protected:
    DOMString _href;
    DOMString _target;
};



// -------------------------------------------------------------------------

class HTMLLinkElementImpl : public khtml::CachedObjectClient, public HTMLElementImpl
{
public:
    HTMLLinkElementImpl(DocumentPtr *doc);

    ~HTMLLinkElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    StyleSheetImpl *sheet() const;

    // overload from HTMLElementImpl
    virtual void attach();
    virtual void detach();
    virtual void parseAttribute(AttrImpl *attr);

    // from CachedObjectClient
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet);
    bool isLoading() const;
    void sheetLoaded();

protected:
    khtml::CachedCSSStyleSheet *m_cachedSheet;
    StyleSheetImpl *m_sheet;
    DOMString m_url;
    DOMString m_type;
    QString m_media;
    DOMString m_rel;
    bool m_loading;
    QString m_data; // needed for temporarily storing the loaded style sheet data
};

// -------------------------------------------------------------------------

class HTMLMetaElementImpl : public HTMLElementImpl
{
public:
    HTMLMetaElementImpl(DocumentPtr *doc);

    ~HTMLMetaElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);
    virtual void attach();

protected:
    DOMString _equiv;
    DOMString _content;
};

// -------------------------------------------------------------------------

class HTMLScriptElementImpl : public HTMLElementImpl
{
public:
    HTMLScriptElementImpl(DocumentPtr *doc);

    ~HTMLScriptElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLStyleElementImpl : public HTMLElementImpl
{
public:
    HTMLStyleElementImpl(DocumentPtr *doc);

    ~HTMLStyleElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    StyleSheetImpl *sheet() const { return m_sheet; }

    // overload from HTMLElementImpl
    virtual void parseAttribute(AttrImpl *attr);
    virtual NodeImpl *addChild(NodeImpl *child);
    virtual void setChanged(bool b=true);

    bool isLoading() const;
    void sheetLoaded();
    void reparseSheet();

    virtual void attach();
    virtual void detach();

protected:
    StyleSheetImpl *m_sheet;
    DOMString m_type;
    DOMString m_media;
};

// -------------------------------------------------------------------------

class HTMLTitleElementImpl : public HTMLElementImpl
{
public:
    HTMLTitleElementImpl(DocumentPtr *doc);

    ~HTMLTitleElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
    virtual void setTitle();
};

}; //namespace

#endif
