/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSStyleSheet_h
#define CSSStyleSheet_h

#include "CSSParserMode.h"
#include "StyleSheet.h"

namespace WebCore {

class CSSCharsetRule;
class CSSImportRule;
class CSSParser;
class CSSRule;
class CSSRuleList;
class CachedCSSStyleSheet;
class CachedResourceLoader;
class Document;
class MediaQuerySet;
class StyleRuleBase;
class StyleRuleImport;
struct CSSNamespace;

typedef int ExceptionCode;

class CSSStyleSheet : public StyleSheet {
public:
    static PassRefPtr<CSSStyleSheet> create()
    {
        return adoptRef(new CSSStyleSheet(static_cast<StyleRuleImport*>(0), String(), KURL(), String()));
    }
    static PassRefPtr<CSSStyleSheet> create(Node* ownerNode)
    {
        return adoptRef(new CSSStyleSheet(ownerNode, String(), KURL(), String()));
    }
    static PassRefPtr<CSSStyleSheet> create(Node* ownerNode, const String& originalURL, const KURL& finalURL, const String& charset)
    {
        return adoptRef(new CSSStyleSheet(ownerNode, originalURL, finalURL, charset));
    }
    static PassRefPtr<CSSStyleSheet> create(StyleRuleImport* ownerRule, const String& originalURL, const KURL& finalURL, const String& charset)
    {
        return adoptRef(new CSSStyleSheet(ownerRule, originalURL, finalURL, charset));
    }
    static PassRefPtr<CSSStyleSheet> createInline(Node* ownerNode, const KURL& finalURL)
    {
        return adoptRef(new CSSStyleSheet(ownerNode, finalURL.string(), finalURL, String()));
    }

    virtual ~CSSStyleSheet();

    virtual CSSStyleSheet* parentStyleSheet() const OVERRIDE;

    PassRefPtr<CSSRuleList> cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);

    // IE Extensions
    PassRefPtr<CSSRuleList> rules();
    int addRule(const String& selector, const String& style, int index, ExceptionCode&);
    int addRule(const String& selector, const String& style, ExceptionCode&);
    void removeRule(unsigned index, ExceptionCode& ec) { deleteRule(index, ec); }

    void addNamespace(CSSParser*, const AtomicString& prefix, const AtomicString& uri);
    const AtomicString& determineNamespace(const AtomicString& prefix);

    void styleSheetChanged();

    virtual bool parseString(const String&, CSSParserMode = CSSStrictMode);

    bool parseStringAtLine(const String&, CSSParserMode, int startLineNumber);

    virtual bool isLoading();

    void checkLoaded();
    void startLoadingDynamicSheet();

    Node* findStyleSheetOwnerNode() const;
    Document* findDocument();

    const String& charset() const { return m_charset; }

    bool loadCompleted() const { return m_loadCompleted; }

    KURL completeURL(const String& url) const;
    void addSubresourceStyleURLs(ListHashSet<KURL>&);

    void setCSSParserMode(CSSParserMode cssParserMode) { m_cssParserMode = cssParserMode; }
    CSSParserMode cssParserMode() const { return m_cssParserMode; }

    void setIsUserStyleSheet(bool b) { m_isUserStyleSheet = b; }
    bool isUserStyleSheet() const { return m_isUserStyleSheet; }
    void setHasSyntacticallyValidCSSHeader(bool b) { m_hasSyntacticallyValidCSSHeader = b; }
    bool hasSyntacticallyValidCSSHeader() const { return m_hasSyntacticallyValidCSSHeader; }

    void parserAppendRule(PassRefPtr<StyleRuleBase>);
    void parserSetEncodingFromCharsetRule(const String& encoding); 

    void clearRules();

    unsigned length() const;
    CSSRule* item(unsigned index);

    // Rules other than @charset and @import.
    const Vector<RefPtr<StyleRuleBase> >& childRules() const { return m_childRules; }
    const Vector<RefPtr<StyleRuleImport> >& importRules() const { return m_importRules; }

    virtual MediaList* media() const OVERRIDE;

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtr<MediaQuerySet>);

    void notifyLoadedSheet(const CachedCSSStyleSheet*);
    
    virtual CSSImportRule* ownerRule() const OVERRIDE;
    void clearOwnerRule() { m_ownerRule = 0; }
    
    CSSImportRule* ensureCSSOMWrapper(StyleRuleImport*);

private:
    CSSStyleSheet(Node* ownerNode, const String& originalURL, const KURL& finalURL, const String& charset);
    CSSStyleSheet(StyleRuleImport* ownerRule, const String& originalURL, const KURL& finalURL, const String& charset);

    virtual bool isCSSStyleSheet() const { return true; }
    virtual String type() const { return "text/css"; }
    
    void clearCharsetRule();
    bool hasCharsetRule() const { return !m_encodingFromCharsetRule.isNull(); }

    PassRefPtr<CSSRule> createChildRuleCSSOMWrapper(unsigned index);

    StyleRuleImport* m_ownerRule;
    String m_encodingFromCharsetRule;
    Vector<RefPtr<StyleRuleImport> > m_importRules;
    Vector<RefPtr<StyleRuleBase> > m_childRules;
    OwnPtr<CSSNamespace> m_namespaces;
    String m_charset;
    RefPtr<MediaQuerySet> m_mediaQueries;

    bool m_loadCompleted : 1;
    CSSParserMode m_cssParserMode;
    bool m_isUserStyleSheet : 1;
    bool m_hasSyntacticallyValidCSSHeader : 1;
    bool m_didLoadErrorOccur : 1;

    mutable Vector<RefPtr<CSSRule> > m_childRuleCSSOMWrappers;
    mutable OwnPtr<CSSRuleList> m_ruleListCSSOMWrapper;
};

} // namespace

#endif
