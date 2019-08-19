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

#include "CSSDefaultStyleSheets.h"
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
#include "StyleScope.h"
#include "StyledElement.h"
#include <wtf/SetForScope.h>

namespace WebCore {

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
    MatchRequest(const RuleSet* ruleSet, bool includeEmptyRules = false, Style::ScopeOrdinal styleScopeOrdinal = Style::ScopeOrdinal::Element)
        : ruleSet(ruleSet)
        , includeEmptyRules(includeEmptyRules)
        , styleScopeOrdinal(styleScopeOrdinal)
    {
    }
    const RuleSet* ruleSet;
    const bool includeEmptyRules;
    Style::ScopeOrdinal styleScopeOrdinal;
};

ElementRuleCollector::ElementRuleCollector(const Element& element, const DocumentRuleSets& ruleSets, const SelectorFilter* selectorFilter)
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

StyleResolver::MatchResult& ElementRuleCollector::matchedResult()
{
    ASSERT(m_mode == SelectorChecker::Mode::ResolvingStyle);
    return m_result;
}

const Vector<RefPtr<StyleRule>>& ElementRuleCollector::matchedRuleList() const
{
    ASSERT(m_mode == SelectorChecker::Mode::CollectingRules);
    return m_matchedRuleList;
}

inline void ElementRuleCollector::addMatchedRule(const RuleData& ruleData, unsigned specificity, Style::ScopeOrdinal styleScopeOrdinal, StyleResolver::RuleRange& ruleRange)
{
    // Update our first/last rule indices in the matched rules array.
    ++ruleRange.lastRuleIndex;
    if (ruleRange.firstRuleIndex == -1)
        ruleRange.firstRuleIndex = ruleRange.lastRuleIndex;

    m_matchedRules.append({ &ruleData, specificity, styleScopeOrdinal });
}

void ElementRuleCollector::clearMatchedRules()
{
    m_matchedRules.clear();
    m_keepAliveSlottedPseudoElementRules.clear();
}

inline void ElementRuleCollector::addElementStyleProperties(const StyleProperties* propertySet, bool isCacheable)
{
    if (!propertySet)
        return;
    m_result.ranges.lastAuthorRule = m_result.matchedProperties().size();
    if (m_result.ranges.firstAuthorRule == -1)
        m_result.ranges.firstAuthorRule = m_result.ranges.lastAuthorRule;
    m_result.addMatchedProperties(*propertySet);
    if (!isCacheable)
        m_result.isCacheable = false;
}

void ElementRuleCollector::collectMatchingRules(const MatchRequest& matchRequest, StyleResolver::RuleRange& ruleRange)
{
    ASSERT(matchRequest.ruleSet);
    ASSERT_WITH_MESSAGE(!(m_mode == SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements && m_pseudoStyleRequest.pseudoId != PseudoId::None), "When in StyleInvalidation or SharingRules, SelectorChecker does not try to match the pseudo ID. While ElementRuleCollector supports matching a particular pseudoId in this case, this would indicate a error at the call site since matching a particular element should be unnecessary.");

    auto* shadowRoot = m_element.containingShadowRoot();
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent)
        collectMatchingShadowPseudoElementRules(matchRequest, ruleRange);

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    auto& id = m_element.idForStyleResolution();
    if (!id.isNull())
        collectMatchingRulesForList(matchRequest.ruleSet->idRules(id), matchRequest, ruleRange);
    if (m_element.hasClass()) {
        for (size_t i = 0; i < m_element.classNames().size(); ++i)
            collectMatchingRulesForList(matchRequest.ruleSet->classRules(m_element.classNames()[i]), matchRequest, ruleRange);
    }

    if (m_element.isLink())
        collectMatchingRulesForList(matchRequest.ruleSet->linkPseudoClassRules(), matchRequest, ruleRange);
    if (SelectorChecker::matchesFocusPseudoClass(m_element))
        collectMatchingRulesForList(matchRequest.ruleSet->focusPseudoClassRules(), matchRequest, ruleRange);
    collectMatchingRulesForList(matchRequest.ruleSet->tagRules(m_element.localName(), m_element.isHTMLElement() && m_element.document().isHTMLDocument()), matchRequest, ruleRange);
    collectMatchingRulesForList(matchRequest.ruleSet->universalRules(), matchRequest, ruleRange);
}

void ElementRuleCollector::sortAndTransferMatchedRules()
{
    if (m_matchedRules.isEmpty())
        return;

    sortMatchedRules();

    if (m_mode == SelectorChecker::Mode::CollectingRules) {
        for (const MatchedRule& matchedRule : m_matchedRules)
            m_matchedRuleList.append(matchedRule.ruleData->rule());
        return;
    }

    for (const MatchedRule& matchedRule : m_matchedRules) {
        m_result.addMatchedProperties(matchedRule.ruleData->rule()->properties(), matchedRule.ruleData->rule(), matchedRule.ruleData->linkMatchType(), matchedRule.ruleData->propertyWhitelistType(), matchedRule.styleScopeOrdinal);
    }
}

