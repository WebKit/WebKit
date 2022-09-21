/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
#include "CSSValueKeywords.h"
#include "CascadeLevel.h"
#include "ContainerQueryEvaluator.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "HTMLElement.h"
#include "HTMLSlotElement.h"
#include "SVGElement.h"
#include "SelectorCheckerTestFunctions.h"
#include "SelectorCompiler.h"
#include "SelectorMatchingState.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#include "StyledElement.h"
#include "UserAgentStyle.h"
#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static const StyleProperties& leftToRightDeclaration()
{
    static auto& declaration = [] () -> const StyleProperties& {
        auto properties = MutableStyleProperties::create();
        properties->setProperty(CSSPropertyDirection, CSSValueLtr);
        return properties.leakRef();
    }();
    return declaration;
}

static const StyleProperties& rightToLeftDeclaration()
{
    static auto& declaration = [] () -> const StyleProperties& {
        auto properties = MutableStyleProperties::create();
        properties->setProperty(CSSPropertyDirection, CSSValueRtl);
        return properties.leakRef();
    }();
    return declaration;
}

class MatchRequest {
public:
    MatchRequest(const RuleSet& ruleSet, ScopeOrdinal styleScopeOrdinal = ScopeOrdinal::Element)
        : ruleSet(ruleSet)
        , styleScopeOrdinal(styleScopeOrdinal)
    {
    }
    const RuleSet& ruleSet;
    ScopeOrdinal styleScopeOrdinal;
};

ElementRuleCollector::ElementRuleCollector(const Element& element, const ScopeRuleSets& ruleSets, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_authorStyle(ruleSets.authorStyle())
    , m_userStyle(ruleSets.userStyle())
    , m_userAgentMediaQueryStyle(ruleSets.userAgentMediaQueryStyle())
    , m_selectorMatchingState(selectorMatchingState)
{
    ASSERT(!m_selectorMatchingState || m_selectorMatchingState->selectorFilter.parentStackIsConsistent(element.parentNode()));
}

ElementRuleCollector::ElementRuleCollector(const Element& element, const RuleSet& authorStyle, SelectorMatchingState* selectorMatchingState)
    : m_element(element)
    , m_authorStyle(authorStyle)
    , m_selectorMatchingState(selectorMatchingState)
{
    ASSERT(!m_selectorMatchingState || m_selectorMatchingState->selectorFilter.parentStackIsConsistent(element.parentNode()));
}

const MatchResult& ElementRuleCollector::matchResult() const
{
    ASSERT(m_mode == SelectorChecker::Mode::ResolvingStyle);
    return m_result;
}

const Vector<RefPtr<const StyleRule>>& ElementRuleCollector::matchedRuleList() const
{
    ASSERT(m_mode == SelectorChecker::Mode::CollectingRules);
    return m_matchedRuleList;
}

inline void ElementRuleCollector::addMatchedRule(const RuleData& ruleData, unsigned specificity, const MatchRequest& matchRequest)
{
    auto cascadeLayerPriority = matchRequest.ruleSet.cascadeLayerPriorityFor(ruleData);
    m_matchedRules.append({ &ruleData, specificity, matchRequest.styleScopeOrdinal, cascadeLayerPriority });
}

void ElementRuleCollector::clearMatchedRules()
{
    m_matchedRules.clear();
    m_matchedRuleTransferIndex = 0;
}

inline void ElementRuleCollector::addElementStyleProperties(const StyleProperties* propertySet, CascadeLayerPriority priority, bool isCacheable, FromStyleAttribute fromStyleAttribute)
{
    if (!propertySet || propertySet->isEmpty())
        return;

    if (!isCacheable)
        m_result.isCacheable = false;

    auto matchedProperty = MatchedProperties { propertySet };
    matchedProperty.cascadeLayerPriority = priority;
    matchedProperty.fromStyleAttribute = fromStyleAttribute;
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
        matchShadowPseudoElementRules(level);
        matchPartPseudoElementRules(level);
    }
}

