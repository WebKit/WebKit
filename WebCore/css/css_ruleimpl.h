/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006 Apple Computer, Inc.
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

#ifndef CSS_css_ruleimpl_h_
#define CSS_css_ruleimpl_h_

#include "CachedObjectClient.h"
#include "PlatformString.h"
#include "Shared.h"
#include "css_base.h"
#include "css_valueimpl.h"

namespace WebCore {

class CachedCSSStyleSheet;
class CSSMutableStyleDeclaration;
class CSSStyleSheet;
class CSSStyleSheet;
class MediaList;

enum CSSRuleType { UNKNOWN_RULE, STYLE_RULE, CHARSET_RULE, IMPORT_RULE, MEDIA_RULE, FONT_FACE_RULE, PAGE_RULE };

class CSSRule : public StyleBase
{
public:
    CSSRule(StyleBase *parent)
        : StyleBase(parent), m_type(UNKNOWN_RULE) {}

    virtual bool isRule() { return true; }
    unsigned short type() const { return m_type; }

    CSSStyleSheet *parentStyleSheet() const;
    CSSRule *parentRule() const;

    virtual String cssText() const;
    void setCssText(WebCore::String str);

protected:
    CSSRuleType m_type;
};


class CSSCharsetRule : public CSSRule
{
public:
    CSSCharsetRule(StyleBase *parent)
        : CSSRule(parent) { m_type = CHARSET_RULE; }

    virtual bool isCharsetRule() { return true; }
    virtual String cssText() const;

    String encoding() const { return m_encoding; }
    void setEncoding(String _encoding) { m_encoding = _encoding; }

protected:
    String m_encoding;
};


class CSSFontFaceRule : public CSSRule
{
public:
    CSSFontFaceRule(StyleBase *parent);
    virtual ~CSSFontFaceRule();

    CSSMutableStyleDeclaration *style() const { return m_style.get(); }

    virtual bool isFontFaceRule() { return true; }

protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
};


class CSSImportRule : public CachedObjectClient, public CSSRule
{
public:
    CSSImportRule(StyleBase* parent, const String& href, MediaList*);
    virtual ~CSSImportRule();

    String href() const { return m_strHref; }
    MediaList* media() const { return m_lstMedia.get(); }
    CSSStyleSheet* styleSheet() const { return m_styleSheet.get(); }

    virtual bool isImportRule() { return true; }
    virtual String cssText() const;
  
    bool isLoading() const;

    // from CachedObjectClient
    virtual void setStyleSheet(const String& url, const String& sheet);

    virtual void insertedIntoParent();

protected:
    String m_strHref;
    RefPtr<MediaList> m_lstMedia;
    RefPtr<CSSStyleSheet> m_styleSheet;
    CachedCSSStyleSheet* m_cachedSheet;
    bool m_loading;
};

class MediaList;

class CSSRuleList : public Shared<CSSRuleList>
{
public:
    CSSRuleList() { }
    CSSRuleList(StyleList *);
    ~CSSRuleList();

    unsigned length() const { return m_lstCSSRules.count(); }
    CSSRule *item ( unsigned index ) { return m_lstCSSRules.at( index ); }

    /* not part of the DOM */
    unsigned insertRule ( CSSRule *rule, unsigned index );
    void deleteRule ( unsigned index );
    void append( CSSRule *rule );

protected:
    DeprecatedPtrList<CSSRule> m_lstCSSRules;
};

class CSSMediaRule : public CSSRule
{
public:
    CSSMediaRule( StyleBase *parent );
    CSSMediaRule( StyleBase *parent, const WebCore::String &media );
    CSSMediaRule( StyleBase *parent, MediaList *mediaList, CSSRuleList *ruleList );
    virtual ~CSSMediaRule();

    MediaList *media() const { return m_lstMedia.get(); }
    CSSRuleList *cssRules() { return m_lstCSSRules.get(); }

    unsigned insertRule ( const WebCore::String &rule, unsigned index );
    void deleteRule ( unsigned index ) { m_lstCSSRules->deleteRule( index ); }

    virtual bool isMediaRule() { return true; }
    virtual String cssText() const;

    /* Not part of the DOM */
    unsigned append( CSSRule *rule );
protected:
    RefPtr<MediaList> m_lstMedia;
    RefPtr<CSSRuleList> m_lstCSSRules;
};


class CSSPageRule : public CSSRule
{
public:
    CSSPageRule(StyleBase *parent);
    virtual ~CSSPageRule();

    CSSMutableStyleDeclaration *style() const { return m_style.get(); }

    virtual bool isPageRule() { return true; }

    WebCore::String selectorText() const;
    void setSelectorText(WebCore::String str);

protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
};

class CSSImportantRule;

class CSSStyleRule : public CSSRule
{
public:
    CSSStyleRule(StyleBase* parent);
    virtual ~CSSStyleRule();

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

    virtual bool isStyleRule() { return true; }
    virtual String cssText() const;

    WebCore::String selectorText() const;
    void setSelectorText(WebCore::String str);

    virtual bool parseString( const String &string, bool = false );

    void setSelector(CSSSelector* selector) { m_selector = selector; }
    void setDeclaration(PassRefPtr<CSSMutableStyleDeclaration>);

    CSSSelector* selector() { return m_selector; }
    CSSMutableStyleDeclaration* declaration() { return m_style.get(); }
 
protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
    CSSSelector* m_selector;
};

class CSSUnknownRule : public CSSRule
{
public:
    CSSUnknownRule(StyleBase *parent) : CSSRule(parent) {}

    virtual bool isUnknownRule() { return true; }
};

} // namespace

#endif