void ElementRuleCollector::matchAuthorRules(bool includeEmptyRules)
{
    clearMatchedRules();

    m_result.ranges.lastAuthorRule = m_result.matchedProperties().size() - 1;
    StyleResolver::RuleRange ruleRange = m_result.ranges.authorRuleRange();

    {
        MatchRequest matchRequest(&m_authorStyle, includeEmptyRules);
        collectMatchingRules(matchRequest, ruleRange);
    }

    auto* parent = m_element.parentElement();
    if (parent && parent->shadowRoot())
        matchSlottedPseudoElementRules(includeEmptyRules, ruleRange);

    if (m_element.shadowRoot())
        matchHostPseudoClassRules(includeEmptyRules, ruleRange);

    if (m_element.isInShadowTree())
        matchAuthorShadowPseudoElementRules(includeEmptyRules, ruleRange);

    sortAndTransferMatchedRules();
}

void ElementRuleCollector::matchAuthorShadowPseudoElementRules(bool includeEmptyRules, StyleResolver::RuleRange& ruleRange)
{
    ASSERT(m_element.isInShadowTree());
    auto& shadowRoot = *m_element.containingShadowRoot();
    if (shadowRoot.mode() != ShadowRootMode::UserAgent)
        return;
    // Look up shadow pseudo elements also from the host scope author style as they are web-exposed.
    auto& hostAuthorRules = Style::Scope::forNode(*shadowRoot.host()).resolver().ruleSets().authorStyle();
    MatchRequest hostAuthorRequest { &hostAuthorRules, includeEmptyRules, Style::ScopeOrdinal::ContainingHost };
    collectMatchingShadowPseudoElementRules(hostAuthorRequest, ruleRange);
}

void ElementRuleCollector::matchHostPseudoClassRules(bool includeEmptyRules, StyleResolver::RuleRange& ruleRange)
{
    ASSERT(m_element.shadowRoot());

    auto& shadowAuthorStyle = m_element.shadowRoot()->styleScope().resolver().ruleSets().authorStyle();
    auto& shadowHostRules = shadowAuthorStyle.hostPseudoClassRules();
    if (shadowHostRules.isEmpty())
        return;

    SetForScope<bool> change(m_isMatchingHostPseudoClass, true);

    MatchRequest hostMatchRequest { nullptr, includeEmptyRules, Style::ScopeOrdinal::Shadow };
    collectMatchingRulesForList(&shadowHostRules, hostMatchRequest, ruleRange);
}

void ElementRuleCollector::matchSlottedPseudoElementRules(bool includeEmptyRules, StyleResolver::RuleRange& ruleRange)
{
    auto* slot = m_element.assignedSlot();
    auto styleScopeOrdinal = Style::ScopeOrdinal::FirstSlot;

    for (; slot; slot = slot->assignedSlot(), ++styleScopeOrdinal) {
        auto& styleScope = Style::Scope::forNode(*slot);
        if (!styleScope.resolver().ruleSets().isAuthorStyleDefined())
            continue;
        // Find out if there are any ::slotted rules in the shadow tree matching the current slot.
        // FIXME: This is really part of the slot style and could be cached when resolving it.
        ElementRuleCollector collector(*slot, styleScope.resolver().ruleSets().authorStyle(), nullptr);
        auto slottedPseudoElementRules = collector.collectSlottedPseudoElementRulesForSlot(includeEmptyRules);
        if (!slottedPseudoElementRules)
            continue;
        // Match in the current scope.
        SetForScope<bool> change(m_isMatchingSlottedPseudoElements, true);

        MatchRequest scopeMatchRequest(nullptr, includeEmptyRules, styleScopeOrdinal);
        collectMatchingRulesForList(slottedPseudoElementRules.get(), scopeMatchRequest, ruleRange);

        m_keepAliveSlottedPseudoElementRules.append(WTFMove(slottedPseudoElementRules));
    }
}

void ElementRuleCollector::collectMatchingShadowPseudoElementRules(const MatchRequest& matchRequest, StyleResolver::RuleRange& ruleRange)
{
    ASSERT(matchRequest.ruleSet);
    ASSERT(m_element.containingShadowRoot()->mode() == ShadowRootMode::UserAgent);

    auto& rules = *matchRequest.ruleSet;
#if ENABLE(VIDEO_TRACK)
    // FXIME: WebVTT should not be done by styling UA shadow trees like this.
    if (m_element.isWebVTTElement())
        collectMatchingRulesForList(rules.cuePseudoRules(), matchRequest, ruleRange);
#endif
    auto& pseudoId = m_element.shadowPseudoId();
    if (!pseudoId.isEmpty())
        collectMatchingRulesForList(rules.shadowPseudoElementRules(pseudoId), matchRequest, ruleRange);
}

