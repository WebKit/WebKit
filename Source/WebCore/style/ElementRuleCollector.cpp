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

#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSValueKeywords.h"
#include "HTMLElement.h"
#include "HTMLSlotElement.h"
#include "SVGElement.h"
#include "SelectorCompiler.h"
#include "SelectorFilter.h"
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
    MatchRequest(const RuleSet* ruleSet, ScopeOrdinal styleScopeOrdinal = ScopeOrdinal::Element)
        : ruleSet(ruleSet)
        , styleScopeOrdinal(styleScopeOrdinal)
    {
    }
    const RuleSet* ruleSet;
    ScopeOrdinal styleScopeOrdinal;
};

ElementRuleCollector::ElementRuleCollector(const Element& element, const ScopeRuleSets& ruleSets, const SelectorFilter* selectorFilter)
    : m_element(element)
    , m_authorStyle(ruleSets.authorStyle())
    , m_userStyle(ruleSets.userStyle())
    , m_userAgentMediaQueryStyle(ruleSets.userAgentMediaQueryStyle())
    , m_selectorFilter(selectorFilter)
{
    ASSERT(!m_selectorFilter || m_selectorFilter->parentStackIsConsistent(element.parentNode()));
}

ElementRuleCollector::ElementRuleCollector(const Element& element, const RuleSet& authorStyle, const SelectorFilter* selectorFilter)
    : m_element(element)
    , m_authorStyle(authorStyle)
    , m_selectorFilter(selectorFilter)
{
    ASSERT(!m_selectorFilter || m_selectorFilter->parentStackIsConsistent(element.parentNode()));
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

inline void ElementRuleCollector::addMatchedRule(const RuleData& ruleData, unsigned specificity, ScopeOrdinal styleScopeOrdinal)
{
    m_matchedRules.append({ &ruleData, specificity, styleScopeOrdinal });
}

void ElementRuleCollector::clearMatchedRules()
{
    m_matchedRules.clear();
    m_keepAliveSlottedPseudoElementRules.clear();
    m_matchedRuleTransferIndex = 0;
}

inline void ElementRuleCollector::addElementStyleProperties(const StyleProperties* propertySet, bool isCacheable)
{
    if (!propertySet || propertySet->isEmpty())
        return;

    if (!isCacheable)
        m_result.isCacheable = false;

    addMatchedProperties({ propertySet }, DeclarationOrigin::Author);
}

void ElementRuleCollector::collectMatchingRules(const MatchRequest& matchRequest)
{
    ASSERT(matchRequest.ruleSet);
    ASSERT_WITH_MESSAGE(!(m_mode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && m_pseudoElementRequest.pseudoId != PseudoId::None), "When in StyleInvalidation or SharingRules, SelectorChecker does not try to match the pseudo ID. While ElementRuleCollector supports matching a particular pseudoId in this case, this would indicate a error at the call site since matching a particular element should be unnecessary.");

    auto* shadowRoot = element().containingShadowRoot();
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        collectMatchingShadowPseudoElementRules(matchRequest);

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    auto& id = element().idForStyleResolution();
    if (!id.isNull())
        collectMatchingRulesForList(matchRequest.ruleSet->idRules(id), matchRequest);
    if (element().hasClass()) {
        for (size_t i = 0; i < element().classNames().size(); ++i)
            collectMatchingRulesForList(matchRequest.ruleSet->classRules(element().classNames()[i]), matchRequest);
    }

    if (element().isLink())
        collectMatchingRulesForList(matchRequest.ruleSet->linkPseudoClassRules(), matchRequest);
    if (SelectorChecker::matchesFocusPseudoClass(element()))
        collectMatchingRulesForList(matchRequest.ruleSet->focusPseudoClassRules(), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet->tagRules(element().localName(), element().isHTMLElement() && element().document().isHTMLDocument()), matchRequest);
    collectMatchingRulesForList(matchRequest.ruleSet->universalRules(), matchRequest);
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

void ElementRuleCollector::transferMatchedRules(DeclarationOrigin declarationOrigin, Optional<ScopeOrdinal> fromScope)
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
            static_cast<uint16_t>(matchedRule.ruleData->linkMatchType()),
            static_cast<uint16_t>(matchedRule.ruleData->propertyWhitelistType()),
            matchedRule.styleScopeOrdinal
        }, declarationOrigin);
    }
}

void ElementRuleCollector::matchAuthorRules()
{
    clearMatchedRules();

    collectMatchingAuthorRules();

    sortAndTransferMatchedRules(DeclarationOrigin::Author);
}

