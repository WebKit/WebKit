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

#include "Document.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "ElementRuleCollector.h"
#include "HTMLSlotElement.h"
#include "RuleSetBuilder.h"
#include "SelectorChecker.h"
#include "SelectorMatchingState.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleRuleImport.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyleSheetContents.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <ranges>
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static bool NODELETE shouldDirtyAllStyle(const Vector<Ref<StyleRuleBase>>& rules)
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

static bool NODELETE shouldDirtyAllStyle(const StyleSheetContents& sheet)
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

static bool NODELETE shouldDirtyAllStyle(const Vector<Ref<StyleSheetContents>>& sheets)
{
    for (auto& sheet : sheets) {
        if (shouldDirtyAllStyle(sheet))
            return true;
    }
    return false;
}

Invalidator::Invalidator(const Vector<Ref<StyleSheetContents>>& sheets, const MQ::MediaQueryEvaluator& mediaQueryEvaluator)
    : m_ownedRuleSet(RuleSet::create())
    , m_ruleSets({ { m_ownedRuleSet } })
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
        if (!ruleSet.ruleSet->slottedPseudoElementRules().isEmpty())
            information.hasSlottedPseudoElementRules = true;
        if (!ruleSet.ruleSet->hostPseudoClassRules().isEmpty())
            information.hasHostPseudoClassRules = true;
        if (ruleSet.ruleSet->hasHostPseudoClassRulesMatchingInShadowTree())
            information.hasHostPseudoClassRulesMatchingInShadowTree = true;
        if (ruleSet.ruleSet->hasUserAgentPartRules())
            information.hasUserAgentPartRules = true;
#if ENABLE(VIDEO)
        if (!ruleSet.ruleSet->cuePseudoRules().isEmpty())
            information.hasCuePseudoElementRules = true;
#endif
        if (!ruleSet.ruleSet->partPseudoElementRules().isEmpty())
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
        RefPtr element = dynamicDowncast<Element>(node.get());
        if (!element)
            continue;
        if (RefPtr slotElement = dynamicDowncast<HTMLSlotElement>(*element); slotElement && node->containingShadowRoot()) {
            invalidateAssignedElements(*slotElement);
            continue;
        }
        element->invalidateStyleInternal();
        // Invalidate ::slotted nested pseudo-elements.
        if (RefPtr shadowRoot = element->userAgentShadowRoot()) {
            for (Ref descendant : descendantsOfType<Element>(*shadowRoot)) {
                if (!descendant->userAgentPart().isEmpty())
                    descendant->invalidateStyleInternal();
            }
        }
    }
}