std::unique_ptr<RuleSet::RuleDataVector> ElementRuleCollector::collectSlottedPseudoElementRulesForSlot(bool includeEmptyRules)
{
    ASSERT(is<HTMLSlotElement>(m_element));

    clearMatchedRules();

    m_mode = SelectorChecker::Mode::CollectingRules;

    // Match global author rules.
    MatchRequest matchRequest(&m_authorStyle, includeEmptyRules);
    StyleResolver::RuleRange ruleRange = m_result.ranges.authorRuleRange();
    collectMatchingRulesForList(&m_authorStyle.slottedPseudoElementRules(), matchRequest, ruleRange);

    if (m_matchedRules.isEmpty())
        return { };

    auto ruleDataVector = makeUnique<RuleSet::RuleDataVector>();
    ruleDataVector->reserveInitialCapacity(m_matchedRules.size());
    for (auto& matchedRule : m_matchedRules)
        ruleDataVector->uncheckedAppend(*matchedRule.ruleData);

    return ruleDataVector;
}

void ElementRuleCollector::matchUserRules(bool includeEmptyRules)
{
    if (!m_userStyle)
        return;
    
    clearMatchedRules();

    m_result.ranges.lastUserRule = m_result.matchedProperties().size() - 1;
    MatchRequest matchRequest(m_userStyle, includeEmptyRules);
    StyleResolver::RuleRange ruleRange = m_result.ranges.userRuleRange();
    collectMatchingRules(matchRequest, ruleRange);

    sortAndTransferMatchedRules();
}

