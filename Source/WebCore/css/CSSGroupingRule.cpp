/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSGroupingRule.h"

#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSGroupingRule::CSSGroupingRule(StyleRuleGroup& groupRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_groupRule(groupRule)
    , m_childRuleCSSOMWrappers(groupRule.childRules().size())
{
}

CSSGroupingRule::~CSSGroupingRule()
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    for (auto& wrapper : m_childRuleCSSOMWrappers) {
        if (wrapper)
            wrapper->setParentRule(nullptr);
    }
}

ExceptionOr<unsigned> CSSGroupingRule::insertRule(const String& ruleString, unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index > m_groupRule->childRules().size()) {
        // IndexSizeError: Raised if the specified index is not a valid insertion point.
        return Exception { IndexSizeError };
    }

    CSSStyleSheet* styleSheet = parentStyleSheet();
    auto isNestedContext = hasStyleRuleAncestor() ? CSSParserEnum::IsNestedContext::Yes : CSSParserEnum::IsNestedContext::No;
    RefPtr<StyleRuleBase> newRule = CSSParser::parseRule(parserContext(), styleSheet ? &styleSheet->contents() : nullptr, ruleString, isNestedContext);
    if (!newRule) {
        // SyntaxError: Raised if the specified rule has a syntax error and is unparsable.
        return Exception { SyntaxError };
    }

    if (newRule->isImportRule() || newRule->isNamespaceRule()) {
        // FIXME: an HierarchyRequestError should also be thrown for a @charset or a nested
        // @media rule. They are currently not getting parsed, resulting in a SyntaxError
        // to get raised above.

        // HierarchyRequestError: Raised if the rule cannot be inserted at the specified
        // index, e.g., if an @import rule is inserted after a standard rule set or other
        // at-rule.
        return Exception { HierarchyRequestError };
    }

    // Nesting inside style rule only accepts style rule or group rule
    if (hasStyleRuleAncestor() && !newRule->isStyleRule() && !newRule->isGroupRule())
        return Exception { HierarchyRequestError };

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperInsertRule(index, newRule.releaseNonNull());

    m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());
    return index;
}

ExceptionOr<void> CSSGroupingRule::deleteRule(unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index >= m_groupRule->childRules().size()) {
        // IndexSizeError: Raised if the specified index does not correspond to a
        // rule in the media rule list.
        return Exception { IndexSizeError };
    }

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperRemoveRule(index);

    if (m_childRuleCSSOMWrappers[index])
        m_childRuleCSSOMWrappers[index]->setParentRule(nullptr);
    m_childRuleCSSOMWrappers.remove(index);

    return { };
}

void CSSGroupingRule::appendCSSTextForItems(StringBuilder& builder) const
{
    builder.append(" {");

    StringBuilder decls;
    StringBuilder rules;
    cssTextForDeclsAndRules(decls, rules);

    if (decls.isEmpty() && rules.isEmpty()) {
        builder.append("\n}");
        return;
    }

    if (rules.isEmpty()) {
        builder.append(' ', static_cast<StringView>(decls), " }");
        return;
    }
    
    if (decls.isEmpty()) {
        builder.append(static_cast<StringView>(rules), "\n}");
        return;
    }

    builder.append('\n', "  ", static_cast<StringView>(decls), static_cast<StringView>(rules), "\n}");
    return;
}

void CSSGroupingRule::cssTextForDeclsAndRules(StringBuilder& decls, StringBuilder& rules) const
{
    auto& childRules = m_groupRule->childRules();
    for (unsigned index = 0 ; index < childRules.size() ; index++) {
        // We put the declarations at the upper level when the rule:
        // - is the first rule
        // - has just "&" as original selector
        // - has no child rules
        if (!index) {
            // It's the first rule.
            auto childRule = childRules[index];
            if (childRule->isStyleRuleWithNesting()) {
                auto& nestedStyleRule = downcast<StyleRuleWithNesting>(childRule);
                if (nestedStyleRule.originalSelectorList().hasOnlyNestingSelector() && nestedStyleRule.nestedRules().isEmpty()) {
                    decls.append(nestedStyleRule.properties().asText());
                    continue;
                }
            }
        }
        // Otherwise we print the child rule
        auto wrappedRule = item(index);
        rules.append("\n  ", wrappedRule->cssText());
    }
}

RefPtr<StyleRuleWithNesting> CSSGroupingRule::prepareChildStyleRuleForNesting(StyleRule& styleRule)
{
    CSSStyleSheet::RuleMutationScope scope(this);
    auto& rules = m_groupRule->m_childRules;
    for (size_t i = 0 ; i < rules.size() ; i++) {
        auto& rule = rules[i];
        if (rule.ptr() == &styleRule) {
            auto styleRuleWithNesting = StyleRuleWithNesting::create(WTFMove(styleRule));
            rules[i] = styleRuleWithNesting;
            if (auto* styleSheet = parentStyleSheet())
                styleSheet->contents().setHasNestingRules();
            return styleRuleWithNesting;
        }        
    }
    return { };
}

unsigned CSSGroupingRule::length() const
{ 
    return m_groupRule->childRules().size(); 
}

CSSRule* CSSGroupingRule::item(unsigned index) const
{ 
    if (index >= length())
        return nullptr;
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    auto& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = m_groupRule->childRules()[index]->createCSSOMWrapper(const_cast<CSSGroupingRule&>(*this));
    return rule.get();
}

CSSRuleList& CSSGroupingRule::cssRules() const
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = makeUnique<LiveCSSRuleList<CSSGroupingRule>>(const_cast<CSSGroupingRule&>(*this));
    return *m_ruleListCSSOMWrapper;
}

void CSSGroupingRule::reattach(StyleRuleBase& rule)
{
    m_groupRule = downcast<StyleRuleGroup>(rule);
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->reattach(m_groupRule->childRules()[i]);
    }
}

} // namespace WebCore