void ElementRuleCollector::collectMatchingRules(const MatchRequest& matchRequest)
{
    ASSERT_WITH_MESSAGE(!(m_mode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && m_pseudoElementRequest.pseudoId != PseudoId::None), "When in StyleInvalidation or SharingRules, SelectorChecker does not try to match the pseudo ID. While ElementRuleCollector supports matching a particular pseudoId in this case, this would indicate a error at the call site since matching a particular element should be unnecessary.");

    auto& element = this->element();
    auto* shadowRoot = element.containingShadowRoot();
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        collectMatchingShadowPseudoElementRules(matchRequest);

    bool isHTML = element.isHTMLElement() && element.document().isHTMLDocument();

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    auto& id = element.idForStyleResolution();
    if (!id.isNull())
        collectMatchingRulesForList(matchRequest.ruleSet.idRules(id), matchRequest);
    if (element.hasClass()) {
        for (size_t i = 0; i < element.classNames().size(); ++i)
            collectMatchingRulesForList(matchRequest.ruleSet.classRules(element.classNames()[i]), matchRequest);
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
    if (element.isLink())
        collectMatchingRulesForList(matchRequest.ruleSet.linkPseudoClassRules(), matchRequest);
    if (matchesFocusPseudoClass(element))
        collectMatchingRulesForList(matchRequest.ruleSet.focusPseudoClassRules(), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet.tagRules(element.localName(), isHTML), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet.universalRules(), matchRequest);
}


Vector<MatchedProperties>& ElementRuleCollector::declarationsForOrigin(MatchResult& matchResult, DeclarationOrigin declarationOrigin)
{
    switch (declarationOrigin) {
    case DeclarationOrigin::UserAgent: return matchResult.userAgentDeclarations;
    case DeclarationOrigin::User: return matchResult.userDeclarations;
    case DeclarationOrigin::Author: return matchResult.authorDeclarations;
    }
    ASSERT_NOT_REACHED();
    return matchResult.authorDeclarations;
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
        declarationsForOrigin(m_result, declarationOrigin).reserveCapacity(m_matchedRules.size());

    for (; m_matchedRuleTransferIndex < m_matchedRules.size(); ++m_matchedRuleTransferIndex) {
        auto& matchedRule = m_matchedRules[m_matchedRuleTransferIndex];
        if (fromScope && matchedRule.styleScopeOrdinal < *fromScope)
            break;

        if (m_mode == SelectorChecker::Mode::CollectingRules) {
            m_matchedRuleList.append(&matchedRule.ruleData->styleRule());
            continue;
        }

        addMatchedProperties({
            &matchedRule.ruleData->styleRule().properties(),
            static_cast<uint8_t>(matchedRule.ruleData->linkMatchType()),
            matchedRule.ruleData->propertyAllowlist(),
            matchedRule.styleScopeOrdinal,
            FromStyleAttribute::No,
            matchedRule.cascadeLayerPriority
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

void ElementRuleCollector::matchShadowPseudoElementRules(CascadeLevel level)
{
    ASSERT(element().isInShadowTree());
    auto& shadowRoot = *element().containingShadowRoot();
    if (shadowRoot.mode() != ShadowRootMode::UserAgent)
        return;

    // Look up shadow pseudo elements also from the host scope style as they are web-exposed.
    auto* hostRules = Scope::forNode(*shadowRoot.host()).resolver().ruleSets().styleForCascadeLevel(level);
    if (!hostRules)
        return;

    MatchRequest hostRequest { *hostRules, ScopeOrdinal::ContainingHost };
    collectMatchingShadowPseudoElementRules(hostRequest);
}

void ElementRuleCollector::matchHostPseudoClassRules(CascadeLevel level)
{
    ASSERT(element().shadowRoot());

    auto* shadowRules = element().shadowRoot()->styleScope().resolver().ruleSets().styleForCascadeLevel(level);
    if (!shadowRules)
        return;

    auto& shadowHostRules = shadowRules->hostPseudoClassRules();
    if (shadowHostRules.isEmpty())
        return;

    MatchRequest hostMatchRequest { *shadowRules, ScopeOrdinal::Shadow };
    collectMatchingRulesForList(&shadowHostRules, hostMatchRequest);
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

    bool isUAShadowPseudoElement = element().containingShadowRoot()->mode() == ShadowRootMode::UserAgent && !element().shadowPseudoId().isNull();

    auto& partMatchingElement = isUAShadowPseudoElement ? *element().shadowHost() : element();
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
        collectMatchingRulesForList(&hostRules->partPseudoElementRules(), scopeMatchRequest);

        // Element may only be exposed to styling from enclosing scopes via exportparts attributes.
        if (element != &partMatchingElement && element->shadowRoot()->partMappings().isEmpty())
            break;

        if (styleScopeOrdinal == ScopeOrdinal::ContainingHostLimit)
            break;
    }
}

void ElementRuleCollector::collectMatchingShadowPseudoElementRules(const MatchRequest& matchRequest)
{
    ASSERT(element().containingShadowRoot()->mode() == ShadowRootMode::UserAgent);

    auto& rules = matchRequest.ruleSet;
#if ENABLE(VIDEO)
    // FXIME: WebVTT should not be done by styling UA shadow trees like this.
    if (element().isWebVTTElement())
        collectMatchingRulesForList(&rules.cuePseudoRules(), matchRequest);
#endif
    auto& pseudoId = element().shadowPseudoId();
    if (!pseudoId.isEmpty())
        collectMatchingRulesForList(rules.shadowPseudoElementRules(pseudoId), matchRequest);
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
}

void ElementRuleCollector::matchUARules(const RuleSet& rules)
{
    clearMatchedRules();
    
    collectMatchingRules(MatchRequest(rules));

    sortAndTransferMatchedRules(DeclarationOrigin::UserAgent);
}

inline bool ElementRuleCollector::ruleMatches(const RuleData& ruleData, unsigned& specificity, ScopeOrdinal styleScopeOrdinal)
{
    // We know a sufficiently simple single part selector matches simply because we found it from the rule hash when filtering the RuleSet.
    // This is limited to HTML only so we don't need to check the namespace (because of tag name match).
    auto matchBasedOnRuleHash = ruleData.matchBasedOnRuleHash();
    if (matchBasedOnRuleHash != MatchBasedOnRuleHash::None && element().isHTMLElement()) {
        ASSERT_WITH_MESSAGE(m_pseudoElementRequest.pseudoId == PseudoId::None, "If we match based on the rule hash while collecting for a particular pseudo element ID, we would add incorrect rules for that pseudo element ID. We should never end in ruleMatches() with a pseudo element if the ruleData cannot match any pseudo element.");

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
        ASSERT_WITH_MESSAGE(!SelectorCompiler::ruleCollectorSimpleSelectorChecker(compiledSelector, &element(), &ignoreSpecificity) || m_pseudoElementRequest.pseudoId == PseudoId::None, "When matching pseudo elements, we should never compile a selector checker without context unless it cannot match anything.");
#endif
        bool selectorMatches = SelectorCompiler::ruleCollectorSimpleSelectorChecker(compiledSelector, &element(), &specificity);

        if (selectorMatches && ruleData.containsUncommonAttributeSelector())
            m_didMatchUncommonAttributeSelector = true;

        return selectorMatches;
    }
#endif // ENABLE(CSS_SELECTOR_JIT)

    SelectorChecker::CheckingContext context(m_mode);
    context.pseudoId = m_pseudoElementRequest.pseudoId;
    context.scrollbarState = m_pseudoElementRequest.scrollbarState;
    context.nameForHightlightPseudoElement = m_pseudoElementRequest.highlightName;
    context.styleScopeOrdinal = styleScopeOrdinal;
    context.selectorMatchingState = m_selectorMatchingState;
    
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

    for (unsigned i = 0, size = rules->size(); i < size; ++i) {
        const auto& ruleData = rules->data()[i];

        if (UNLIKELY(!ruleData.isEnabled()))
            continue;

        if (!ruleData.canMatchPseudoElement() && m_pseudoElementRequest.pseudoId != PseudoId::None)
            continue;

        if (m_selectorMatchingState && m_selectorMatchingState->selectorFilter.fastRejectSelector(ruleData.descendantSelectorIdentifierHashes()))
            continue;

        if (matchRequest.ruleSet.hasContainerQueries() && !containerQueriesMatch(ruleData, matchRequest))
            continue;

        auto& rule = ruleData.styleRule();

        // If the rule has no properties to apply, then ignore it in the non-debug mode.
        // Note that if we get null back here, it means we have a rule with deferred properties,
        // and that means we always have to consider it.
        if (rule.properties().isEmpty() && !m_shouldIncludeEmptyRules)
            continue;

        unsigned specificity;
        if (ruleMatches(ruleData, specificity, matchRequest.styleScopeOrdinal))
            addMatchedRule(ruleData, specificity, matchRequest);
    }
}

bool ElementRuleCollector::containerQueriesMatch(const RuleData& ruleData, const MatchRequest& matchRequest)
{
    auto queries = matchRequest.ruleSet.containerQueriesFor(ruleData);

    if (queries.isEmpty())
        return true;

    // Style bits indicating which pseudo-elements match are set during regular element matching. Container queries need to be evaluate in the right mode.
    auto selectionMode = ruleData.canMatchPseudoElement() ? ContainerQueryEvaluator::SelectionMode::PseudoElement : ContainerQueryEvaluator::SelectionMode::Element;

    // "Style rules defined on an element inside multiple nested container queries apply when all of the wrapping container queries are true for that element."
    ContainerQueryEvaluator evaluator(element(), selectionMode, matchRequest.styleScopeOrdinal, m_selectorMatchingState);
    for (auto* query : queries) {
        if (!evaluator.evaluate(*query))
            return false;
    }
    return true;
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

    if (is<StyledElement>(element())) {
        auto& styledElement = downcast<StyledElement>(element());
        // https://html.spec.whatwg.org/#presentational-hints
        addElementStyleProperties(styledElement.presentationalHintStyle(), RuleSet::cascadeLayerPriorityForPresentationalHints);

        // Tables and table cells share an additional presentation style that must be applied
        // after all attributes, since their style depends on the values of multiple attributes.
        addElementStyleProperties(styledElement.additionalPresentationalHintStyle(), RuleSet::cascadeLayerPriorityForPresentationalHints);

        if (is<HTMLElement>(styledElement)) {
            auto result = downcast<HTMLElement>(styledElement).directionalityIfDirIsAuto();
            auto& properties = result.value_or(TextDirection::LTR) == TextDirection::LTR ? leftToRightDeclaration() : rightToLeftDeclaration();
            if (result)
                addMatchedProperties({ &properties }, DeclarationOrigin::Author);
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
    if (!is<StyledElement>(element()))
        return;

    if (auto* inlineStyle = downcast<StyledElement>(element()).inlineStyle()) {
        // FIXME: Media control shadow trees seem to have problems with caching.
        bool isInlineStyleCacheable = !inlineStyle->isMutable() && !element().isInShadowTree();
        addElementStyleProperties(inlineStyle, RuleSet::cascadeLayerPriorityForUnlayered, isInlineStyleCacheable, FromStyleAttribute::Yes);
    }

    if (includeSMILProperties && is<SVGElement>(element()))
        addElementStyleProperties(downcast<SVGElement>(element()).animatedSMILStyleProperties(), RuleSet::cascadeLayerPriorityForUnlayered, false /* isCacheable */);
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
    // FIXME: This should be moved to the matched properties cache code.
    auto computeIsCacheable = [&] {
        if (!m_result.isCacheable)
            return false;

        if (matchedProperties.styleScopeOrdinal != ScopeOrdinal::Element)
            return false;

        auto& properties = *matchedProperties.properties;
        for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
            // Currently the property cache only copy the non-inherited values and resolve
            // the inherited ones.
            // Here we define some exception were we have to resolve some properties that are not inherited
            // by default. If those exceptions become too common on the web, it should be possible
            // to build a list of exception to resolve instead of completely disabling the cache.
            StyleProperties::PropertyReference current = properties.propertyAt(i);
            if (current.isInherited())
                continue;

            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            const CSSValue& value = *current.value();
            if (value.isInheritValue())
                return false;

            // The value currentColor has implicitely the same side effect. It depends on the value of color,
            // which is an inherited value, making the non-inherited property implicitly inherited.
            if (is<CSSPrimitiveValue>(value) && StyleColor::isCurrentColor(downcast<CSSPrimitiveValue>(value)))
                return false;

            if (value.hasVariableReferences())
                return false;
        }

        return true;
    };

    m_result.isCacheable = computeIsCacheable();

    declarationsForOrigin(m_result, declarationOrigin).append(WTFMove(matchedProperties));
}

void ElementRuleCollector::addAuthorKeyframeRules(const StyleRuleKeyframe& keyframe)
{
    ASSERT(m_result.authorDeclarations.isEmpty());
    m_result.authorDeclarations.append({ &keyframe.properties(), SelectorChecker::MatchAll, propertyAllowlistForPseudoId(m_pseudoElementRequest.pseudoId) });
}

}
} // namespace WebCore