bool ElementRuleCollector::matchesAnyAuthorRules()
{
    clearMatchedRules();

    // FIXME: This should bail out on first match.
    collectMatchingAuthorRules();

    return !m_matchedRules.isEmpty();
}

void ElementRuleCollector::collectMatchingAuthorRules()
{
    {
        MatchRequest matchRequest(m_authorStyle.ptr());
        collectMatchingRules(matchRequest);
    }

    auto* parent = element().parentElement();
    if (parent && parent->shadowRoot())
        matchSlottedPseudoElementRules();

    if (element().shadowRoot())
        matchHostPseudoClassRules();

    if (element().isInShadowTree()) {
        matchAuthorShadowPseudoElementRules();
        matchPartPseudoElementRules();
    }
}

void ElementRuleCollector::matchAuthorShadowPseudoElementRules()
{
    ASSERT(element().isInShadowTree());
    auto& shadowRoot = *element().containingShadowRoot();
    if (shadowRoot.mode() != ShadowRootMode::UserAgent)
        return;
    // Look up shadow pseudo elements also from the host scope author style as they are web-exposed.
    auto& hostAuthorRules = Scope::forNode(*shadowRoot.host()).resolver().ruleSets().authorStyle();
    MatchRequest hostAuthorRequest { &hostAuthorRules, ScopeOrdinal::ContainingHost };
    collectMatchingShadowPseudoElementRules(hostAuthorRequest);
}

void ElementRuleCollector::matchHostPseudoClassRules()
{
    ASSERT(element().shadowRoot());

    auto& shadowAuthorStyle = element().shadowRoot()->styleScope().resolver().ruleSets().authorStyle();
    auto& shadowHostRules = shadowAuthorStyle.hostPseudoClassRules();
    if (shadowHostRules.isEmpty())
        return;

    SetForScope<bool> change(m_isMatchingHostPseudoClass, true);

    MatchRequest hostMatchRequest { nullptr, ScopeOrdinal::Shadow };
    collectMatchingRulesForList(&shadowHostRules, hostMatchRequest);
}

void ElementRuleCollector::matchSlottedPseudoElementRules()
{
    auto* slot = element().assignedSlot();
    auto styleScopeOrdinal = ScopeOrdinal::FirstSlot;

    for (; slot; slot = slot->assignedSlot(), ++styleScopeOrdinal) {
        auto& styleScope = Scope::forNode(*slot);
        if (!styleScope.resolver().ruleSets().isAuthorStyleDefined())
            continue;
        // Find out if there are any ::slotted rules in the shadow tree matching the current slot.
        // FIXME: This is really part of the slot style and could be cached when resolving it.
        ElementRuleCollector collector(*slot, styleScope.resolver().ruleSets().authorStyle(), nullptr);
        auto slottedPseudoElementRules = collector.collectSlottedPseudoElementRulesForSlot();
        if (!slottedPseudoElementRules)
            continue;
        // Match in the current scope.
        SetForScope<bool> change(m_isMatchingSlottedPseudoElements, true);

        MatchRequest scopeMatchRequest(nullptr, styleScopeOrdinal);
        collectMatchingRulesForList(slottedPseudoElementRules.get(), scopeMatchRequest);

        m_keepAliveSlottedPseudoElementRules.append(WTFMove(slottedPseudoElementRules));
    }
}

void ElementRuleCollector::matchPartPseudoElementRules()
{
    ASSERT(element().isInShadowTree());

    bool isUAShadowPseudoElement = element().containingShadowRoot()->mode() == ShadowRootMode::UserAgent && !element().shadowPseudoId().isNull();

    auto& partMatchingElement = isUAShadowPseudoElement ? *element().shadowHost() : element();
    if (partMatchingElement.partNames().isEmpty() || !partMatchingElement.isInShadowTree())
        return;

    matchPartPseudoElementRulesForScope(*partMatchingElement.containingShadowRoot());
}

