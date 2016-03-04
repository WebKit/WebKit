/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

class CSSCharsetRule;
class CSSImportRule;
class CSSParser;
class CSSRule;
class CSSRuleList;
class CSSStyleSheet;
class CachedCSSStyleSheet;
class Document;
class MediaQuerySet;
class SecurityOrigin;
class StyleRuleKeyframes;
class StyleSheetContents;

typedef int ExceptionCode;

class CSSStyleSheet final : public StyleSheet {
public:
    static Ref<CSSStyleSheet> create(Ref<StyleSheetContents>&&, CSSImportRule* ownerRule = 0);
    static Ref<CSSStyleSheet> create(Ref<StyleSheetContents>&&, Node* ownerNode);
    static Ref<CSSStyleSheet> createInline(Node&, const URL&, const TextPosition& startPosition = TextPosition::minimumPosition(), const String& encoding = String());

    virtual ~CSSStyleSheet();

    CSSStyleSheet* parentStyleSheet() const override;
    Node* ownerNode() const override { return m_ownerNode; }
    MediaList* media() const override;
    String href() const override;
    String title() const override { return m_title; }
    bool disabled() const override { return m_isDisabled; }
    void setDisabled(bool) override;
    
    RefPtr<CSSRuleList> cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);
    
    // IE Extensions
    RefPtr<CSSRuleList> rules();
    int addRule(const String& selector, const String& style, int index, ExceptionCode&);
    int addRule(const String& selector, const String& style, ExceptionCode&);
    void removeRule(unsigned index, ExceptionCode& ec) { deleteRule(index, ec); }
    
    // For CSSRuleList.
    unsigned length() const;
    CSSRule* item(unsigned index);

    void clearOwnerNode() override;
    CSSImportRule* ownerRule() const override { return m_ownerRule; }
    URL baseURL() const override;
    bool isLoading() const override;
    
    void clearOwnerRule() { m_ownerRule = 0; }
    Document* ownerDocument() const;
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtr<MediaQuerySet>);
    void setTitle(const String& title) { m_title = title; }

    bool hadRulesMutation() const { return m_mutatedRules; }
    void clearHadRulesMutation() { m_mutatedRules = false; }

    enum RuleMutationType { OtherMutation, RuleInsertion };
    enum WhetherContentsWereClonedForMutation { ContentsWereNotClonedForMutation = 0, ContentsWereClonedForMutation };

    class RuleMutationScope {
        WTF_MAKE_NONCOPYABLE(RuleMutationScope);
    public:
        RuleMutationScope(CSSStyleSheet*, RuleMutationType = OtherMutation, StyleRuleKeyframes* insertedKeyframesRule = nullptr);
        RuleMutationScope(CSSRule*);
        ~RuleMutationScope();

    private:
        CSSStyleSheet* m_styleSheet;
        RuleMutationType m_mutationType;
        WhetherContentsWereClonedForMutation m_contentsWereClonedForMutation;
        StyleRuleKeyframes* m_insertedKeyframesRule;
    };

    WhetherContentsWereClonedForMutation willMutateRules();
    void didMutateRules(RuleMutationType, WhetherContentsWereClonedForMutation, StyleRuleKeyframes* insertedKeyframesRule);
    void didMutateRuleFromCSSStyleDeclaration();
    void didMutate();
    
    void clearChildRuleCSSOMWrappers();
    void reattachChildRuleCSSOMWrappers();

    StyleSheetContents& contents() { return m_contents; }

    bool isInline() const { return m_isInlineStylesheet; }
    TextPosition startPosition() const { return m_startPosition; }

    void detachFromDocument() { m_ownerNode = nullptr; }

private:
    CSSStyleSheet(Ref<StyleSheetContents>&&, CSSImportRule* ownerRule);
    CSSStyleSheet(Ref<StyleSheetContents>&&, Node* ownerNode, const TextPosition& startPosition, bool isInlineStylesheet);

    bool isCSSStyleSheet() const override { return true; }
    String type() const override { return ASCIILiteral("text/css"); }

    bool canAccessRules() const;
    
    Ref<StyleSheetContents> m_contents;
    bool m_isInlineStylesheet;
    bool m_isDisabled;
    bool m_mutatedRules;
    String m_title;
    RefPtr<MediaQuerySet> m_mediaQueries;

    Node* m_ownerNode;
    CSSImportRule* m_ownerRule;

    TextPosition m_startPosition;

    mutable RefPtr<MediaList> m_mediaCSSOMWrapper;
    mutable Vector<RefPtr<CSSRule>> m_childRuleCSSOMWrappers;
    mutable std::unique_ptr<CSSRuleList> m_ruleListCSSOMWrapper;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSStyleSheet)
    static bool isType(const WebCore::StyleSheet& styleSheet) { return styleSheet.isCSSStyleSheet(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // CSSStyleSheet_h
