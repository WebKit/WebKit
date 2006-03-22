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

#include "HTMLElement.h"
#include "CachedObjectClient.h"
#include "css/css_stylesheetimpl.h"

namespace WebCore {

class CSSStyleSheet;
class CachedCSSStyleSheet;
class CachedScript;
class String;
class FrameView;
class HTMLFormElement;
class StyleSheet;

class HTMLBaseElement : public HTMLElement
{
public:
    HTMLBaseElement(Document *doc);
    ~HTMLBaseElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    String href() const { return m_href; }
    String target() const { return m_target; }

    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    void process();
    
    void setHref(const String &);
    void setTarget(const String &);

protected:
    String m_href;
    String m_target;
};



// -------------------------------------------------------------------------

class HTMLLinkElement : public WebCore::CachedObjectClient, public HTMLElement
{
public:
    HTMLLinkElement(Document *doc);
    ~HTMLLinkElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    bool disabled() const;
    void setDisabled(bool);

    String charset() const;
    void setCharset(const String &);

    String href() const;
    void setHref(const String &);

    String hreflang() const;
    void setHreflang(const String &);

    String media() const;
    void setMedia(const String &);

    String rel() const;
    void setRel(const String &);

    String rev() const;
    void setRev(const String &);

    String target() const;
    void setTarget(const String &);

    String type() const;
    void setType(const String &);

    StyleSheet* sheet() const { return m_sheet.get(); }

    // overload from HTMLElement
    virtual void parseMappedAttribute(MappedAttribute *attr);

    void process();

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // from CachedObjectClient
    virtual void setStyleSheet(const WebCore::String &url, const WebCore::String &sheet);
    bool isLoading() const;
    void sheetLoaded();

    bool isAlternate() const { return m_disabledState == 0 && m_alternate; }
    bool isDisabled() const { return m_disabledState == 2; }
    bool isEnabledViaScript() const { return m_disabledState == 1; }

    int disabledState() { return m_disabledState; }
    void setDisabledState(bool _disabled);

    virtual bool isURLAttribute(Attribute *attr) const;
    
    void tokenizeRelAttribute(const AtomicString& rel);

protected:
    WebCore::CachedCSSStyleSheet *m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    String m_url;
    String m_type;
    String m_media;
    int m_disabledState; // 0=unset(default), 1=enabled via script, 2=disabled
    bool m_loading : 1;
    bool m_alternate : 1;
    bool m_isStyleSheet : 1;
    bool m_isIcon : 1;
};

// -------------------------------------------------------------------------

class HTMLMetaElement : public HTMLElement
{
public:
    HTMLMetaElement(Document *doc);
    ~HTMLMetaElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void insertedIntoDocument();

    void process();

    String content() const;
    void setContent(const String &);

    String httpEquiv() const;
    void setHttpEquiv(const String &);

    String name() const;
    void setName(const String &);

    String scheme() const;
    void setScheme(const String &);

protected:
    String m_equiv;
    String m_content;
};

// -------------------------------------------------------------------------

class HTMLScriptElement : public HTMLElement, public WebCore::CachedObjectClient
{
public:
    HTMLScriptElement(Document *doc);
    ~HTMLScriptElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void notifyFinished(WebCore::CachedObject *finishedObj);

    virtual void childrenChanged();

    virtual bool isURLAttribute(Attribute *attr) const;

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }
    virtual void closeRenderer();

    void evaluateScript(const String &URL, const String &script);

    String text() const;
    void setText(const String &);

    String htmlFor() const;
    void setHtmlFor(const String &);

    String event() const;
    void setEvent(const String &);

    String charset() const;
    void setCharset(const String &);

    bool defer() const;
    void setDefer(bool);

    String src() const;
    void setSrc(const String &);

    String type() const;
    void setType(const String &);

private:
    WebCore::CachedScript *m_cachedScript;
    bool m_createdByParser;
    bool m_evaluated;
};

// -------------------------------------------------------------------------

class HTMLStyleElement : public HTMLElement
{
public:
    HTMLStyleElement(Document *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    StyleSheet *sheet() const { return m_sheet.get(); }

    // overload from HTMLElement
    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

    bool isLoading() const;
    void sheetLoaded();

    bool disabled() const;
    void setDisabled(bool);

    String media() const;
    void setMedia(const String &);

    String type() const;
    void setType(const String &);

protected:
    RefPtr<CSSStyleSheet> m_sheet;
    bool m_loading;
    String m_type;
    String m_media;
};

// -------------------------------------------------------------------------

class HTMLTitleElement : public HTMLElement
{
public:
    HTMLTitleElement(Document *doc);
    ~HTMLTitleElement();

    virtual bool checkDTD(const Node* newChild) { return newChild->isTextNode(); }

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged();

    String text() const;
    void setText(const String &);

protected:
    String m_title;
};

} //namespace

#endif
