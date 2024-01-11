// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSAtRuleID.h"
#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "StyleRule.h"
#include <memory>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserObserver;
class CSSParserObserverWrapper;
class CSSSelectorList;
class CSSTokenizer;
class StyleRuleCounterStyle;
class StyleRuleKeyframe;
class StyleRule;
class StyleRuleBase;
class StyleRuleCharset;
class StyleRuleContainer;
class StyleRuleFontFace;
class StyleRuleFontFeatureValues;
class StyleRuleFontFeatureValuesBlock;
class StyleRuleFontPaletteValues;
class StyleRuleImport;
class StyleRuleKeyframes;
class StyleRuleLayer;
class StyleRuleMedia;
class StyleRuleNamespace;
class StyleRulePage;
class StyleRuleSupports;
class StyleRuleViewport;
class StyleSheetContents;
class ImmutableStyleProperties;
class Element;
class MutableStyleProperties;
    
class CSSParserImpl {
    WTF_MAKE_NONCOPYABLE(CSSParserImpl);
public:
    CSSParserImpl(const CSSParserContext&, const String&, StyleSheetContents* = nullptr, CSSParserObserverWrapper* = nullptr, CSSParserEnum::IsNestedContext = CSSParserEnum::IsNestedContext::No);

    enum AllowedRulesType {
        // As per css-syntax, css-cascade and css-namespaces, @charset rules
        // must come first, followed by @import then @namespace.
        // AllowImportRules actually means we allow @import and any rules that
        // may follow it, i.e. @namespace rules and regular rules.
        // AllowCharsetRules and AllowNamespaceRules behave similarly.
        AllowCharsetRules,
        AllowLayerStatementRules,
        AllowImportRules,
        AllowNamespaceRules,
        RegularRules,
        KeyframeRules,
        CounterStyleRules,
        FontFeatureValuesRules,
        NoRules, // For parsing at-rules inside declaration lists (without nesting support)
    };

    static CSSParser::ParseResult parseValue(MutableStyleProperties*, CSSPropertyID, const String&, bool important, const CSSParserContext&);
    static CSSParser::ParseResult parseCustomPropertyValue(MutableStyleProperties*, const AtomString& propertyName, const String&, bool important, const CSSParserContext&);
    static Ref<ImmutableStyleProperties> parseInlineStyleDeclaration(const String&, const Element*);
    static bool parseDeclarationList(MutableStyleProperties*, const String&, const CSSParserContext&);
    static RefPtr<StyleRuleBase> parseRule(const String&, const CSSParserContext&, StyleSheetContents*, AllowedRulesType, CSSParserEnum::IsNestedContext = CSSParserEnum::IsNestedContext::No);
    static void parseStyleSheet(const String&, const CSSParserContext&, StyleSheetContents&);
    static CSSSelectorList parsePageSelector(CSSParserTokenRange, StyleSheetContents*);

    static Vector<double> parseKeyframeKeyList(const String&);

    bool supportsDeclaration(CSSParserTokenRange&);
    const CSSParserContext& context() const { return m_context; }

    // This function updates the range it's given.
    RefPtr<StyleRuleBase> consumeAtRule(CSSParserTokenRange&, AllowedRulesType);

    static void parseDeclarationListForInspector(const String&, const CSSParserContext&, CSSParserObserver&);
    static void parseStyleSheetForInspector(const String&, const CSSParserContext&, StyleSheetContents*, CSSParserObserver&);

    CSSTokenizer* tokenizer() const { return m_tokenizer.get(); };

private:
    struct NestingContext {
        // FIXME: Can we build StylePropertySets directly?
        // FIXME: Investigate using a smaller inline buffer
        ParsedPropertyVector m_parsedProperties;
        Vector<Ref<StyleRuleBase>> m_parsedRules;
    };
    CSSParserImpl(const CSSParserContext&, StyleSheetContents*);

    enum RuleListType {
        TopLevelRuleList,
        RegularRuleList,
        KeyframesRuleList,
        FontFeatureValuesRuleList,
    };

    // Returns whether the first encountered rule was valid
    template<typename T>
    bool consumeRuleList(CSSParserTokenRange, RuleListType, T callback);

    // This function updates the range it's given.
    RefPtr<StyleRuleBase> consumeQualifiedRule(CSSParserTokenRange&, AllowedRulesType);

