/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "ElementRuleCollector.h"

#include "CSSKeyframeRule.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CascadeLevel.h"
#include "ContainerQueryEvaluator.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "ElementTextDirection.h"
#include "HTMLElement.h"
#include "HTMLSlotElement.h"
#include "SVGElement.h"
#include "SelectorCheckerTestFunctions.h"
#include "SelectorCompiler.h"
#include "SelectorMatchingState.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRuleImport.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include "UserAgentStyle.h"
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static const StyleProperties& leftToRightDeclaration()
{
IGNORE_GCC_WARNINGS_BEGIN("dangling-reference")
    static auto& declaration = [] () -> const StyleProperties& {
        auto properties = MutableStyleProperties::create();
        properties->setProperty(CSSPropertyDirection, CSSValueLtr);
        return properties.leakRef();
    }();
IGNORE_GCC_WARNINGS_END
    return declaration;
}

static const StyleProperties& rightToLeftDeclaration()
{
IGNORE_GCC_WARNINGS_BEGIN("dangling-reference")
    static auto& declaration = [] () -> const StyleProperties& {
        auto properties = MutableStyleProperties::create();
        properties->setProperty(CSSPropertyDirection, CSSValueRtl);
        return properties.leakRef();
    }();
IGNORE_GCC_WARNINGS_END
    return declaration;
}

struct MatchRequest {
    MatchRequest(const RuleSet& ruleSet, ScopeOrdinal styleScopeOrdinal = ScopeOrdinal::Element)
        : ruleSet(ruleSet)
        , styleScopeOrdinal(styleScopeOrdinal)
    {
    }
    const RuleSet& ruleSet;
    ScopeOrdinal styleScopeOrdinal;
    bool matchingPartPseudoElementRules { false };
};

