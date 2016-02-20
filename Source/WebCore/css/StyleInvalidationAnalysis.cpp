/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleInvalidationAnalysis.h"

#include "CSSSelectorList.h"
#include "Document.h"
#include "ElementIterator.h"
#include "ElementRuleCollector.h"
#include "SelectorFilter.h"
#include "ShadowRoot.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

namespace WebCore {

static bool shouldDirtyAllStyle(const Vector<RefPtr<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (is<StyleRuleMedia>(*rule)) {
            if (shouldDirtyAllStyle(downcast<StyleRuleMedia>(*rule).childRules()))
                return true;
            continue;
        }
        // FIXME: At least font faces don't need full recalc in all cases.
        if (!is<StyleRule>(*rule))
            return true;
    }
    return false;
}

static bool shouldDirtyAllStyle(const StyleSheetContents& sheet)
{
    for (auto& import : sheet.importRules()) {
        if (!import->styleSheet())
            continue;
        if (shouldDirtyAllStyle(*import->styleSheet()))
            return true;
    }
    if (shouldDirtyAllStyle(sheet.childRules()))
        return true;
    return false;
}

static bool shouldDirtyAllStyle(const Vector<StyleSheetContents*>& sheets)
{
    for (auto& sheet : sheets) {
        if (shouldDirtyAllStyle(*sheet))
            return true;
    }
    return false;
}

StyleInvalidationAnalysis::StyleInvalidationAnalysis(const Vector<StyleSheetContents*>& sheets, const MediaQueryEvaluator& mediaQueryEvaluator)
    : m_ownedRuleSet(std::make_unique<RuleSet>())
    , m_ruleSet(*m_ownedRuleSet)
    , m_dirtiesAllStyle(shouldDirtyAllStyle(sheets))
{
    if (m_dirtiesAllStyle)
        return;

    m_ownedRuleSet->disableAutoShrinkToFit();
    for (auto& sheet : sheets)
        m_ownedRuleSet->addRulesFromSheet(*sheet, mediaQueryEvaluator);

    m_hasShadowPseudoElementRulesInAuthorSheet = m_ruleSet.hasShadowPseudoElementRules();
}

StyleInvalidationAnalysis::StyleInvalidationAnalysis(const RuleSet& ruleSet)
    : m_ruleSet(ruleSet)
    , m_hasShadowPseudoElementRulesInAuthorSheet(ruleSet.hasShadowPseudoElementRules())
{
}

StyleInvalidationAnalysis::CheckDescendants StyleInvalidationAnalysis::invalidateIfNeeded(Element& element, const SelectorFilter* filter)
{
    if (m_hasShadowPseudoElementRulesInAuthorSheet) {
        // FIXME: This could do actual rule matching too.
        if (element.shadowRoot())
            element.setNeedsStyleRecalc();
    }

    switch (element.styleChangeType()) {
    case NoStyleChange: {
        ElementRuleCollector ruleCollector(element, m_ruleSet, filter);
        ruleCollector.setMode(SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements);
        ruleCollector.matchAuthorRules(false);

        if (ruleCollector.hasMatchedRules())
            element.setNeedsStyleRecalc(InlineStyleChange);
        return CheckDescendants::Yes;
    }
    case InlineStyleChange:
        return CheckDescendants::Yes;
    case FullStyleChange:
    case SyntheticStyleChange:
    case ReconstructRenderTree:
        return CheckDescendants::No;
    }
    ASSERT_NOT_REACHED();
    return CheckDescendants::Yes;
}

void StyleInvalidationAnalysis::invalidateStyleForTree(Element& root, SelectorFilter* filter)
{
    if (invalidateIfNeeded(root, filter) == CheckDescendants::No)
        return;

    Vector<Element*, 20> parentStack;
    Element* previousElement = &root;
    auto descendants = descendantsOfType<Element>(root);
    for (auto it = descendants.begin(), end = descendants.end(); it != end;) {
        auto& descendant = *it;
        auto* parent = descendant.parentElement();
        if (parentStack.isEmpty() || parentStack.last() != parent) {
            if (parent == previousElement) {
                parentStack.append(parent);
                if (filter)
                    filter->pushParent(parent);
            } else {
                while (parentStack.last() != parent) {
                    parentStack.removeLast();
                    if (filter)
                        filter->popParent();
                }
            }
        }
        previousElement = &descendant;

        if (invalidateIfNeeded(descendant, filter) == CheckDescendants::Yes)
            it.traverseNext();
        else
            it.traverseNextSkippingChildren();
    }
}

void StyleInvalidationAnalysis::invalidateStyle(Document& document)
{
    ASSERT(!m_dirtiesAllStyle);

    Element* documentElement = document.documentElement();
    if (!documentElement)
        return;

    SelectorFilter filter;
    invalidateStyleForTree(*documentElement, &filter);
}

void StyleInvalidationAnalysis::invalidateStyle(Element& element)
{
    ASSERT(!m_dirtiesAllStyle);

    // Don't use SelectorFilter as the rule sets here tend to be small and the filter would have setup cost deep in the tree.
    invalidateStyleForTree(element, nullptr);
}

}