void ElementRuleCollector::matchPartPseudoElementRulesForScope(const ShadowRoot& scopeShadowRoot)
{
    auto& shadowHost = *scopeShadowRoot.host();
    {
        SetForScope<RefPtr<const Element>> partMatchingScope(m_shadowHostInPartRuleScope, &shadowHost);

        auto& hostAuthorRules = Scope::forNode(shadowHost).resolver().ruleSets().authorStyle();
        MatchRequest hostAuthorRequest { &hostAuthorRules, ScopeOrdinal::ContainingHost };
        collectMatchingRulesForList(&hostAuthorRules.partPseudoElementRules(), hostAuthorRequest);
    }

    // Element may be exposed to styling from enclosing scopes via exportparts attributes.
    if (scopeShadowRoot.partMappings().isEmpty())
        return;

    if (auto* parentScopeShadowRoot = shadowHost.containingShadowRoot())
        matchPartPseudoElementRulesForScope(*parentScopeShadowRoot);
}

void ElementRuleCollector::collectMatchingShadowPseudoElementRules(const MatchRequest& matchRequest)
{
    ASSERT(matchRequest.ruleSet);
    ASSERT(element().containingShadowRoot()->mode() == ShadowRootMode::UserAgent);

    auto& rules = *matchRequest.ruleSet;
#if ENABLE(VIDEO_TRACK)
    // FXIME: WebVTT should not be done by styling UA shadow trees like this.
    if (element().isWebVTTElement())
        collectMatchingRulesForList(rules.cuePseudoRules(), matchRequest);
#endif
    auto& pseudoId = element().shadowPseudoId();
    if (!pseudoId.isEmpty())
        collectMatchingRulesForList(rules.shadowPseudoElementRules(pseudoId), matchRequest);
}

std::unique_ptr<RuleSet::RuleDataVector> ElementRuleCollector::collectSlottedPseudoElementRulesForSlot()
{
    ASSERT(is<HTMLSlotElement>(element()));

    clearMatchedRules();

    m_mode = SelectorChecker::Mode::CollectingRules;

    // Match global author rules.
    MatchRequest matchRequest(m_authorStyle.ptr());
    collectMatchingRulesForList(&m_authorStyle->slottedPseudoElementRules(), matchRequest);

    if (m_matchedRules.isEmpty())
        return { };

    auto ruleDataVector = makeUnique<RuleSet::RuleDataVector>();
    ruleDataVector->reserveInitialCapacity(m_matchedRules.size());
    for (auto& matchedRule : m_matchedRules)
        ruleDataVector->uncheckedAppend(*matchedRule.ruleData);

    return ruleDataVector;
}

void ElementRuleCollector::matchUserRules()
{
    if (!m_userStyle)
        return;
    
    clearMatchedRules();

    MatchRequest matchRequest(m_userStyle.get());
    collectMatchingRules(matchRequest);

    sortAndTransferMatchedRules(DeclarationOrigin::User);
}

void ElementRuleCollector::matchUARules()
{
    // First we match rules from the user agent sheet.
    if (UserAgentStyle::simpleDefaultStyleSheet)
        m_result.isCacheable = false;
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
    
    collectMatchingRules(MatchRequest(&rules));

    sortAndTransferMatchedRules(DeclarationOrigin::UserAgent);
}

static const CSSSelector* findSlottedPseudoElementSelector(const CSSSelector* selector)
{
    for (; selector; selector = selector->tagHistory()) {
        if (selector->match() == CSSSelector::PseudoElement && selector->pseudoElementType() == CSSSelector::PseudoElementSlotted) {
            if (auto* list = selector->selectorList())
                return list->first();
            break;
        }
    };
    return nullptr;
}

inline bool ElementRuleCollector::ruleMatches(const RuleData& ruleData, unsigned& specificity)
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

        auto selectorChecker = SelectorCompiler::ruleCollectorSimpleSelectorCheckerFunction(compiledSelector);
#if !ASSERT_MSG_DISABLED
        unsigned ignoreSpecificity;
        ASSERT_WITH_MESSAGE(!selectorChecker(&element(), &ignoreSpecificity) || m_pseudoElementRequest.pseudoId == PseudoId::None, "When matching pseudo elements, we should never compile a selector checker without context unless it cannot match anything.");
#endif
        bool selectorMatches = selectorChecker(&element(), &specificity);

        if (selectorMatches && ruleData.containsUncommonAttributeSelector())
            m_didMatchUncommonAttributeSelector = true;

        return selectorMatches;
    }
#endif // ENABLE(CSS_SELECTOR_JIT)

    SelectorChecker::CheckingContext context(m_mode);
    context.pseudoId = m_pseudoElementRequest.pseudoId;
    context.scrollbarState = m_pseudoElementRequest.scrollbarState;
    context.nameForHightlightPseudoElement = m_pseudoElementRequest.highlightName;
    context.isMatchingHostPseudoClass = m_isMatchingHostPseudoClass;
    context.shadowHostInPartRuleScope = m_shadowHostInPartRuleScope.get();

    bool selectorMatches;
