/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef _CSS_css_ruleimpl_h_
#define _CSS_css_ruleimpl_h_

#include "dom/dom_string.h"
#include "dom/css_rule.h"
#include "css/cssparser.h"
#include "misc/loader_client.h"
#include "misc/shared.h"

namespace khtml {
    class CachedCSSStyleSheet;
};

namespace DOM {

class CSSRule;
class CSSStyleSheet;
class CSSStyleSheetImpl;
class CSSStyleDeclarationImpl;
class MediaListImpl;

class CSSRuleImpl : public StyleBaseImpl
{
public:
    CSSRuleImpl(StyleBaseImpl *parent);

    virtual ~CSSRuleImpl();

    virtual bool isRule() { return true; }

    unsigned short type() const;
    CSSStyleSheetImpl *parentStyleSheet() const;
    CSSRuleImpl *parentRule() const;

    DOM::DOMString cssText() const;
    void setCssText(DOM::DOMString str);
    virtual void init() {}

protected:
    CSSRule::RuleType m_type;
};


class CSSCharsetRuleImpl : public CSSRuleImpl
{
public:
    CSSCharsetRuleImpl(StyleBaseImpl *parent);

    virtual ~CSSCharsetRuleImpl();

    virtual bool isCharsetRule() { return true; }

    DOMString encoding() const { return m_encoding; }
    void setEncoding(DOMString _encoding) { m_encoding = _encoding; }

protected:
    DOMString m_encoding;
};


class CSSFontFaceRuleImpl : public CSSRuleImpl
{
public:
    CSSFontFaceRuleImpl(StyleBaseImpl *parent);

    virtual ~CSSFontFaceRuleImpl();

    CSSStyleDeclarationImpl *style() const;

    virtual bool isFontFaceRule() { return true; }

protected:
    CSSStyleDeclarationImpl *m_style;
};


class CSSImportRuleImpl : public khtml::CachedObjectClient, public CSSRuleImpl
{
public:
    CSSImportRuleImpl( StyleBaseImpl *parent, const DOM::DOMString &href,
                       const DOM::DOMString &media );

    virtual ~CSSImportRuleImpl();

    DOM::DOMString href() const;
    MediaListImpl *media() const;
    CSSStyleSheetImpl *styleSheet() const;

    virtual bool isImportRule() { return true; }

    // from CachedObjectClient
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet);

    bool isLoading();
    virtual void init();

protected:
    DOMString m_strHref;
    MediaListImpl *m_lstMedia;
    CSSStyleSheetImpl *m_styleSheet;
    khtml::CachedCSSStyleSheet *m_cachedSheet;
    bool m_loading;
};

class CSSRuleList;

class CSSQuirksRuleImpl : public CSSRuleImpl
{
public:
    CSSQuirksRuleImpl( StyleBaseImpl *parent );
    CSSQuirksRuleImpl( StyleBaseImpl *parent, const QChar *&curP,
                      const QChar * endP );

    virtual ~CSSQuirksRuleImpl();

    CSSRuleListImpl *cssRules();
    unsigned long insertRule ( const DOM::DOMString &rule, unsigned long index );
    void deleteRule ( unsigned long index );

    virtual bool isQuirksRule() { return true; }
    
protected:
    CSSRuleListImpl *m_lstCSSRules;

    /* Not part of the DOM */
    unsigned long appendRule( CSSRuleImpl *rule );
};

class MediaList;

class CSSMediaRuleImpl : public CSSRuleImpl
{
public:
    CSSMediaRuleImpl( StyleBaseImpl *parent );
    CSSMediaRuleImpl( StyleBaseImpl *parent, const QChar *&curP,
                      const QChar * endP, const DOM::DOMString &media );

    virtual ~CSSMediaRuleImpl();

    MediaListImpl *media() const;
    CSSRuleListImpl *cssRules();
    unsigned long insertRule ( const DOM::DOMString &rule, unsigned long index );
    void deleteRule ( unsigned long index );

    virtual bool isMediaRule() { return true; }
protected:
    MediaListImpl *m_lstMedia;
    CSSRuleListImpl *m_lstCSSRules;

    /* Not part of the DOM */
    unsigned long appendRule( CSSRuleImpl *rule );
};


class CSSPageRuleImpl : public CSSRuleImpl
{
public:
    CSSPageRuleImpl(StyleBaseImpl *parent);

    virtual ~CSSPageRuleImpl();

    CSSStyleDeclarationImpl *style() const;

    virtual bool isPageRule() { return true; }

    DOM::DOMString selectorText() const;
    void setSelectorText(DOM::DOMString str);

protected:
    CSSStyleDeclarationImpl *m_style;
};


class CSSStyleRuleImpl : public CSSRuleImpl
{
public:
    CSSStyleRuleImpl(StyleBaseImpl *parent);

    virtual ~CSSStyleRuleImpl();

    CSSStyleDeclarationImpl *style() const;

    virtual bool isStyleRule() { return true; }

    DOM::DOMString selectorText() const;
    void setSelectorText(DOM::DOMString str);

    virtual bool parseString( const DOMString &string, bool = false );

    void setSelector( QPtrList<CSSSelector> *selector);
    void setDeclaration( CSSStyleDeclarationImpl *style);

    QPtrList<CSSSelector> *selector() { return m_selector; }
    CSSStyleDeclarationImpl *declaration() { return m_style; }

    void setNonCSSHints();

protected:
    CSSStyleDeclarationImpl *m_style;
    QPtrList<CSSSelector> *m_selector;
};



class CSSUnknownRuleImpl : public CSSRuleImpl
{
public:
    CSSUnknownRuleImpl(StyleBaseImpl *parent);

    ~CSSUnknownRuleImpl();

    virtual bool isUnknownRule() { return true; }
};


class CSSRuleListImpl : public khtml::Shared<CSSRuleListImpl>
{
public:
    CSSRuleListImpl();
    ~CSSRuleListImpl();

    unsigned long length() const;
    CSSRuleImpl *item ( unsigned long index );

    /* not part of the DOM */
    unsigned long insertRule ( CSSRuleImpl *rule, unsigned long index );
    void deleteRule ( unsigned long index );

protected:
    QPtrList<CSSRuleImpl> m_lstCSSRules;
};

}; // namespace

#endif