    // This function is used for all the nested group rules (@media, @layer, @supports, @scope, @container,..etc)
    // It handles potentially orphaned declarations (in the context of style nesting)
    // https://drafts.csswg.org/css-nesting/#conditionals
    Vector<Ref<StyleRuleBase>> consumeNestedGroupRules(CSSParserTokenRange block);

    static RefPtr<StyleRuleCharset> consumeCharsetRule(CSSParserTokenRange prelude);
    RefPtr<StyleRuleImport> consumeImportRule(CSSParserTokenRange prelude);
    RefPtr<StyleRuleNamespace> consumeNamespaceRule(CSSParserTokenRange prelude);
    RefPtr<StyleRuleMedia> consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleSupports> consumeSupportsRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleViewport> consumeViewportRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleFontFace> consumeFontFaceRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleFontFeatureValuesBlock> consumeFontFeatureValuesRuleBlock(CSSAtRuleID, CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleFontFeatureValues> consumeFontFeatureValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleFontPaletteValues> consumeFontPaletteValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleKeyframes> consumeKeyframesRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRulePage> consumePageRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleCounterStyle> consumeCounterStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleLayer> consumeLayerRule(CSSParserTokenRange prelude, std::optional<CSSParserTokenRange> block);
    RefPtr<StyleRuleContainer> consumeContainerRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleProperty> consumePropertyRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleScope> consumeScopeRule(CSSParserTokenRange prelude, CSSParserTokenRange block);

    RefPtr<StyleRuleKeyframe> consumeKeyframeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    RefPtr<StyleRuleBase> consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block);
    ParsedPropertyVector consumeDeclarationListInNewNestingContext(CSSParserTokenRange, StyleRuleType);

    enum class OnlyDeclarations : bool { No, Yes };

    enum class ParsingStyleDeclarationsInRuleList : bool { No, Yes };

    // FIXME: We should return value for all those functions instead of using class member attributes.
    void consumeBlockContent(CSSParserTokenRange, StyleRuleType, OnlyDeclarations, ParsingStyleDeclarationsInRuleList = ParsingStyleDeclarationsInRuleList::No);
    void consumeDeclarationList(CSSParserTokenRange, StyleRuleType);
    void consumeStyleBlock(CSSParserTokenRange, StyleRuleType, ParsingStyleDeclarationsInRuleList = ParsingStyleDeclarationsInRuleList::No);
    bool consumeDeclaration(CSSParserTokenRange, StyleRuleType);
    void consumeDeclarationValue(CSSParserTokenRange, CSSPropertyID, bool important, StyleRuleType);
    void consumeCustomPropertyValue(CSSParserTokenRange, const AtomString& propertyName, bool important);

    static Vector<double> consumeKeyframeKeyList(CSSParserTokenRange);

    Ref<StyleRuleBase> createNestingParentRule();
    void runInNewNestingContext(auto&& run);
    NestingContext& topContext()
    {
        ASSERT(!m_nestingContextStack.isEmpty());
        return m_nestingContextStack.last();
    }

    bool isStyleNestedContext()
    {
        return (m_isAlwaysNestedContext == CSSParserEnum::IsNestedContext::Yes || m_styleRuleNestingLevel) && context().cssNestingEnabled;
    }

    bool isNestedContext()
    {
        return m_scopeRuleNestingLevel || isStyleNestedContext();
    }

    CSSParserEnum::IsNestedContext m_isAlwaysNestedContext { CSSParserEnum::IsNestedContext::No }; // Do we directly start in a nested context (for CSSOM)

    // FIXME: we could unify all those into a single stack data structure.
    // https://bugs.webkit.org/show_bug.cgi?id=265566
    unsigned m_styleRuleNestingLevel { 0 };
    unsigned m_scopeRuleNestingLevel { 0 };
    unsigned m_ruleListNestingLevel { 0 };
    enum class AncestorRuleType : bool {
        Style,
        Scope,
    };
    Vector<AncestorRuleType, 16> m_ancestorRuleTypeStack;
    static void appendImplicitSelectorIfNeeded(MutableCSSSelector&, AncestorRuleType);

    Vector<NestingContext> m_nestingContextStack { NestingContext { } };
    const CSSParserContext& m_context;

    RefPtr<StyleSheetContents> m_styleSheet;
    std::unique_ptr<CSSTokenizer> m_tokenizer;

    // For the inspector
    CSSParserObserverWrapper* m_observerWrapper { nullptr };
};

} // namespace WebCore
