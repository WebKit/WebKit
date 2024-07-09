/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "ElementRuleCollector.h"
#include "HTMLSlotElement.h"
#include "RuleSetBuilder.h"
#include "SelectorMatchingState.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleRuleImport.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyleSheetContents.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static bool shouldDirtyAllStyle(const Vector<Ref<StyleRuleBase>>& rules)
{
    for (auto& rule : rules) {
        if (auto* styleRuleMedia = dynamicDowncast<StyleRuleMedia>(rule.get())) {
            if (shouldDirtyAllStyle(styleRuleMedia->childRules()))
                return true;
            continue;
        }
        if (auto* styleRuleWithNesting = dynamicDowncast<StyleRuleWithNesting>(rule.get())) {
            if (shouldDirtyAllStyle(styleRuleWithNesting->nestedRules()))
                return true;
            continue;
        }
        // FIXME: At least font faces don't need full recalc in all cases.
        if (!is<StyleRule>(rule))
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

static bool shouldDirtyAllStyle(const Vector<Ref<StyleSheetContents>>& sheets)
{
    for (auto& sheet : sheets) {
        if (shouldDirtyAllStyle(sheet))
            return true;
    }
    return false;
}

Invalidator::Invalidator(const Vector<Ref<StyleSheetContents>>& sheets, const MQ::MediaQueryEvaluator& mediaQueryEvaluator)
    : m_ownedRuleSet(RuleSet::create())
    , m_ruleSets({ m_ownedRuleSet })
    , m_dirtiesAllStyle(shouldDirtyAllStyle(sheets))
{
    if (m_dirtiesAllStyle)
        return;

    RuleSetBuilder ruleSetBuilder(*m_ownedRuleSet, mediaQueryEvaluator, nullptr, RuleSetBuilder::ShrinkToFit::Disable);

    for (auto& sheet : sheets)
        ruleSetBuilder.addRulesFromSheet(sheet);

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
        if (ruleSet->hasHostPseudoClassRulesMatchingInShadowTree())
            information.hasHostPseudoClassRulesMatchingInShadowTree = true;
        if (ruleSet->hasUserAgentPartRules())
            information.hasUserAgentPartRules = true;
#if ENABLE(VIDEO)
        if (!ruleSet->cuePseudoRules().isEmpty())
            information.hasCuePseudoElementRules = true;
#endif
        if (!ruleSet->partPseudoElementRules().isEmpty())
            information.hasPartPseudoElementRules = true;
    }
    return information;
}

static void invalidateAssignedElements(HTMLSlotElement& slot)
{
    auto* assignedNodes = slot.assignedNodes();
    if (!assignedNodes)
        return;
    for (auto& node : *assignedNodes) {
        auto* element = dynamicDowncast<Element>(node.get());
        if (!element)
            continue;
        if (auto* slotElement = dynamicDowncast<HTMLSlotElement>(*element); slotElement && node->containingShadowRoot()) {
            invalidateAssignedElements(*slotElement);
            continue;
        }
        element->invalidateStyleInternal();
    }
}

Invalidator::CheckDescendants Invalidator::invalidateIfNeeded(Element& element, SelectorMatchingState* selectorMatchingState)
{
    invalidateInShadowTreeIfNeeded(element);

    if (m_ruleInformation.hasSlottedPseudoElementRules) {
        if (auto* slotElement = dynamicDowncast<HTMLSlotElement>(element))
            invalidateAssignedElements(*slotElement);
    }

    switch (element.styleValidity()) {
    case Validity::Valid:
    case Validity::AnimationInvalid:
    case Validity::InlineStyleInvalid: {
        for (auto& ruleSet : m_ruleSets) {
            ElementRuleCollector ruleCollector(element, *ruleSet, selectorMatchingState);
            ruleCollector.setMode(SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements);

            if (ruleCollector.matchesAnyAuthorRules()) {
                element.invalidateStyleInternal();
                break;
            }
        }

        return CheckDescendants::Yes;
    }
    case Validity::ElementInvalid:
        return CheckDescendants::Yes;
    case Validity::SubtreeInvalid:
        return CheckDescendants::No;
    }
    ASSERT_NOT_REACHED();
    return CheckDescendants::Yes;
}

void Invalidator::invalidateStyleForTree(Element& root, SelectorMatchingState* selectorMatchingState)
{
    if (invalidateIfNeeded(root, selectorMatchingState) == CheckDescendants::No)
        return;
    invalidateStyleForDescendants(root, selectorMatchingState);
}

void Invalidator::invalidateStyleForDescendants(Element& root, SelectorMatchingState* selectorMatchingState)
{
    Vector<Element*, 20> parentStack;
    Element* previousElement = &root;
    for (auto it = descendantsOfType<Element>(root).begin(); it; ) {
        auto& descendant = *it;
        auto* parent = descendant.parentElement();
        if (parentStack.isEmpty() || parentStack.last() != parent) {
            if (parent == previousElement) {
                parentStack.append(parent);
                if (selectorMatchingState)
                    selectorMatchingState->selectorFilter.pushParentInitializingIfNeeded(*parent);
            } else {
                while (parentStack.last() != parent) {
                    parentStack.removeLast();
                    if (selectorMatchingState)
                        selectorMatchingState->selectorFilter.popParent();
                }
            }
        }
        previousElement = &descendant;

        if (invalidateIfNeeded(descendant, selectorMatchingState) == CheckDescendants::Yes)
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

    SelectorMatchingState selectorMatchingState;
    invalidateStyleForTree(*documentElement, &selectorMatchingState);
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
        SelectorMatchingState selectorMatchingState;
        invalidateStyleForTree(child, &selectorMatchingState);
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
        SelectorMatchingState selectorMatchingState;
        invalidateStyleForDescendants(element, &selectorMatchingState);
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
        SelectorMatchingState selectorMatchingState;
        for (auto* sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
            selectorMatchingState.selectorFilter.popParentsUntil(element.parentElement());
            invalidateStyleForDescendants(*sibling, &selectorMatchingState);
        }
        break;
    }
    case MatchElement::ParentAnySibling:
        for (auto& sibling : childrenOfType<Element>(*element.parentNode())) {
            auto siblingChildren = childrenOfType<Element>(sibling);
            for (auto& siblingChild : siblingChildren)
                invalidateIfNeeded(siblingChild, nullptr);
        }
        break;
    case MatchElement::AncestorAnySibling: {
        SelectorMatchingState selectorMatchingState;
        for (auto& sibling : childrenOfType<Element>(*element.parentNode())) {
            selectorMatchingState.selectorFilter.popParentsUntil(element.parentElement());
            invalidateStyleForDescendants(sibling, &selectorMatchingState);
        }
        break;
    }
    case MatchElement::HasChild: {
        if (auto* parent = element.parentElement())
            invalidateIfNeeded(*parent, nullptr);
        break;
    }
    case MatchElement::HasDescendant: {
        Vector<Element*, 16> ancestors;
        for (auto* parent = element.parentElement(); parent; parent = parent->parentElement())
            ancestors.append(parent);

        SelectorMatchingState selectorMatchingState;
        selectorMatchingState.selectorFilter.parentStackReserveInitialCapacity(ancestors.size());
        for (auto* ancestor : makeReversedRange(ancestors)) {
            invalidateIfNeeded(*ancestor, &selectorMatchingState);
            selectorMatchingState.selectorFilter.pushParent(ancestor);
        }
        break;
    }
    case MatchElement::HasSibling:
        if (auto* sibling = element.previousElementSibling()) {
            SelectorMatchingState selectorMatchingState;
            if (RefPtr parent = element.parentElement())
                selectorMatchingState.selectorFilter.pushParentInitializingIfNeeded(*parent);

            for (; sibling; sibling = sibling->previousElementSibling())
                invalidateIfNeeded(*sibling, &selectorMatchingState);
        }
        break;
    case MatchElement::HasAnySibling: {
        SelectorMatchingState selectorMatchingState;
        if (auto* parent = element.parentElement())
            selectorMatchingState.selectorFilter.pushParentInitializingIfNeeded(*parent);
        for (auto& sibling : childrenOfType<Element>(*element.parentNode()))
            invalidateIfNeeded(sibling, &selectorMatchingState);
        break;
    }
    case MatchElement::HasSiblingDescendant: {
        Vector<Element*, 16> elementAndAncestors;
        elementAndAncestors.append(&element);
        for (auto* parent = element.parentElement(); parent; parent = parent->parentElement())
            elementAndAncestors.append(parent);

        SelectorMatchingState selectorMatchingState;
        selectorMatchingState.selectorFilter.parentStackReserveInitialCapacity(elementAndAncestors.size());
        for (auto* elementOrAncestor : makeReversedRange(elementAndAncestors)) {
            for (auto* sibling = elementOrAncestor->previousElementSibling(); sibling; sibling = sibling->previousElementSibling())
                invalidateIfNeeded(*sibling, &selectorMatchingState);

            selectorMatchingState.selectorFilter.pushParent(elementOrAncestor);
        }
        break;
    }
    case MatchElement::HasNonSubject:
    case MatchElement::HasScopeBreaking: {
        SelectorMatchingState selectorMatchingState;
        invalidateStyleForDescendants(*element.document().documentElement(), &selectorMatchingState);
        break;
    }
    case MatchElement::Host:
        invalidateInShadowTreeIfNeeded(element);
        break;
    case MatchElement::HostChild:
        if (auto* host = element.shadowHost()) {
            for (auto& hostChild : childrenOfType<Element>(*host))
                invalidateIfNeeded(hostChild, nullptr);
        }
        break;
    }
}

