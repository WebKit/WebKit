/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentRule(0);
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
    RefPtr<StyleRuleBase> newRule = CSSParser::parseRule(parserContext(), styleSheet ? &styleSheet->contents() : nullptr, ruleString);
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
        m_childRuleCSSOMWrappers[index]->setParentRule(0);
    m_childRuleCSSOMWrappers.remove(index);

    return { };
}

void CSSGroupingRule::appendCssTextForItems(StringBuilder& result) const
{
    unsigned size = length();
    for (unsigned i = 0; i < size; ++i) {
        result.appendLiteral("  ");
        result.append(item(i)->cssText());
        result.append('\n');
    }
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
    RefPtr<CSSRule>& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = m_groupRule->childRules()[index]->createCSSOMWrapper(const_cast<CSSGroupingRule*>(this));
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
    m_groupRule = static_cast<StyleRuleGroup&>(rule);
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->reattach(*m_groupRule.get().childRules()[i]);
    }
}

} // namespace WebCore
