/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
class CSSStyleSheet;
class CachedCSSStyleSheet;
class CachedResourceLoader;
class Document;
class MediaQuerySet;
class StyleRuleBase;
class StyleRuleImport;
struct CSSNamespace;

typedef int ExceptionCode;

class StyleSheetInternal : public RefCounted<StyleSheetInternal> {
public:
    static PassRefPtr<StyleSheetInternal> create()
    {
        return adoptRef(new StyleSheetInternal(static_cast<StyleRuleImport*>(0), String(), KURL(), String()));
    }
    static PassRefPtr<StyleSheetInternal> create(Node* ownerNode)
    {
        return adoptRef(new StyleSheetInternal(ownerNode, String(), KURL(), String()));
    }
    static PassRefPtr<StyleSheetInternal> create(Node* ownerNode, const String& originalURL, const KURL& finalURL, const String& charset)
    {
        return adoptRef(new StyleSheetInternal(ownerNode, originalURL, finalURL, charset));
    }
    static PassRefPtr<StyleSheetInternal> create(StyleRuleImport* ownerRule, const String& originalURL, const KURL& finalURL, const String& charset)
    {
        return adoptRef(new StyleSheetInternal(ownerRule, originalURL, finalURL, charset));
    }
    static PassRefPtr<StyleSheetInternal> createInline(Node* ownerNode, const KURL& finalURL)
    {
        return adoptRef(new StyleSheetInternal(ownerNode, finalURL.string(), finalURL, String()));
    }

    ~StyleSheetInternal();

    void addNamespace(CSSParser*, const AtomicString& prefix, const AtomicString& uri);
    const AtomicString& determineNamespace(const AtomicString& prefix);

    void styleSheetChanged();

    bool parseString(const String&, CSSParserMode = CSSStrictMode);

    bool parseStringAtLine(const String&, CSSParserMode, int startLineNumber);

    bool isLoading() const;

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

    // Rules other than @charset and @import.
    const Vector<RefPtr<StyleRuleBase> >& childRules() const { return m_childRules; }
    const Vector<RefPtr<StyleRuleImport> >& importRules() const { return m_importRules; }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtr<MediaQuerySet>);

    void notifyLoadedSheet(const CachedCSSStyleSheet*);
    
    StyleSheetInternal* parentStyleSheet() const;
    Node* ownerNode() const { return m_ownerNode; }
    void clearOwnerNode() { m_ownerNode = 0; }
    StyleRuleImport* ownerRule() const { return m_ownerRule; }
    void clearOwnerRule() { m_ownerRule = 0; }
    
    // Note that href is the URL that started the redirect chain that led to
    // this style sheet. This property probably isn't useful for much except
    // the JavaScript binding (which needs to use this value for security).
    String originalURL() const { return m_originalURL; }
    String title() const { return m_title; }
    void setTitle(const String& title) { m_title = title; }
    
    void setFinalURL(const KURL& finalURL) { m_finalURL = finalURL; }
    const KURL& finalURL() const { return m_finalURL; }
    KURL baseURL() const;

    unsigned ruleCount() const;
    
    bool wrapperInsertRule(PassRefPtr<StyleRuleBase>, unsigned index);
    void wrapperDeleteRule(unsigned index);

    PassRefPtr<CSSRule> createChildRuleCSSOMWrapper(unsigned index, CSSStyleSheet* parentWrapper);

private:
    StyleSheetInternal(Node* ownerNode, const String& originalURL, const KURL& finalURL, const String& charset);
    StyleSheetInternal(StyleRuleImport* ownerRule, const String& originalURL, const KURL& finalURL, const String& charset);
    
    void clearCharsetRule();
    bool hasCharsetRule() const { return !m_encodingFromCharsetRule.isNull(); }

    Node* m_ownerNode;
    StyleRuleImport* m_ownerRule;

    String m_originalURL;
    KURL m_finalURL;
    String m_title;

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
};

class CSSStyleSheet : public StyleSheet {
public:
    static RefPtr<CSSStyleSheet> create(PassRefPtr<StyleSheetInternal> sheet, CSSImportRule* ownerRule = 0) 
    { 
        return adoptRef(new CSSStyleSheet(sheet, ownerRule));
    }

    virtual ~CSSStyleSheet();

    virtual CSSStyleSheet* parentStyleSheet() const OVERRIDE;
    virtual Node* ownerNode() const OVERRIDE { return m_internal->ownerNode(); }
    virtual MediaList* media() const OVERRIDE;
    virtual String href() const OVERRIDE { return m_internal->originalURL(); }  
    virtual String title() const OVERRIDE { return m_internal->title(); }
    virtual bool disabled() const OVERRIDE { return m_isDisabled; }
    virtual void setDisabled(bool) OVERRIDE;
    
    PassRefPtr<CSSRuleList> cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);
    
    // IE Extensions
    PassRefPtr<CSSRuleList> rules();
    int addRule(const String& selector, const String& style, int index, ExceptionCode&);
    int addRule(const String& selector, const String& style, ExceptionCode&);
    void removeRule(unsigned index, ExceptionCode& ec) { deleteRule(index, ec); }
    
    // For CSSRuleList.
    unsigned length() const;
    CSSRule* item(unsigned index);

    virtual void clearOwnerNode() OVERRIDE { m_internal->clearOwnerNode(); }
    virtual CSSImportRule* ownerRule() const OVERRIDE { return m_ownerRule; }
    virtual KURL baseURL() const OVERRIDE { return m_internal->baseURL(); }
    virtual bool isLoading() const OVERRIDE { return m_internal->isLoading(); }
    
    void clearOwnerRule() { m_ownerRule = 0; }
    void styleSheetChanged() { m_internal->styleSheetChanged(); }
    Document* findDocument() { return m_internal->findDocument(); }
    
    void clearChildRuleCSSOMWrappers();

    StyleSheetInternal* internal() const { return m_internal.get(); }

private:
    CSSStyleSheet(PassRefPtr<StyleSheetInternal>, CSSImportRule* ownerRule);

    virtual bool isCSSStyleSheet() const { return true; }
    virtual String type() const { return "text/css"; }
    
    RefPtr<StyleSheetInternal> m_internal;
    bool m_isDisabled;

    CSSImportRule* m_ownerRule;
    mutable Vector<RefPtr<CSSRule> > m_childRuleCSSOMWrappers;
    mutable OwnPtr<CSSRuleList> m_ruleListCSSOMWrapper;
};

} // namespace

#endif