#if ENABLE(CSS_SELECTOR_JIT)
    if (compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext) {
        compiledSelector.wasUsed();

        auto selectorChecker = SelectorCompiler::ruleCollectorSelectorCheckerFunctionWithCheckingContext(compiledSelector);
        selectorMatches = selectorChecker(&element(), &context, &specificity);
    } else
#endif // ENABLE(CSS_SELECTOR_JIT)
    {
        auto* selector = ruleData.selector();
        if (m_isMatchingSlottedPseudoElements) {
            selector = findSlottedPseudoElementSelector(ruleData.selector());
            if (!selector)
                return false;
        }
        // Slow path.
        SelectorChecker selectorChecker(element().document());
        selectorMatches = selectorChecker.match(*selector, element(), context, specificity);
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

        if (m_selectorFilter && m_selectorFilter->fastRejectSelector(ruleData.descendantSelectorIdentifierHashes()))
            continue;

        auto& rule = ruleData.styleRule();

        // If the rule has no properties to apply, then ignore it in the non-debug mode.
        // Note that if we get null back here, it means we have a rule with deferred properties,
        // and that means we always have to consider it.
        const StyleProperties* properties = rule.propertiesWithoutDeferredParsing();
        if (properties && properties->isEmpty() && !m_shouldIncludeEmptyRules)
            continue;

        unsigned specificity;
        if (ruleMatches(ruleData, specificity))
            addMatchedRule(ruleData, specificity, matchRequest.styleScopeOrdinal);
    }
}

static inline bool compareRules(MatchedRule r1, MatchedRule r2)
{
    // For normal properties the earlier scope wins. This may be reversed by !important which is handled when resolving cascade.
    if (r1.styleScopeOrdinal != r2.styleScopeOrdinal)
        return r1.styleScopeOrdinal > r2.styleScopeOrdinal;

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

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (is<StyledElement>(element())) {
        auto& styledElement = downcast<StyledElement>(element());
        addElementStyleProperties(styledElement.presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        addElementStyleProperties(styledElement.additionalPresentationAttributeStyle());

        if (is<HTMLElement>(styledElement)) {
            bool isAuto;
            auto textDirection = downcast<HTMLElement>(styledElement).directionalityIfhasDirAutoAttribute(isAuto);
            auto& properties = textDirection == TextDirection::LTR ? leftToRightDeclaration() : rightToLeftDeclaration();
            if (isAuto)
                addMatchedProperties({ &properties }, DeclarationOrigin::Author);
        }
    }
    
    if (matchAuthorAndUserStyles) {
        clearMatchedRules();

        collectMatchingAuthorRules();
        sortMatchedRules();

        transferMatchedRules(DeclarationOrigin::Author, ScopeOrdinal::Element);

        // Inline style behaves as if it has higher specificity than any rule.
        addElementInlineStyleProperties(includeSMILProperties);

        // Rules from the host scope override inline style.
        transferMatchedRules(DeclarationOrigin::Author, ScopeOrdinal::ContainingHost);
    }
}

void ElementRuleCollector::addElementInlineStyleProperties(bool includeSMILProperties)
{
    if (!is<StyledElement>(element()))
        return;

    if (auto* inlineStyle = downcast<StyledElement>(element()).inlineStyle()) {
        // FIXME: Media control shadow trees seem to have problems with caching.
        bool isInlineStyleCacheable = !inlineStyle->isMutable() && !element().isInShadowTree();
        addElementStyleProperties(inlineStyle, isInlineStyleCacheable);
    }

    if (includeSMILProperties && is<SVGElement>(element()))
        addElementStyleProperties(downcast<SVGElement>(element()).animatedSMILStyleProperties(), false /* isCacheable */);
}

bool ElementRuleCollector::hasAnyMatchingRules(const RuleSet* ruleSet)
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
            if (value.isInheritedValue())
                return false;

            // The value currentColor has implicitely the same side effect. It depends on the value of color,
            // which is an inherited value, making the non-inherited property implicitly inherited.
            if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueCurrentcolor)
                return false;

            if (value.hasVariableReferences())
                return false;
        }

        return true;
    };

    m_result.isCacheable = computeIsCacheable();

    declarationsForOrigin(m_result, declarationOrigin).append(WTFMove(matchedProperties));
}

}
} // namespace WebCore
