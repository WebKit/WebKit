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
#include "CSSRule.h"
#include "StyleSheet.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/AtomicStringHash.h>

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
class SecurityOrigin;
class StyleRuleBase;
class StyleRuleImport;

typedef int ExceptionCode;

class StyleSheetInternal : public RefCounted<StyleSheetInternal> {
public:
    static PassRefPtr<StyleSheetInternal> create(const CSSParserContext& context = CSSParserContext(CSSStrictMode))
    {
        return adoptRef(new StyleSheetInternal(0, String(), KURL(), context));
    }
    static PassRefPtr<StyleSheetInternal> create(const String& originalURL, const KURL& finalURL, const CSSParserContext& context)
    {
        return adoptRef(new StyleSheetInternal(0, originalURL, finalURL, context));
    }
    static PassRefPtr<StyleSheetInternal> create(StyleRuleImport* ownerRule, const String& originalURL, const KURL& finalURL, const CSSParserContext& context)
    {
        return adoptRef(new StyleSheetInternal(ownerRule, originalURL, finalURL, context));
    }

    ~StyleSheetInternal();
    
    const CSSParserContext& parserContext() const { return m_parserContext; }

    const AtomicString& determineNamespace(const AtomicString& prefix);

    void parseAuthorStyleSheet(const CachedCSSStyleSheet*, const SecurityOrigin*);
    bool parseString(const String&);
    bool parseStringAtLine(const String&, int startLineNumber);

    bool isCacheable() const;

    bool isLoading() const;

    void checkLoaded();
    void startLoadingDynamicSheet();

    StyleSheetInternal* rootStyleSheet() const;
    Node* singleOwnerNode() const;
    Document* singleOwnerDocument() const;

    const String& charset() const { return m_parserContext.charset; }

    bool loadCompleted() const { return m_loadCompleted; }
    bool hasFailedOrCanceledSubresources() const;

    KURL completeURL(const String& url) const;
    void addSubresourceStyleURLs(ListHashSet<KURL>&);

    void setIsUserStyleSheet(bool b) { m_isUserStyleSheet = b; }
    bool isUserStyleSheet() const { return m_isUserStyleSheet; }
    void setHasSyntacticallyValidCSSHeader(bool b) { m_hasSyntacticallyValidCSSHeader = b; }
    bool hasSyntacticallyValidCSSHeader() const { return m_hasSyntacticallyValidCSSHeader; }

    void parserAddNamespace(const AtomicString& prefix, const AtomicString& uri);
    void parserAppendRule(PassRefPtr<StyleRuleBase>);
    void parserSetEncodingFromCharsetRule(const String& encoding); 
    void parserSetUsesRemUnits(bool b) { m_usesRemUnits = b; }

    void clearRules();

    bool hasCharsetRule() const { return !m_encodingFromCharsetRule.isNull(); }
    String encodingFromCharsetRule() const { return m_encodingFromCharsetRule; }
    // Rules other than @charset and @import.
    const Vector<RefPtr<StyleRuleBase> >& childRules() const { return m_childRules; }
    const Vector<RefPtr<StyleRuleImport> >& importRules() const { return m_importRules; }

    void notifyLoadedSheet(const CachedCSSStyleSheet*);
    
    StyleSheetInternal* parentStyleSheet() const;
    StyleRuleImport* ownerRule() const { return m_ownerRule; }
    void clearOwnerRule() { m_ownerRule = 0; }
    
    // Note that href is the URL that started the redirect chain that led to
    // this style sheet. This property probably isn't useful for much except
    // the JavaScript binding (which needs to use this value for security).
    String originalURL() const { return m_originalURL; }
    
    const KURL& finalURL() const { return m_finalURL; }
    const KURL& baseURL() const { return m_parserContext.baseURL; }

    unsigned ruleCount() const;
    StyleRuleBase* ruleAt(unsigned index) const;

    bool usesRemUnits() const { return m_usesRemUnits; }

    unsigned estimatedSizeInBytes() const;
    
    bool wrapperInsertRule(PassRefPtr<StyleRuleBase>, unsigned index);
    void wrapperDeleteRule(unsigned index);

    PassRefPtr<StyleSheetInternal> copy() const { return adoptRef(new StyleSheetInternal(*this)); }

    void registerClient(CSSStyleSheet*);
    void unregisterClient(CSSStyleSheet*);
    bool hasOneClient() { return m_clients.size() == 1; }

    bool isMutable() const { return m_isMutable; }
    void setMutable() { m_isMutable = true; }

