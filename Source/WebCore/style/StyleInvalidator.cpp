/*
 * Copyright (C) 2012, 2014, 2017 Apple Inc. All rights reserved.
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
#include "StyleInvalidator.h"

#include "CSSSelectorList.h"
#include "Document.h"
#include "ElementIterator.h"
#include "ElementRuleCollector.h"
#include "HTMLSlotElement.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "SelectorFilter.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleRuleImport.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyleSheetContents.h"
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static bool shouldDirtyAllStyle(const Vector<RefPtr<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (is<StyleRuleMedia>(*rule)) {
            const auto* childRules = downcast<StyleRuleMedia>(*rule).childRulesWithoutDeferredParsing();
            if (childRules && shouldDirtyAllStyle(*childRules))
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

Invalidator::Invalidator(const Vector<StyleSheetContents*>& sheets, const MediaQueryEvaluator& mediaQueryEvaluator)
    : m_ownedRuleSet(RuleSet::create())
    , m_ruleSets({ m_ownedRuleSet })
    , m_dirtiesAllStyle(shouldDirtyAllStyle(sheets))
{
    if (m_dirtiesAllStyle)
        return;

    m_ownedRuleSet->disableAutoShrinkToFit();
    for (auto& sheet : sheets)
        m_ownedRuleSet->addRulesFromSheet(*sheet, mediaQueryEvaluator);

    m_ruleInformation = collectRuleInformation();
}

Invalidator::Invalidator(const InvalidationRuleSetVector& ruleSets)
    : m_ruleSets(ruleSets)
    , m_ruleInformation(collectRuleInformation())
{
    ASSERT(m_ruleSets.size());
}

Invalidator::~Invalidator() = default;

Invalidator::RuleInformation Invalidator::collectRuleInformation()
{
    RuleInformation information;
    for (auto& ruleSet : m_ruleSets) {
        if (!ruleSet->slottedPseudoElementRules().isEmpty())
            information.hasSlottedPseudoElementRules = true;
        if (!ruleSet->hostPseudoClassRules().isEmpty())
            information.hasHostPseudoClassRules = true;
        if (ruleSet->hasShadowPseudoElementRules())
            information.hasShadowPseudoElementRules = true;
        if (!ruleSet->partPseudoElementRules().isEmpty())
            information.hasPartPseudoElementRules = true;
    }
    return information;
}

Invalidator::CheckDescendants Invalidator::invalidateIfNeeded(Element& element, const SelectorFilter* filter)
{
    invalidateInShadowTreeIfNeeded(element);

    bool shouldCheckForSlots = m_ruleInformation.hasSlottedPseudoElementRules && !m_didInvalidateHostChildren;
    if (shouldCheckForSlots && is<HTMLSlotElement>(element)) {
        auto* containingShadowRoot = element.containingShadowRoot();
        if (containingShadowRoot && containingShadowRoot->host()) {
            for (auto& possiblySlotted : childrenOfType<Element>(*containingShadowRoot->host()))
                possiblySlotted.invalidateStyleInternal();
        }
        // No need to do this again.
        m_didInvalidateHostChildren = true;
    }

    switch (element.styleValidity()) {
    case Style::Validity::Valid: {
        for (auto& ruleSet : m_ruleSets) {
            ElementRuleCollector ruleCollector(element, *ruleSet, filter);
            ruleCollector.setMode(SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements);

            if (ruleCollector.matchesAnyAuthorRules()) {
                element.invalidateStyleInternal();
                break;
            }
        }

        return CheckDescendants::Yes;
    }
    case Style::Validity::ElementInvalid:
        return CheckDescendants::Yes;
    case Style::Validity::SubtreeInvalid:
    case Style::Validity::SubtreeAndRenderersInvalid:
        if (shouldCheckForSlots)
            return CheckDescendants::Yes;
        return CheckDescendants::No;
    }
    ASSERT_NOT_REACHED();
    return CheckDescendants::Yes;
}

void Invalidator::invalidateStyleForTree(Element& root, SelectorFilter* filter)
{
    if (invalidateIfNeeded(root, filter) == CheckDescendants::No)
        return;
    invalidateStyleForDescendants(root, filter);
}

void Invalidator::invalidateStyleForDescendants(Element& root, SelectorFilter* filter)
{
    Vector<Element*, 20> parentStack;
    Element* previousElement = &root;
    for (auto it = descendantsOfType<Element>(root).begin(); it; ) {
        auto& descendant = *it;
        auto* parent = descendant.parentElement();
        if (parentStack.isEmpty() || parentStack.last() != parent) {
            if (parent == previousElement) {
                parentStack.append(parent);
                if (filter)
                    filter->pushParentInitializingIfNeeded(*parent);
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

void Invalidator::invalidateStyle(Document& document)
{
    ASSERT(!m_dirtiesAllStyle);

    Element* documentElement = document.documentElement();
    if (!documentElement)
        return;

    SelectorFilter filter;
    invalidateStyleForTree(*documentElement, &filter);
}

void Invalidator::invalidateStyle(Scope& scope)
{
    if (m_dirtiesAllStyle) {
        invalidateAllStyle(scope);
        return;
    }

    if (auto* shadowRoot = scope.shadowRoot()) {
        invalidateStyle(*shadowRoot);
        return;
    }

    invalidateStyle(scope.document());
}

void Invalidator::invalidateStyle(ShadowRoot& shadowRoot)
{
    ASSERT(!m_dirtiesAllStyle);

    if (m_ruleInformation.hasHostPseudoClassRules && shadowRoot.host())
        shadowRoot.host()->invalidateStyleInternal();

    for (auto& child : childrenOfType<Element>(shadowRoot)) {
        SelectorFilter filter;
        invalidateStyleForTree(child, &filter);
    }
}

void Invalidator::invalidateStyle(Element& element)
{
    ASSERT(!m_dirtiesAllStyle);

    // Don't use SelectorFilter as the rule sets here tend to be small and the filter would have setup cost deep in the tree.
    invalidateStyleForTree(element, nullptr);
}

void Invalidator::invalidateStyleWithMatchElement(Element& element, MatchElement matchElement)
{
    switch (matchElement) {
    case MatchElement::Subject: {
        invalidateIfNeeded(element, nullptr);
        break;
    }
    case MatchElement::Parent: {
        auto children = childrenOfType<Element>(element);
        for (auto& child : children)
            invalidateIfNeeded(child, nullptr);
        break;
    }
    case MatchElement::Ancestor: {
        SelectorFilter filter;
        invalidateStyleForDescendants(element, &filter);
        break;
    }
    case MatchElement::DirectSibling:
        if (auto* sibling = element.nextElementSibling())
            invalidateIfNeeded(*sibling, nullptr);
        break;
    case MatchElement::IndirectSibling:
        for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling())
            invalidateIfNeeded(*sibling, nullptr);
        break;
    case MatchElement::AnySibling:
        for (auto& parentChild : childrenOfType<Element>(*element.parentNode()))
            invalidateIfNeeded(parentChild, nullptr);
        break;
    case MatchElement::ParentSibling:
        for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
            auto siblingChildren = childrenOfType<Element>(*sibling);
            for (auto& siblingChild : siblingChildren)
                invalidateIfNeeded(siblingChild, nullptr);
        }
        break;
    case MatchElement::AncestorSibling: {
        SelectorFilter filter;
        for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
            filter.popParentsUntil(element.parentElement());
            invalidateStyleForDescendants(*sibling, &filter);
        }
        break;
    }
    case MatchElement::Host:
        invalidateInShadowTreeIfNeeded(element);
        break;
    }
}

void Invalidator::invalidateShadowParts(ShadowRoot& shadowRoot)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().cssShadowPartsEnabled())
        return;

    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return;

    for (auto& descendant : descendantsOfType<Element>(shadowRoot)) {
        // FIXME: We could only invalidate part names that actually show up in rules.
        if (!descendant.partNames().isEmpty())
            descendant.invalidateStyleInternal();

        auto* nestedShadowRoot = descendant.shadowRoot();
        if (nestedShadowRoot && !nestedShadowRoot->partMappings().isEmpty())
            invalidateShadowParts(*nestedShadowRoot);
    }
}

void Invalidator::invalidateInShadowTreeIfNeeded(Element& element)
{
    auto* shadowRoot = element.shadowRoot();
    if (!shadowRoot)
        return;

    // FIXME: This could do actual rule matching too.
    if (m_ruleInformation.hasShadowPseudoElementRules)
        element.invalidateStyleForSubtreeInternal();

    // FIXME: More fine-grained invalidation for ::part()
    if (m_ruleInformation.hasPartPseudoElementRules)
        invalidateShadowParts(*shadowRoot);
}

void Invalidator::addToMatchElementRuleSets(Invalidator::MatchElementRuleSets& matchElementRuleSets, const InvalidationRuleSet& invalidationRuleSet)
{
    matchElementRuleSets.ensure(invalidationRuleSet.matchElement, [] {
        return InvalidationRuleSetVector { };
    }).iterator->value.append(invalidationRuleSet.ruleSet.copyRef());
}

void Invalidator::invalidateWithMatchElementRuleSets(Element& element, const MatchElementRuleSets& matchElementRuleSets)
{
    SetForScope<bool> isInvalidating(element.styleResolver().ruleSets().isInvalidatingStyleWithRuleSets(), true);

    for (auto& matchElementAndRuleSet : matchElementRuleSets) {
        Invalidator invalidator(matchElementAndRuleSet.value);
        invalidator.invalidateStyleWithMatchElement(element, matchElementAndRuleSet.key);
    }
}

void Invalidator::invalidateAllStyle(Scope& scope)
{
    if (auto* shadowRoot = scope.shadowRoot()) {
        for (auto& shadowChild : childrenOfType<Element>(*shadowRoot))
            shadowChild.invalidateStyleForSubtreeInternal();
        invalidateHostAndSlottedStyleIfNeeded(*shadowRoot);
        return;
    }

    scope.document().scheduleFullStyleRebuild();
}

void Invalidator::invalidateHostAndSlottedStyleIfNeeded(ShadowRoot& shadowRoot)
{
    auto& host = *shadowRoot.host();
    auto* resolver = shadowRoot.styleScope().resolverIfExists();
    if (!resolver)
        return;
    auto& authorStyle = resolver->ruleSets().authorStyle();

    if (!authorStyle.hostPseudoClassRules().isEmpty())
        host.invalidateStyleInternal();

    if (!authorStyle.slottedPseudoElementRules().isEmpty()) {
        for (auto& shadowChild : childrenOfType<Element>(host))
            shadowChild.invalidateStyleInternal();
    }
}

}
}