void ElementRuleCollector::matchUARules()
{
    // First we match rules from the user agent sheet.
    if (CSSDefaultStyleSheets::simpleDefaultStyleSheet)
        m_result.isCacheable = false;
    RuleSet* userAgentStyleSheet = m_isPrintStyle
        ? CSSDefaultStyleSheets::defaultPrintStyle : CSSDefaultStyleSheets::defaultStyle;
    matchUARules(*userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (m_element.document().inQuirksMode())
        matchUARules(*CSSDefaultStyleSheets::defaultQuirksStyle);

    if (m_userAgentMediaQueryStyle)
        matchUARules(*m_userAgentMediaQueryStyle);
}

void ElementRuleCollector::matchUARules(const RuleSet& rules)
{
    clearMatchedRules();
    
    m_result.ranges.lastUARule = m_result.matchedProperties().size() - 1;
    StyleResolver::RuleRange ruleRange = m_result.ranges.UARuleRange();
    collectMatchingRules(MatchRequest(&rules), ruleRange);

    sortAndTransferMatchedRules();
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
    MatchBasedOnRuleHash matchBasedOnRuleHash = ruleData.matchBasedOnRuleHash();
    if (matchBasedOnRuleHash != MatchBasedOnRuleHash::None && m_element.isHTMLElement()) {
        ASSERT_WITH_MESSAGE(m_pseudoStyleRequest.pseudoId == PseudoId::None, "If we match based on the rule hash while collecting for a particular pseudo element ID, we would add incorrect rules for that pseudo element ID. We should never end in ruleMatches() with a pseudo element if the ruleData cannot match any pseudo element.");

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
    auto& compiledSelector = ruleData.rule()->compiledSelectorForListIndex(ruleData.selectorListIndex());
    void* compiledSelectorChecker = compiledSelector.codeRef.code().executableAddress();
    if (!compiledSelectorChecker && compiledSelector.status == SelectorCompilationStatus::NotCompiled) {
        compiledSelector.status = SelectorCompiler::compileSelector(ruleData.selector(), SelectorCompiler::SelectorContext::RuleCollector, compiledSelector.codeRef);

        compiledSelectorChecker = compiledSelector.codeRef.code().executableAddress();
    }

    if (compiledSelectorChecker && compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker) {
        auto selectorChecker = SelectorCompiler::ruleCollectorSimpleSelectorCheckerFunction(compiledSelectorChecker, compiledSelector.status);
#if !ASSERT_MSG_DISABLED
        unsigned ignoreSpecificity;
        ASSERT_WITH_MESSAGE(!selectorChecker(&m_element, &ignoreSpecificity) || m_pseudoStyleRequest.pseudoId == PseudoId::None, "When matching pseudo elements, we should never compile a selector checker without context unless it cannot match anything.");
#endif
#if CSS_SELECTOR_JIT_PROFILING
        ruleData.compiledSelectorUsed();
#endif
        bool selectorMatches = selectorChecker(&m_element, &specificity);

        if (selectorMatches && ruleData.containsUncommonAttributeSelector())
            m_didMatchUncommonAttributeSelector = true;

        return selectorMatches;
    }
#endif // ENABLE(CSS_SELECTOR_JIT)

    SelectorChecker::CheckingContext context(m_mode);
    context.pseudoId = m_pseudoStyleRequest.pseudoId;
    context.scrollbar = m_pseudoStyleRequest.scrollbar;
    context.scrollbarPart = m_pseudoStyleRequest.scrollbarPart;
    context.isMatchingHostPseudoClass = m_isMatchingHostPseudoClass;

    bool selectorMatches;
#if ENABLE(CSS_SELECTOR_JIT)
    if (compiledSelectorChecker) {
        ASSERT(compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);

        auto selectorChecker = SelectorCompiler::ruleCollectorSelectorCheckerFunctionWithCheckingContext(compiledSelectorChecker, compiledSelector.status);

#if CSS_SELECTOR_JIT_PROFILING
        compiledSelector.useCount++;
#endif
        selectorMatches = selectorChecker(&m_element, &context, &specificity);
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
        SelectorChecker selectorChecker(m_element.document());
        selectorMatches = selectorChecker.match(*selector, m_element, context, specificity);
    }

    if (ruleData.containsUncommonAttributeSelector()) {
        if (selectorMatches || context.pseudoIDSet)
            m_didMatchUncommonAttributeSelector = true;
    }
    m_matchedPseudoElementIds.merge(context.pseudoIDSet);
    m_styleRelations.appendVector(context.styleRelations);

    return selectorMatches;
}

void ElementRuleCollector::collectMatchingRulesForList(const RuleSet::RuleDataVector* rules, const MatchRequest& matchRequest, StyleResolver::RuleRange& ruleRange)
{
    if (!rules)
        return;

    for (unsigned i = 0, size = rules->size(); i < size; ++i) {
        const RuleData& ruleData = rules->data()[i];

        if (!ruleData.canMatchPseudoElement() && m_pseudoStyleRequest.pseudoId != PseudoId::None)
            continue;

        if (m_selectorFilter && m_selectorFilter->fastRejectSelector(ruleData.descendantSelectorIdentifierHashes()))
            continue;

        StyleRule* rule = ruleData.rule();

        // If the rule has no properties to apply, then ignore it in the non-debug mode.
        // Note that if we get null back here, it means we have a rule with deferred properties,
        // and that means we always have to consider it.
        const StyleProperties* properties = rule->propertiesWithoutDeferredParsing();
        if (properties && properties->isEmpty() && !matchRequest.includeEmptyRules)
            continue;

        unsigned specificity;
        if (ruleMatches(ruleData, specificity))
            addMatchedRule(ruleData, specificity, matchRequest.styleScopeOrdinal, ruleRange);
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
        matchUserRules(false);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (is<StyledElement>(m_element)) {
        auto& styledElement = downcast<StyledElement>(m_element);
        addElementStyleProperties(styledElement.presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        addElementStyleProperties(styledElement.additionalPresentationAttributeStyle());

        if (is<HTMLElement>(styledElement)) {
            bool isAuto;
            TextDirection textDirection = downcast<HTMLElement>(styledElement).directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                m_result.addMatchedProperties(textDirection == TextDirection::LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }
    
    // Check the rules in author sheets next.
    if (matchAuthorAndUserStyles)
        matchAuthorRules(false);

    if (matchAuthorAndUserStyles && is<StyledElement>(m_element)) {
        auto& styledElement = downcast<StyledElement>(m_element);
        // Now check our inline style attribute.
        if (styledElement.inlineStyle()) {
            // Inline style is immutable as long as there is no CSSOM wrapper.
            // FIXME: Media control shadow trees seem to have problems with caching.
            bool isInlineStyleCacheable = !styledElement.inlineStyle()->isMutable() && !styledElement.isInShadowTree();
            // FIXME: Constify.
            addElementStyleProperties(styledElement.inlineStyle(), isInlineStyleCacheable);
        }

        // Now check SMIL animation override style.
        if (includeSMILProperties && is<SVGElement>(styledElement))
            addElementStyleProperties(downcast<SVGElement>(styledElement).animatedSMILStyleProperties(), false /* isCacheable */);
    }
}

bool ElementRuleCollector::hasAnyMatchingRules(const RuleSet* ruleSet)
{
    clearMatchedRules();

    m_mode = SelectorChecker::Mode::CollectingRulesIgnoringVirtualPseudoElements;
    int firstRuleIndex = -1, lastRuleIndex = -1;
    StyleResolver::RuleRange ruleRange(firstRuleIndex, lastRuleIndex);
    collectMatchingRules(MatchRequest(ruleSet), ruleRange);

    return !m_matchedRules.isEmpty();
}

} // namespace WebCore