    bool isInMemoryCache() const { return m_isInMemoryCache; }
    void addedToMemoryCache();
    void removedFromMemoryCache();

private:
    StyleSheetInternal(StyleRuleImport* ownerRule, const String& originalURL, const KURL& baseURL, const CSSParserContext&);
    StyleSheetInternal(const StyleSheetInternal&);

    void clearCharsetRule();

    StyleRuleImport* m_ownerRule;

    String m_originalURL;
    KURL m_finalURL;

    String m_encodingFromCharsetRule;
    Vector<RefPtr<StyleRuleImport> > m_importRules;
    Vector<RefPtr<StyleRuleBase> > m_childRules;
    typedef HashMap<AtomicString, AtomicString> PrefixNamespaceURIMap;
    PrefixNamespaceURIMap m_namespaces;

    bool m_loadCompleted : 1;
    bool m_isUserStyleSheet : 1;
    bool m_hasSyntacticallyValidCSSHeader : 1;
    bool m_didLoadErrorOccur : 1;
    bool m_usesRemUnits : 1;
    bool m_isMutable : 1;
    bool m_isInMemoryCache : 1;
    
    CSSParserContext m_parserContext;

    Vector<CSSStyleSheet*> m_clients;
};

class CSSStyleSheet : public StyleSheet {
public:
    static RefPtr<CSSStyleSheet> create(PassRefPtr<StyleSheetInternal> sheet, CSSImportRule* ownerRule = 0) 
    { 
        return adoptRef(new CSSStyleSheet(sheet, ownerRule));
    }
    static RefPtr<CSSStyleSheet> create(PassRefPtr<StyleSheetInternal> sheet, Node* ownerNode)
    { 
        return adoptRef(new CSSStyleSheet(sheet, ownerNode));
    }
    static PassRefPtr<CSSStyleSheet> createInline(Node*, const KURL&, const String& encoding = String());

    virtual ~CSSStyleSheet();

    virtual CSSStyleSheet* parentStyleSheet() const OVERRIDE;
    virtual Node* ownerNode() const OVERRIDE { return m_ownerNode; }
    virtual MediaList* media() const OVERRIDE;
    virtual String href() const OVERRIDE { return m_internal->originalURL(); }  
    virtual String title() const OVERRIDE { return m_title; }
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

    virtual void clearOwnerNode() OVERRIDE { m_ownerNode = 0; }
    virtual CSSImportRule* ownerRule() const OVERRIDE { return m_ownerRule; }
    virtual KURL baseURL() const OVERRIDE { return m_internal->baseURL(); }
    virtual bool isLoading() const OVERRIDE { return m_internal->isLoading(); }
    
    void clearOwnerRule() { m_ownerRule = 0; }
    Document* ownerDocument() const;
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtr<MediaQuerySet>);
    void setTitle(const String& title) { m_title = title; }

    class RuleMutationScope {
        WTF_MAKE_NONCOPYABLE(RuleMutationScope);
    public:
        RuleMutationScope(CSSStyleSheet*);
        RuleMutationScope(CSSRule*);
        ~RuleMutationScope();

    private:
        CSSStyleSheet* m_styleSheet;
    };

    void willMutateRules();
    void didMutateRules();
    void didMutate();
    
    void clearChildRuleCSSOMWrappers();
    void reattachChildRuleCSSOMWrappers();

    StyleSheetInternal* internal() const { return m_internal.get(); }

private:
    CSSStyleSheet(PassRefPtr<StyleSheetInternal>, CSSImportRule* ownerRule);
    CSSStyleSheet(PassRefPtr<StyleSheetInternal>, Node* ownerNode);

    virtual bool isCSSStyleSheet() const { return true; }
    virtual String type() const { return "text/css"; }
    
    RefPtr<StyleSheetInternal> m_internal;
    bool m_isDisabled;
    String m_title;
    RefPtr<MediaQuerySet> m_mediaQueries;

    Node* m_ownerNode;
    CSSImportRule* m_ownerRule;

    mutable RefPtr<MediaList> m_mediaCSSOMWrapper;
    mutable Vector<RefPtr<CSSRule> > m_childRuleCSSOMWrappers;
    mutable OwnPtr<CSSRuleList> m_ruleListCSSOMWrapper;
};

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSStyleSheet* sheet)
    : m_styleSheet(sheet)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSRule* rule)
    : m_styleSheet(rule ? rule->parentStyleSheet() : 0)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::~RuleMutationScope()
{
    if (m_styleSheet)
        m_styleSheet->didMutateRules();
}

} // namespace

#endif