ElementRuleCollector::ElementRuleCollector(const Element& element, const ScopeRuleSets& ruleSets, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_authorStyle(ruleSets.authorStyle())
    , m_userStyle(ruleSets.userStyle())
    , m_userAgentMediaQueryStyle(ruleSets.userAgentMediaQueryStyle())
    , m_dynamicViewTransitionsStyle(ruleSets.dynamicViewTransitionsStyle())
    , m_selectorMatchingState(selectorMatchingState)
    , m_result(makeUnique<MatchResult>(element.isLink()))
{
    ASSERT(!m_selectorMatchingState || m_selectorMatchingState->selectorFilter.parentStackIsConsistent(element.parentNode()));
}

ElementRuleCollector::ElementRuleCollector(const Element& element, const RuleSet& authorStyle, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_authorStyle(authorStyle)
    , m_selectorMatchingState(selectorMatchingState)
    , m_result(makeUnique<MatchResult>(element.isLink()))
{
    ASSERT(!m_selectorMatchingState || m_selectorMatchingState->selectorFilter.parentStackIsConsistent(element.parentNode()));
}

const MatchResult& ElementRuleCollector::matchResult() const
{
    ASSERT(m_mode == SelectorChecker::Mode::ResolvingStyle);
    return *m_result;
}

std::unique_ptr<MatchResult> ElementRuleCollector::releaseMatchResult()
{
    return WTFMove(m_result);
}

const Vector<RefPtr<const StyleRule>>& ElementRuleCollector::matchedRuleList() const
{
    ASSERT(m_mode == SelectorChecker::Mode::CollectingRules);
    return m_matchedRuleList;
}

inline void ElementRuleCollector::addMatchedRule(const RuleData& ruleData, unsigned specificity, unsigned scopingRootDistance, const MatchRequest& matchRequest)
{
    auto cascadeLayerPriority = matchRequest.ruleSet.cascadeLayerPriorityFor(ruleData);
    m_matchedRules.append({ &ruleData, specificity, scopingRootDistance, matchRequest.styleScopeOrdinal, cascadeLayerPriority });
}

void ElementRuleCollector::clearMatchedRules()
{
    m_matchedRules.clear();
    m_matchedRuleTransferIndex = 0;
}

inline void ElementRuleCollector::addElementStyleProperties(const StyleProperties* propertySet, CascadeLayerPriority priority, IsCacheable isCacheable, FromStyleAttribute fromStyleAttribute)
{
    if (!propertySet || propertySet->isEmpty())
        return;

    auto matchedProperty = MatchedProperties { *propertySet };
    matchedProperty.cascadeLayerPriority = priority;
    matchedProperty.fromStyleAttribute = fromStyleAttribute;
    matchedProperty.isCacheable = isCacheable;
    addMatchedProperties(WTFMove(matchedProperty), DeclarationOrigin::Author);
}

void ElementRuleCollector::collectMatchingRules(CascadeLevel level)
{
    switch (level) {
    case CascadeLevel::Author: {
        MatchRequest matchRequest(m_authorStyle);
        collectMatchingRules(matchRequest);
        break;
    }

    case CascadeLevel::User:
        if (m_userStyle) {
            MatchRequest matchRequest(*m_userStyle);
            collectMatchingRules(matchRequest);
        }
        break;

    case CascadeLevel::UserAgent:
        ASSERT_NOT_REACHED();
        return;
    }

    auto* parent = element().parentElement();
    if (parent && parent->shadowRoot())
        matchSlottedPseudoElementRules(level);

    if (element().shadowRoot())
        matchHostPseudoClassRules(level);

    if (element().isInShadowTree()) {
        matchUserAgentPartRules(level);
        matchPartPseudoElementRules(level);
    }
}

void ElementRuleCollector::collectMatchingRules(const MatchRequest& matchRequest)
{
    ASSERT_WITH_MESSAGE(!(m_mode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && m_pseudoElementRequest), "When in StyleInvalidation or SharingRules, SelectorChecker does not try to match the pseudo ID. While ElementRuleCollector supports matching a particular pseudoId in this case, this would indicate a error at the call site since matching a particular element should be unnecessary.");

    auto& element = this->element();
    auto* shadowRoot = element.containingShadowRoot();
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        collectMatchingUserAgentPartRules(matchRequest);

    bool isHTML = element.isHTMLElement() && element.document().isHTMLDocument();

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    auto& id = element.idForStyleResolution();
    if (!id.isNull())
        collectMatchingRulesForList(matchRequest.ruleSet.idRules(id), matchRequest);
    if (element.hasClass()) {
        for (auto& className : element.classNames())
            collectMatchingRulesForList(matchRequest.ruleSet.classRules(className), matchRequest);
    }
    if (element.hasAttributesWithoutUpdate() && matchRequest.ruleSet.hasAttributeRules()) {
        Vector<const RuleSet::RuleDataVector*, 4> ruleVectors;
        for (auto& attribute : element.attributesIterator()) {
            if (auto* rules = matchRequest.ruleSet.attributeRules(attribute.localName(), isHTML))
                ruleVectors.append(rules);
        }
        for (auto* rules : ruleVectors)
            collectMatchingRulesForList(rules, matchRequest);
    }
    if (m_pseudoElementRequest && m_pseudoElementRequest->nameArgument() != nullAtom())
        collectMatchingRulesForList(matchRequest.ruleSet.namedPseudoElementRules(m_pseudoElementRequest->nameArgument()), matchRequest);
    if (element.isLink())
        collectMatchingRulesForList(matchRequest.ruleSet.linkPseudoClassRules(), matchRequest);
    if (matchesFocusPseudoClass(element))
        collectMatchingRulesForList(matchRequest.ruleSet.focusPseudoClassRules(), matchRequest);
    if (&element == element.document().documentElement())
        collectMatchingRulesForList(matchRequest.ruleSet.rootElementRules(), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet.tagRules(element.localName(), isHTML), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet.universalRules(), matchRequest);
}


Vector<MatchedProperties>& ElementRuleCollector::declarationsForOrigin(DeclarationOrigin declarationOrigin)
{
    switch (declarationOrigin) {
    case DeclarationOrigin::UserAgent: return m_result->userAgentDeclarations;
    case DeclarationOrigin::User: return m_result->userDeclarations;
    case DeclarationOrigin::Author: return m_result->authorDeclarations;
    }
    ASSERT_NOT_REACHED();
    return m_result->authorDeclarations;
}

void ElementRuleCollector::sortAndTransferMatchedRules(DeclarationOrigin declarationOrigin)
{
    if (m_matchedRules.isEmpty())
        return;

    sortMatchedRules();

    transferMatchedRules(declarationOrigin);
}

void ElementRuleCollector::transferMatchedRules(DeclarationOrigin declarationOrigin, std::optional<ScopeOrdinal> fromScope)
{
    if (m_mode != SelectorChecker::Mode::CollectingRules)
        declarationsForOrigin(declarationOrigin).reserveCapacity(m_matchedRules.size());

    for (; m_matchedRuleTransferIndex < m_matchedRules.size(); ++m_matchedRuleTransferIndex) {
        auto& matchedRule = m_matchedRules[m_matchedRuleTransferIndex];
        if (fromScope && matchedRule.styleScopeOrdinal < *fromScope)
            break;

        if (m_mode == SelectorChecker::Mode::CollectingRules) {
            m_matchedRuleList.append(&matchedRule.ruleData->styleRule());
            continue;
        }

        addMatchedProperties({
            matchedRule.ruleData->styleRule().properties(),
            static_cast<uint8_t>(matchedRule.ruleData->linkMatchType()),
            matchedRule.ruleData->propertyAllowlist(),
            matchedRule.styleScopeOrdinal,
            FromStyleAttribute::No,
            matchedRule.cascadeLayerPriority,
            matchedRule.ruleData->isStartingStyle()
        }, declarationOrigin);
    }
}

void ElementRuleCollector::matchAuthorRules()
{
    clearMatchedRules();

    collectMatchingRules(CascadeLevel::Author);

    sortAndTransferMatchedRules(DeclarationOrigin::Author);
}

bool ElementRuleCollector::matchesAnyAuthorRules()
{
    clearMatchedRules();

    // FIXME: This should bail out on first match.
    collectMatchingRules(CascadeLevel::Author);

    return !m_matchedRules.isEmpty();
}

void ElementRuleCollector::matchUserAgentPartRules(CascadeLevel level)
{
    ASSERT(element().isInShadowTree());
    auto* shadowRoot = element().containingShadowRoot();
    if (!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent)
        return;

    // Look up user agent parts also from the host scope style as they are web-exposed.
    auto* hostRules = Scope::forNode(*shadowRoot->host()).resolver().ruleSets().styleForCascadeLevel(level);
    if (!hostRules)
        return;

    MatchRequest hostRequest { *hostRules, ScopeOrdinal::ContainingHost };
    collectMatchingUserAgentPartRules(hostRequest);
}

void ElementRuleCollector::matchHostPseudoClassRules(CascadeLevel level)
{
    ASSERT(element().shadowRoot());

    auto* shadowRules = element().shadowRoot()->styleScope().resolver().ruleSets().styleForCascadeLevel(level);
    if (!shadowRules)
        return;

    auto collect = [&] (const auto& rules) {
        if (rules.isEmpty())
            return;

        MatchRequest hostMatchRequest { *shadowRules, ScopeOrdinal::Shadow };
        collectMatchingRulesForList(&rules, hostMatchRequest);
    };

    if (shadowRules->hasHostPseudoClassRulesInUniversalBucket()) {
        if (auto* universalRules = shadowRules->universalRules())
            collect(*universalRules);
    }

    collect(shadowRules->hostPseudoClassRules());
}

void ElementRuleCollector::matchSlottedPseudoElementRules(CascadeLevel level)
{
    auto* slot = element().assignedSlot();
    auto styleScopeOrdinal = ScopeOrdinal::FirstSlot;

    for (; slot; slot = slot->assignedSlot(), ++styleScopeOrdinal) {
        auto& styleScope = Scope::forNode(*slot);
        if (!styleScope.resolver().ruleSets().isAuthorStyleDefined())
            continue;

        auto* scopeRules = styleScope.resolver().ruleSets().styleForCascadeLevel(level);
        if (!scopeRules)
            continue;

        MatchRequest scopeMatchRequest(*scopeRules, styleScopeOrdinal);
        collectMatchingRulesForList(&scopeRules->slottedPseudoElementRules(), scopeMatchRequest);

        if (styleScopeOrdinal == ScopeOrdinal::SlotLimit)
            break;
    }
}

void ElementRuleCollector::matchPartPseudoElementRules(CascadeLevel level)
{
    ASSERT(element().isInShadowTree());
    if (!element().containingShadowRoot())
        return;

    bool isUserAgentPart = element().containingShadowRoot()->mode() == ShadowRootMode::UserAgent && !element().userAgentPart().isNull();

    auto& partMatchingElement = isUserAgentPart ? *element().shadowHost() : element();
    if (partMatchingElement.partNames().isEmpty() || !partMatchingElement.isInShadowTree())
        return;

    matchPartPseudoElementRulesForScope(partMatchingElement, level);
}

void ElementRuleCollector::matchPartPseudoElementRulesForScope(const Element& partMatchingElement, CascadeLevel level)
{
    auto* element = &partMatchingElement;
    auto styleScopeOrdinal = ScopeOrdinal::Element;

    for (; element; element = element->shadowHost(), --styleScopeOrdinal) {
        auto& styleScope = Scope::forNode(const_cast<Element&>(*element));
        if (!styleScope.resolver().ruleSets().isAuthorStyleDefined())
            continue;

        auto* hostRules = styleScope.resolver().ruleSets().styleForCascadeLevel(level);
        if (!hostRules)
            continue;

        MatchRequest scopeMatchRequest(*hostRules, styleScopeOrdinal);
        scopeMatchRequest.matchingPartPseudoElementRules = true;

        collectMatchingRulesForList(&hostRules->partPseudoElementRules(), scopeMatchRequest);

        // Element may only be exposed to styling from enclosing scopes via exportparts attributes.
        if (element != &partMatchingElement && element->shadowRoot()->partMappings().isEmpty())
            break;

        if (styleScopeOrdinal == ScopeOrdinal::ContainingHostLimit)
            break;
    }
}

void ElementRuleCollector::collectMatchingUserAgentPartRules(const MatchRequest& matchRequest)
{
    ASSERT(element().isInUserAgentShadowTree());

    auto& rules = matchRequest.ruleSet;
#if ENABLE(VIDEO)
    if (element().isWebVTTElement())
        collectMatchingRulesForList(&rules.cuePseudoRules(), matchRequest);
#endif
    if (auto& part = element().userAgentPart(); !part.isEmpty())
        collectMatchingRulesForList(rules.userAgentPartRules(part), matchRequest);
}

void ElementRuleCollector::matchUserRules()
{
    clearMatchedRules();

    collectMatchingRules(CascadeLevel::User);

    sortAndTransferMatchedRules(DeclarationOrigin::User);
}

void ElementRuleCollector::matchUARules()
{
    // First we match rules from the user agent sheet.
    auto* userAgentStyleSheet = m_isPrintStyle
        ? UserAgentStyle::defaultPrintStyle : UserAgentStyle::defaultStyle;
    matchUARules(*userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (element().document().inQuirksMode())
        matchUARules(*UserAgentStyle::defaultQuirksStyle);

    if (m_userAgentMediaQueryStyle)
        matchUARules(*m_userAgentMediaQueryStyle);

    if (m_dynamicViewTransitionsStyle)
        matchUARules(*m_dynamicViewTransitionsStyle);
}

void ElementRuleCollector::matchUARules(const RuleSet& rules)
{
    clearMatchedRules();

    collectMatchingRules(MatchRequest(rules));

    sortAndTransferMatchedRules(DeclarationOrigin::UserAgent);
}

static Vector<AtomString> classListForNamedViewTransitionPseudoElement(const Document& document, const AtomString& name)
{
    auto* activeViewTransition = document.activeViewTransition();
    if (!activeViewTransition)
        return { };

    ASSERT(!name.isNull());

    auto* capturedElement = activeViewTransition->namedElements().find(name);
    if (!capturedElement)
        return { };

    return capturedElement->classList;
}

inline bool ElementRuleCollector::ruleMatches(const RuleData& ruleData, unsigned& specificity, ScopeOrdinal styleScopeOrdinal, const ContainerNode* scopingRoot)
{
    // We know a sufficiently simple single part selector matches simply because we found it from the rule hash when filtering the RuleSet.
    // This is limited to HTML only so we don't need to check the namespace (because of tag name match).
    auto matchBasedOnRuleHash = ruleData.matchBasedOnRuleHash();
    if (matchBasedOnRuleHash != MatchBasedOnRuleHash::None && element().isHTMLElement()) {
        ASSERT_WITH_MESSAGE(!m_pseudoElementRequest, "If we match based on the rule hash while collecting for a particular pseudo element ID, we would add incorrect rules for that pseudo element ID. We should never end in ruleMatches() with a pseudo element if the ruleData cannot match any pseudo element.");

        switch (matchBasedOnRuleHash) {
        case MatchBasedOnRuleHash::None:
            ASSERT_NOT_REACHED();
            break;
        case MatchBasedOnRuleHash::Universal:
            specificity = 0;
            break;
        case MatchBasedOnRuleHash::ClassA:
            specificity = static_cast<unsigned>(SelectorSpecificityIncrement::ClassA);
            break;
        case MatchBasedOnRuleHash::ClassB:
            specificity = static_cast<unsigned>(SelectorSpecificityIncrement::ClassB);
            break;
        case MatchBasedOnRuleHash::ClassC:
            specificity = static_cast<unsigned>(SelectorSpecificityIncrement::ClassC);
            break;
        }
        return true;
    }

#if ENABLE(CSS_SELECTOR_JIT)
    auto& compiledSelector = ruleData.compiledSelector();

    if (compiledSelector.status == SelectorCompilationStatus::NotCompiled)
        SelectorCompiler::compileSelector(compiledSelector, ruleData.selector(), SelectorCompiler::SelectorContext::RuleCollector);

    if (compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker) {
        compiledSelector.wasUsed();

#if !ASSERT_MSG_DISABLED
        unsigned ignoreSpecificity;
        ASSERT_WITH_MESSAGE(!SelectorCompiler::ruleCollectorSimpleSelectorChecker(compiledSelector, &element(), &ignoreSpecificity) || !m_pseudoElementRequest, "When matching pseudo elements, we should never compile a selector checker without context unless it cannot match anything.");
#endif
        bool selectorMatches = SelectorCompiler::ruleCollectorSimpleSelectorChecker(compiledSelector, &element(), &specificity);

        if (selectorMatches && ruleData.containsUncommonAttributeSelector())
            m_didMatchUncommonAttributeSelector = true;

        return selectorMatches;
    }
#endif // ENABLE(CSS_SELECTOR_JIT)

    SelectorChecker::CheckingContext context(m_mode);
    if (m_pseudoElementRequest) {
        context.pseudoId = m_pseudoElementRequest->pseudoId();
        context.pseudoElementNameArgument = m_pseudoElementRequest->nameArgument();
        context.scrollbarState = m_pseudoElementRequest->scrollbarState();
        if (isNamedViewTransitionPseudoElement(m_pseudoElementRequest->identifier()))
            context.classList = classListForNamedViewTransitionPseudoElement(element().document(), context.pseudoElementNameArgument);
    }
    context.styleScopeOrdinal = styleScopeOrdinal;
    context.selectorMatchingState = m_selectorMatchingState;
    context.scope = scopingRoot;

    bool selectorMatches;
#if ENABLE(CSS_SELECTOR_JIT)
    if (compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext) {
        compiledSelector.wasUsed();
        selectorMatches = SelectorCompiler::ruleCollectorSelectorCheckerWithCheckingContext(compiledSelector, &element(), &context, &specificity);
    } else
#endif // ENABLE(CSS_SELECTOR_JIT)
    {
        auto* selector = ruleData.selector();
        // Slow path.
        SelectorChecker selectorChecker(element().document());
        selectorMatches = selectorChecker.match(*selector, element(), context);
        if (selectorMatches)
            specificity = selector->computeSpecificity();
    }

    if (ruleData.containsUncommonAttributeSelector()) {
        if (selectorMatches || context.pseudoIDSet)
            m_didMatchUncommonAttributeSelector = true;
    }
    m_matchedPseudoElementIds.merge(context.pseudoIDSet);
    m_styleRelations.appendVector(context.styleRelations);

    return selectorMatches;
}

void ElementRuleCollector::collectMatchingRulesForList(const RuleSet::RuleDataVector* rules, const MatchRequest& matchRequest)
{
    if (!rules)
        return;

    for (auto& ruleData : *rules) {
        if (UNLIKELY(!ruleData.isEnabled()))
            continue;

        if (!ruleData.canMatchPseudoElement() && m_pseudoElementRequest)
            continue;

        if (m_selectorMatchingState && m_selectorMatchingState->selectorFilter.fastRejectSelector(ruleData.descendantSelectorIdentifierHashes()))
            continue;

        if (matchRequest.ruleSet.hasContainerQueries() && !containerQueriesMatch(ruleData, matchRequest))
            continue;

        std::optional<Vector<ScopingRootWithDistance>> scopingRoots;
        if (matchRequest.ruleSet.hasScopeRules()) {
            auto [result, roots] = scopeRulesMatch(ruleData, matchRequest);
            if (!result)
                continue;
            scopingRoots = WTFMove(roots);
        }

        auto& rule = ruleData.styleRule();

        // If the rule has no properties to apply, then ignore it in the non-debug mode.
        if (rule.properties().isEmpty() && !m_shouldIncludeEmptyRules)
            continue;

        auto addRuleIfMatches = [&] (const ScopingRootWithDistance& scopingRootWithDistance = { }) {
            unsigned specificity;
            if (ruleMatches(ruleData, specificity, matchRequest.styleScopeOrdinal, scopingRootWithDistance.scopingRoot.get()))
                addMatchedRule(ruleData, specificity, scopingRootWithDistance.distance, matchRequest);
        };

        if (scopingRoots) {
            for (auto& scopingRoot : *scopingRoots)
                addRuleIfMatches(scopingRoot);
            continue;
        }

        addRuleIfMatches();
    }
}

bool ElementRuleCollector::containerQueriesMatch(const RuleData& ruleData, const MatchRequest& matchRequest)
{
    auto queries = matchRequest.ruleSet.containerQueriesFor(ruleData);

    if (queries.isEmpty())
        return true;

    // Style bits indicating which pseudo-elements match are set during regular element matching. Container queries need to be evaluate in the right mode.
    auto selectionMode = [&] {
        if (matchRequest.matchingPartPseudoElementRules)
            return ContainerQueryEvaluator::SelectionMode::PartPseudoElement;
        if (ruleData.canMatchPseudoElement())
            return ContainerQueryEvaluator::SelectionMode::PseudoElement;
        return ContainerQueryEvaluator::SelectionMode::Element;
    }();

    auto* containerQueryEvaluationState = m_selectorMatchingState ? &m_selectorMatchingState->containerQueryEvaluationState : nullptr;

    // "Style rules defined on an element inside multiple nested container queries apply when all of the wrapping container queries are true for that element."
    ContainerQueryEvaluator evaluator(element(), selectionMode, matchRequest.styleScopeOrdinal, containerQueryEvaluationState);
    for (auto* query : queries) {
        if (!evaluator.evaluate(*query))
            return false;
    }
    return true;
}

std::pair<bool, std::optional<Vector<ElementRuleCollector::ScopingRootWithDistance>>> ElementRuleCollector::scopeRulesMatch(const RuleData& ruleData, const MatchRequest& matchRequest)
{
    auto scopeRules = matchRequest.ruleSet.scopeRulesFor(ruleData);

    if (scopeRules.isEmpty())
        return { true, { } };

    SelectorChecker checker(element().rootElement()->document());
    SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements);

    Vector<ScopingRootWithDistance> scopingRoots;
    auto isWithinScope = [&](auto& rule) {
        auto previousScopingRoots = WTFMove(scopingRoots);
        // The last rule (=innermost @scope rule) determines the scoping roots
        scopingRoots.clear();

        auto findScopingRoots = [&](const auto& selectorList) {
            unsigned distance = 0;
            const auto* ancestor = &element();
            while (ancestor) {
                for (const auto* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
                    auto appendIfMatch = [&] (const ContainerNode* previousScopingRoot = nullptr) {
                        auto subContext = context;
                        subContext.scope = previousScopingRoot;
                        auto match = checker.match(*selector, *ancestor, subContext);
                        if (match)
                            scopingRoots.append({ ancestor, distance });
                    };
                    if (previousScopingRoots.isEmpty())
                        appendIfMatch();
                    else {
                        for (const auto& [previousScopingRoot, _] : previousScopingRoots)
                            appendIfMatch(previousScopingRoot.get());
                    }
                }
                ancestor = ancestor->parentElement();
                ++distance;
            }
        };
        /* 
        An element is in scope if:
            It is an inclusive descendant of the scoping root, and
            It is not an inclusive descendant of a scoping limit.
        */
        auto isWithinScopingRootsAndScopeEnd = [&](const CSSSelectorList& scopeEnd) {
            auto match = [&] (const auto* scopingRoot, const auto* selector) {
                auto subContext = context;
                subContext.scope = scopingRoot;
                const auto* ancestor = &element();
                while (ancestor) {
                    auto match = checker.match(*selector, *ancestor, subContext);
                    if (match)
                        return true;
                    if (ancestor == scopingRoot) {
                        // The end of the scope can't be an ancestor of the start of the scope.
                        return false;
                    }
                    ancestor = ancestor->parentElement();
                }
                return false;
            };

            Vector<ScopingRootWithDistance> scopingRootsWithinScope;
            for (auto scopingRootWithDistance : scopingRoots) {
                bool anyScopingLimitMatch = false;
                for (const auto* selector = scopeEnd.first(); selector; selector = CSSSelectorList::next(selector)) {
                    if (match(scopingRootWithDistance.scopingRoot.get(), selector)) {
                        anyScopingLimitMatch = true;
                        break;
                    }
                }
                if (!anyScopingLimitMatch)
                    scopingRootsWithinScope.append(scopingRootWithDistance);
            }
            return scopingRootsWithinScope;
        };

        const auto& scopeStart = rule->scopeStart();
        if (!scopeStart.isEmpty()) {
            findScopingRoots(scopeStart);
            if (scopingRoots.isEmpty())
                return false;
        } else {
            // The scoping root is the parent element of the owner node of the stylesheet where the @scope rule is defined. (If no such element exists, then the scoping root is the root of the containing node tree.
            RefPtr styleSheetContents = rule->styleSheetContents().get();
            if (!styleSheetContents)
                return false;

            styleSheetContents = styleSheetContents->rootStyleSheet();
            ASSERT(styleSheetContents);
            if (!styleSheetContents)
                return false;

            auto appendImplicitScopingRoot = [&](const auto* client) {
                // Verify that the node is in the current document
                if (client->ownerDocument() != &this->element().document())
                    return;
                // The owner node should be the <style> node
                const auto* owner = client->ownerNode();
                if (!owner)
                    return;
                // Find the parent node of the <style>
                const auto* implicitParentNode = owner->parentNode();
                const auto* implicitParentContainerNode = dynamicDowncast<ContainerNode>(implicitParentNode);
                const auto* ancestor = &element();
                unsigned distance = 0;
                while (ancestor) {
                    if (ancestor == implicitParentNode)
                        break;
                    ancestor = ancestor->parentElement();
                    ++distance;
                }
                scopingRoots.append({ implicitParentContainerNode, distance });
            };

            // Each client might act as a scoping root.
            for (const auto& client : styleSheetContents->clients())
                appendImplicitScopingRoot(client);
        }

        const auto& scopeEnd = rule->scopeEnd();
        if (!scopeEnd.isEmpty()) {
            auto scopingRootsWithinScope = isWithinScopingRootsAndScopeEnd(scopeEnd);
            if (scopingRootsWithinScope.isEmpty())
                return false;
            scopingRoots = WTFMove(scopingRootsWithinScope);
        }
        // element is in the @scope donut
        return true;
    };

    // We need to respect each nested @scope to collect this rule
    for (auto& rule : scopeRules) {
        if (!isWithinScope(rule))
            return { false, { } };
    }

    return { true, WTFMove(scopingRoots) };
}

static inline bool compareRules(MatchedRule r1, MatchedRule r2)
{
    // For normal properties the earlier scope wins. This may be reversed by !important which is handled when resolving cascade.
    if (r1.styleScopeOrdinal != r2.styleScopeOrdinal)
        return r1.styleScopeOrdinal > r2.styleScopeOrdinal;

    if (r1.cascadeLayerPriority != r2.cascadeLayerPriority)
        return r1.cascadeLayerPriority < r2.cascadeLayerPriority;

    if (r1.specificity != r2.specificity)
        return r1.specificity < r2.specificity;

    // Rule with the smallest distance has priority.
    if (r1.scopingRootDistance != r2.scopingRootDistance)
        return r2.scopingRootDistance < r1.scopingRootDistance;

    return r1.ruleData->position() < r2.ruleData->position();
}

void ElementRuleCollector::sortMatchedRules()
{
    std::sort(m_matchedRules.begin(), m_matchedRules.end(), compareRules);
}

void ElementRuleCollector::matchAllRules(bool matchAuthorAndUserStyles, bool includeSMILProperties)
{
    matchUARules();

    // Now we check user sheet rules.
    if (matchAuthorAndUserStyles)
        matchUserRules();

    if (auto* styledElement = dynamicDowncast<StyledElement>(element())) {
        if (auto* presentationalHintStyle = styledElement->presentationalHintStyle()) {
            // https://html.spec.whatwg.org/#presentational-hints

            // Presentation attributes in SVG elements tend to be unique and not restyled often. Avoid bloating the cache.
            // FIXME: Refcount is an imperfect proxy for sharing within a single document.
            static constexpr auto matchedDeclarationsCacheSharingThreshold = 4;
            bool allowFullCaching = !styledElement->isSVGElement() || presentationalHintStyle->refCount() > matchedDeclarationsCacheSharingThreshold;

            auto isCacheable = allowFullCaching ? IsCacheable::Yes : IsCacheable::Partially;
            addElementStyleProperties(presentationalHintStyle, RuleSet::cascadeLayerPriorityForPresentationalHints, isCacheable);
        }

        // Tables and table cells share an additional presentation style that must be applied
        // after all attributes, since their style depends on the values of multiple attributes.
        addElementStyleProperties(styledElement->additionalPresentationalHintStyle(), RuleSet::cascadeLayerPriorityForPresentationalHints);

        if (auto* htmlElement = dynamicDowncast<HTMLElement>(*styledElement)) {
            if (auto textDirection = computeTextDirectionIfDirIsAuto(*htmlElement)) {
                auto& properties = *textDirection == TextDirection::LTR ? leftToRightDeclaration() : rightToLeftDeclaration();
                addMatchedProperties({ properties }, DeclarationOrigin::Author);
            }
        }
    }

    if (matchAuthorAndUserStyles) {
        clearMatchedRules();

        collectMatchingRules(CascadeLevel::Author);
        sortMatchedRules();

        transferMatchedRules(DeclarationOrigin::Author, ScopeOrdinal::Element);

        // Inline style behaves as if it has higher specificity than any rule.
        addElementInlineStyleProperties(includeSMILProperties);

        // Rules from the host scopes override inline style.
        transferMatchedRules(DeclarationOrigin::Author);
    }
}

void ElementRuleCollector::addElementInlineStyleProperties(bool includeSMILProperties)
{
    auto* styledElement = dynamicDowncast<StyledElement>(element());
    if (!styledElement)
        return;

    if (auto* inlineStyle = styledElement->inlineStyle()) {
        auto isInlineStyleCacheable = inlineStyle->isMutable() ? IsCacheable::No : IsCacheable::Yes;
        addElementStyleProperties(inlineStyle, RuleSet::cascadeLayerPriorityForUnlayered, isInlineStyleCacheable, FromStyleAttribute::Yes);
    }

    if (includeSMILProperties) {
        if (auto* svgElement = dynamicDowncast<SVGElement>(element()))
            addElementStyleProperties(svgElement->animatedSMILStyleProperties(), RuleSet::cascadeLayerPriorityForUnlayered, IsCacheable::No);
    }
}

bool ElementRuleCollector::hasAnyMatchingRules(const RuleSet& ruleSet)
{
    clearMatchedRules();

    m_mode = SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements;
    collectMatchingRules(MatchRequest(ruleSet));

    return !m_matchedRules.isEmpty();
}

void ElementRuleCollector::addMatchedProperties(MatchedProperties&& matchedProperties, DeclarationOrigin declarationOrigin)
{
    auto& declarations = declarationsForOrigin(declarationOrigin);
    if (!declarations.isEmpty() && declarations.last() == matchedProperties) {
        // It might also be beneficial to overwrite the previous declaration (insteading of appending) if it affects the same exact properties.
        return;
    }
    if (matchedProperties.isStartingStyle == IsStartingStyle::Yes)
        m_result->hasStartingStyle = true;

    if (matchedProperties.isCacheable == IsCacheable::Partially && !m_result->isCompletelyNonCacheable) {
        for (auto property : matchedProperties.properties.get())
            m_result->nonCacheablePropertyIds.append(property.id());
    }
    if (matchedProperties.isCacheable == IsCacheable::No) {
        m_result->isCompletelyNonCacheable = true;
        m_result->nonCacheablePropertyIds.clear();
    }

    declarations.append(WTFMove(matchedProperties));
}

void ElementRuleCollector::addAuthorKeyframeRules(const StyleRuleKeyframe& keyframe)
{
    ASSERT(m_result->authorDeclarations.isEmpty());
    m_result->authorDeclarations.append({ keyframe.properties(), SelectorChecker::MatchAll, propertyAllowlistForPseudoId(m_pseudoElementRequest ? m_pseudoElementRequest->pseudoId() : PseudoId::None) });
}

}
} // namespace WebCore