Invalidator::CheckDescendants Invalidator::invalidateIfNeeded(Element& element, SelectorMatchingState* selectorMatchingState)
{
    ++m_elementTraversalCount;
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
            ElementRuleCollector ruleCollector(element, *ruleSet.ruleSet, selectorMatchingState, SelectorChecker::Mode::StyleInvalidation);

            auto matches = ruleCollector.matchesAnyAuthorRules();
            if (ruleSet.isNegation == IsNegation::No ? matches : !matches) {
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
    RefPtr previousElement = &root;
    for (auto it = descendantsOfType<Element>(root).begin(); it; ) {
        Ref descendant = *it;
        RefPtr parent = descendant->parentElement();
        if (parentStack.isEmpty() || parentStack.last() != parent.get()) {
            if (parent.get() == previousElement.get()) {
                parentStack.append(parent.get());
                if (selectorMatchingState)
                    selectorMatchingState->selectorFilter.pushParentInitializingIfNeeded(*parent);
            } else {
                while (parentStack.last() != parent.get()) {
                    parentStack.removeLast();
                    if (selectorMatchingState)
                        selectorMatchingState->selectorFilter.popParent();
                }
            }
        }
        previousElement = descendant.ptr();

        if (invalidateIfNeeded(descendant.get(), selectorMatchingState) == CheckDescendants::Yes)
            it.traverseNext();
        else
            it.traverseNextSkippingChildren();
    }
}

void Invalidator::invalidateStyle(Document& document)
{
    ASSERT(!m_dirtiesAllStyle);

    RefPtr documentElement = document.documentElement();
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

    if (RefPtr shadowRoot = scope.shadowRoot()) {
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

    for (Ref child : childrenOfType<Element>(shadowRoot)) {
        SelectorMatchingState selectorMatchingState;
        invalidateStyleForTree(child.get(), &selectorMatchingState);
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
    using Relation = MatchElement::Relation;
    using HasRelation = MatchElement::HasRelation;

    if (!matchElement.hasRelation) {
        switch (matchElement.relation) {
        case Relation::Subject:
            // .changed
            invalidateIfNeeded(element, nullptr);
            break;
        case Relation::Parent:
            // .changed > .subject
            for (Ref child : childrenOfType<Element>(element))
                invalidateIfNeeded(child.get(), nullptr);
            break;
        case Relation::Ancestor: {
            // .changed .subject
            SelectorMatchingState selectorMatchingState;
            invalidateStyleForDescendants(element, &selectorMatchingState);
            break;
        }
        case Relation::DirectSibling:
            // .changed + .subject
            if (RefPtr sibling = element.nextElementSibling())
                invalidateIfNeeded(*sibling, nullptr);
            break;
        case Relation::IndirectSibling:
            // .changed ~ .subject
            for (RefPtr sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling())
                invalidateIfNeeded(*sibling, nullptr);
            break;
        case Relation::AnySibling:
            // :nth-last-child(even of .changed)
            for (Ref parentChild : childrenOfType<Element>(*element.parentNode()))
                invalidateIfNeeded(parentChild.get(), nullptr);
            break;
        case Relation::ParentSibling:
            // .changed ~ .a > .subject
            for (RefPtr sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
                for (Ref siblingChild : childrenOfType<Element>(*sibling))
                    invalidateIfNeeded(siblingChild.get(), nullptr);
            }
            break;
        case Relation::AncestorSibling: {
            // .changed ~ .a .subject
            SelectorMatchingState selectorMatchingState;
            for (RefPtr sibling = element.nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
                selectorMatchingState.selectorFilter.popParentsUntil(element.parentElement());
                invalidateStyleForDescendants(*sibling, &selectorMatchingState);
            }
            break;
        }
        case Relation::ParentAnySibling:
            // :nth-last-child(even of .changed) > .subject
            for (Ref sibling : childrenOfType<Element>(*element.parentNode())) {
                for (Ref siblingChild : childrenOfType<Element>(sibling.get()))
                    invalidateIfNeeded(siblingChild.get(), nullptr);
            }
            break;
        case Relation::AncestorAnySibling: {
            // :nth-last-child(even of .changed) .subject
            SelectorMatchingState selectorMatchingState;
            for (Ref sibling : childrenOfType<Element>(*element.parentNode())) {
                selectorMatchingState.selectorFilter.popParentsUntil(element.parentElement());
                invalidateStyleForDescendants(sibling.get(), &selectorMatchingState);
            }
            break;
        }
        case Relation::Host:
            // :host(.changed) .subject
            invalidateInShadowTreeIfNeeded(element);
            break;
        case Relation::HostChild:
            // ::slotted(.changed)
            if (RefPtr host = element.shadowHost()) {
                for (Ref hostChild : childrenOfType<Element>(*host))
                    invalidateIfNeeded(hostChild.get(), nullptr);
            }
            break;
        }
        return;
    }

    auto hasRelation = *matchElement.hasRelation;

    // HasScopeBreaking: :has(:is(.changed .a)) — invalidate entire document.
    if (hasRelation == HasRelation::ScopeBreaking) {
        SelectorMatchingState selectorMatchingState;
        invalidateStyleForDescendants(*element.document().documentElement(), &selectorMatchingState);
        return;
    }

    // :has() in non-subject position.
    if (matchElement.relation != Relation::Subject) {
        if (matchElement.relation == Relation::Parent && hasRelation == HasRelation::Child) {
            // :has(> .changed) > .subject
            SelectorMatchingState selectorMatchingState;
            if (RefPtr parent = element.parentElement())
                selectorMatchingState.selectorFilter.pushParentInitializingIfNeeded(*parent);
            for (Ref sibling : childrenOfType<Element>(*element.parentNode()))
                invalidateIfNeeded(sibling.get(), &selectorMatchingState);
            return;
        }
        if (matchElement.relation == Relation::Parent && hasRelation == HasRelation::Descendant) {
            // :has(.changed) > .subject
            Vector<Element*, 16> ancestors;
            for (RefPtr parent = element.parentElement(); parent; parent = parent->parentElement())
                ancestors.append(parent.get());

            SelectorMatchingState selectorMatchingState;
            selectorMatchingState.selectorFilter.parentStackReserveInitialCapacity(ancestors.size());
            for (RefPtr ancestor : ancestors | std::views::reverse) {
                selectorMatchingState.selectorFilter.pushParent(ancestor.get());
                for (Ref ancestorChild : childrenOfType<Element>(*ancestor))
                    invalidateIfNeeded(ancestorChild.get(), &selectorMatchingState);
            }
            return;
        }
        if (matchElement.relation == Relation::Ancestor && hasRelation == HasRelation::Child) {
            // :has(> .changed) .subject
            if (CheckedPtr parent = element.parentElement()) {
                SelectorMatchingState selectorMatchingState;
                invalidateStyleForDescendants(*parent, &selectorMatchingState);
            }
            return;
        }
        if (matchElement.relation == Relation::Ancestor && hasRelation == HasRelation::Descendant) {
            // .foo:has(.changed) .subject — find outermost ancestor matching any scope selector (.foo) to bound traversal.
            auto scopeElement = [&] -> RefPtr<Element> {
                Vector<Element*, 16> ancestors;
                for (RefPtr ancestor = element.parentElement(); ancestor; ancestor = ancestor->parentElement())
                    ancestors.append(ancestor.get());
                SelectorChecker selectorChecker(element.document());
                SelectorChecker::CheckingContext checkingContext(SelectorChecker::Mode::StyleInvalidation);
                for (RefPtr ancestor : ancestors | std::views::reverse) {
                    for (auto& ruleSet : m_ruleSets) {
                        if (!ruleSet.scopeSelector)
                            return element.document().documentElement();
                        for (auto& selector : *ruleSet.scopeSelector) {
                            if (selectorChecker.match(selector, *ancestor, checkingContext))
                                return ancestor;
                        }
                    }
                }
                return element.document().documentElement();
            }();

            if (scopeElement) {
                SelectorMatchingState selectorMatchingState;
                invalidateStyleForDescendants(*scopeElement, &selectorMatchingState);
                return;
            }
        }
        // Remaining non-subject :has() cases fall back to full document traversal.

        SelectorMatchingState selectorMatchingState;
        invalidateStyleForDescendants(*element.document().documentElement(), &selectorMatchingState);
        return;
    }

    // :has() in subject position.
    switch (hasRelation) {
    case HasRelation::Child:
        // :has(> .changed)
        if (RefPtr parent = element.parentElement())
            invalidateIfNeeded(*parent, nullptr);
        break;
    case HasRelation::Descendant: {
        // :has(.changed)
        Vector<Element*, 16> ancestors;
        for (RefPtr parent = element.parentElement(); parent; parent = parent->parentElement())
            ancestors.append(parent.get());

        SelectorMatchingState selectorMatchingState;
        selectorMatchingState.selectorFilter.parentStackReserveInitialCapacity(ancestors.size());
        for (RefPtr ancestor : ancestors | std::views::reverse) {
            invalidateIfNeeded(*ancestor, &selectorMatchingState);
            selectorMatchingState.selectorFilter.pushParent(ancestor.get());
        }
        break;
    }
    case HasRelation::DirectSibling:
        // :has(+ .changed)
        if (RefPtr sibling = element.previousElementSibling())
            invalidateIfNeeded(*sibling, nullptr);
        break;
    case HasRelation::IndirectSibling: {
        // :has(~ .changed)
        SelectorMatchingState selectorMatchingState;
        if (RefPtr parent = element.parentElement())
            selectorMatchingState.selectorFilter.pushParentInitializingIfNeeded(*parent);
        for (RefPtr sibling = element.previousElementSibling(); sibling; sibling = sibling->previousElementSibling())
            invalidateIfNeeded(*sibling, &selectorMatchingState);
        break;
    }
    case HasRelation::SiblingChild:
    case HasRelation::SiblingDescendant: {
        // :has(~ .a .changed) or :has(~ .a > .changed)
        Vector<Element*, 16> elementAndAncestors;
        elementAndAncestors.append(&element);
        for (RefPtr parent = element.parentElement(); parent; parent = parent->parentElement())
            elementAndAncestors.append(parent.get());

        SelectorMatchingState selectorMatchingState;
        selectorMatchingState.selectorFilter.parentStackReserveInitialCapacity(elementAndAncestors.size());
        for (RefPtr elementOrAncestor : elementAndAncestors | std::views::reverse) {
            for (RefPtr sibling = elementOrAncestor->previousElementSibling(); sibling; sibling = sibling->previousElementSibling())
                invalidateIfNeeded(*sibling, &selectorMatchingState);
            selectorMatchingState.selectorFilter.pushParent(elementOrAncestor.get());
        }
        break;
    }
    case HasRelation::ScopeBreaking:
        ASSERT_NOT_REACHED();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void Invalidator::invalidateShadowParts(ShadowRoot& shadowRoot)
{
    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return;

    for (Ref descendant : descendantsOfType<Element>(shadowRoot)) {
        // FIXME: We could only invalidate part names that actually show up in rules.
        if (!descendant->partNames().isEmpty())
            descendant->invalidateStyleInternal();

        RefPtr nestedShadowRoot = descendant->shadowRoot();
        if (nestedShadowRoot && !nestedShadowRoot->partMappings().isEmpty())
            invalidateShadowParts(*nestedShadowRoot);
    }
}

void Invalidator::invalidateUserAgentParts(ShadowRoot& shadowRoot)
{
    if (shadowRoot.mode() != ShadowRootMode::UserAgent)
        return;

    for (Ref descendant : descendantsOfType<Element>(shadowRoot)) {
        auto& part = descendant->userAgentPart();
        if (!part)
            continue;
        for (auto& ruleSet : m_ruleSets) {
            if (ruleSet.ruleSet->userAgentPartRules(part))
                descendant->invalidateStyleInternal();
        }
    }
}

void Invalidator::invalidateInShadowTreeIfNeeded(Element& element)
{
    RefPtr shadowRoot = element.shadowRoot();
    if (!shadowRoot)
        return;

    if (m_ruleInformation.hasUserAgentPartRules)
        invalidateUserAgentParts(*shadowRoot);

    if (m_ruleInformation.hasHostPseudoClassRulesMatchingInShadowTree) {
        for (Ref child : childrenOfType<Element>(*shadowRoot)) {
            SelectorMatchingState selectorMatchingState;
            invalidateStyleForTree(child.get(), &selectorMatchingState);
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
    auto& scopeSelector = invalidationRuleSet.scopeSelector;
    matchElementRuleSets.ensure(invalidationRuleSet.matchElement, [] {
        return InvalidationRuleSetVector { };
    }).iterator->value.append({ invalidationRuleSet.ruleSet.copyRef(), IsNegation::No, scopeSelector.isEmpty() ? nullptr : &scopeSelector });
}

void Invalidator::addToMatchElementRuleSetsRespectingNegation(Invalidator::MatchElementRuleSets& matchElementRuleSets, const InvalidationRuleSet& invalidationRuleSet)
{
    auto& scopeSelector = invalidationRuleSet.scopeSelector;
    matchElementRuleSets.ensure(invalidationRuleSet.matchElement, [] {
        return InvalidationRuleSetVector { };
    }).iterator->value.append({ invalidationRuleSet.ruleSet.copyRef(), invalidationRuleSet.isNegation, scopeSelector.isEmpty() ? nullptr : &scopeSelector });
}

void Invalidator::invalidateWithMatchElementRuleSets(Element& element, const MatchElementRuleSets& matchElementRuleSets)
{
    SetForScope isInvalidating(element.styleResolver().ruleSets().isInvalidatingStyleWithRuleSets(), true);

    for (auto& matchElementAndRuleSet : matchElementRuleSets) {
        Invalidator invalidator(matchElementAndRuleSet.value);
        invalidator.invalidateStyleWithMatchElement(element, matchElementAndRuleSet.key.key());
        element.document().incrementStyleInvalidationTraversalCountForTesting(invalidator.m_elementTraversalCount);
    }
}

void Invalidator::invalidateWithScopeBreakingHasPseudoClassRuleSet(Element& element, const RuleSet* ruleSet)
{
    SetForScope isInvalidating(element.styleResolver().ruleSets().isInvalidatingStyleWithRuleSets(), true);
    Invalidator invalidator(InvalidationRuleSetVector { { ruleSet } });
    // Scope-breaking :has() can be affected by changes anywhere, so invalidate all descendants from root.
    SelectorMatchingState selectorMatchingState;
    invalidator.invalidateStyleForDescendants(*element.document().documentElement(), &selectorMatchingState);
}

void Invalidator::invalidateAllStyle(Scope& scope)
{
    if (RefPtr shadowRoot = scope.shadowRoot()) {
        for (Ref shadowChild : childrenOfType<Element>(*shadowRoot))
            shadowChild->invalidateStyleForSubtreeInternal();
        invalidateHostAndSlottedStyleIfNeeded(*shadowRoot);
        return;
    }

    scope.document().scheduleFullStyleRebuild();
}

void Invalidator::invalidateHostAndSlottedStyleIfNeeded(ShadowRoot& shadowRoot)
{
    Ref host = *shadowRoot.host();
    RefPtr resolver = shadowRoot.styleScope().resolverIfExists();

    if (!resolver || resolver->ruleSets().hasMatchingUserOrAuthorStyle([] (auto& style) { return !style.hostPseudoClassRules().isEmpty(); }))
        host->invalidateStyleInternal();

    if (!resolver || resolver->ruleSets().hasMatchingUserOrAuthorStyle([] (auto& style) { return !style.slottedPseudoElementRules().isEmpty(); })) {
        for (Ref shadowChild : childrenOfType<Element>(host.get()))
            shadowChild->invalidateStyleInternal();
    }
}

}
}