void Invalidator::invalidateShadowParts(ShadowRoot& shadowRoot)
{
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

void Invalidator::invalidateUserAgentParts(ShadowRoot& shadowRoot)
{
    if (shadowRoot.mode() != ShadowRootMode::UserAgent)
        return;

    for (auto& descendant : descendantsOfType<Element>(shadowRoot)) {
        auto& part = descendant.userAgentPart();
        if (!part)
            continue;
        for (auto& ruleSet : m_ruleSets) {
            if (ruleSet->userAgentPartRules(part))
                descendant.invalidateStyleInternal();
        }
    }
}

void Invalidator::invalidateInShadowTreeIfNeeded(Element& element)
{
    auto* shadowRoot = element.shadowRoot();
    if (!shadowRoot)
        return;

    if (m_ruleInformation.hasUserAgentPartRules)
        invalidateUserAgentParts(*shadowRoot);

    if (m_ruleInformation.hasHostPseudoClassRulesMatchingInShadowTree) {
        for (auto& child : childrenOfType<Element>(*shadowRoot)) {
            SelectorMatchingState selectorMatchingState;
            invalidateStyleForTree(child, &selectorMatchingState);
        }
    }

#if ENABLE(VIDEO)
    if (m_ruleInformation.hasCuePseudoElementRules && element.isMediaElement())
        element.invalidateStyleForSubtreeInternal();
#endif

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
    SetForScope isInvalidating(element.styleResolver().ruleSets().isInvalidatingStyleWithRuleSets(), true);

    for (auto& matchElementAndRuleSet : matchElementRuleSets) {
        Invalidator invalidator(matchElementAndRuleSet.value);
        invalidator.invalidateStyleWithMatchElement(element, matchElementAndRuleSet.key);
    }
}

void Invalidator::invalidateWithScopeBreakingHasPseudoClassRuleSet(Element& element, const RuleSet* ruleSet)
{
    SetForScope isInvalidating(element.styleResolver().ruleSets().isInvalidatingStyleWithRuleSets(), true);
    Invalidator invalidator({ ruleSet });
    invalidator.invalidateStyleWithMatchElement(element, MatchElement::HasScopeBreaking);
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

    if (!resolver || resolver->ruleSets().hasMatchingUserOrAuthorStyle([] (auto& style) { return !style.hostPseudoClassRules().isEmpty(); }))
        host.invalidateStyleInternal();

    if (!resolver || resolver->ruleSets().hasMatchingUserOrAuthorStyle([] (auto& style) { return !style.slottedPseudoElementRules().isEmpty(); })) {
        for (auto& shadowChild : childrenOfType<Element>(host))
            shadowChild.invalidateStyleInternal();
    }
}

}
}
