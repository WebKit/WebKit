/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2014 Apple Inc. All rights reserved.
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
#include "RuleSet.h"

#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "HTMLNames.h"
#include "MediaQueryEvaluator.h"
#include "SecurityOrigin.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "ViewportStyleResolver.h"

#if ENABLE(VIDEO_TRACK)
#include "TextTrackCue.h"
#endif

namespace WebCore {

using namespace HTMLNames;

// -----------------------------------------------------------------

static inline MatchBasedOnRuleHash computeMatchBasedOnRuleHash(const CSSSelector& selector)
{
    if (selector.tagHistory())
        return MatchBasedOnRuleHash::None;

    if (selector.match() == CSSSelector::Tag) {
        const QualifiedName& tagQualifiedName = selector.tagQName();
        const AtomString& selectorNamespace = tagQualifiedName.namespaceURI();
        if (selectorNamespace == starAtom() || selectorNamespace == xhtmlNamespaceURI) {
            if (tagQualifiedName == anyQName())
                return MatchBasedOnRuleHash::Universal;
            return MatchBasedOnRuleHash::ClassC;
        }
        return MatchBasedOnRuleHash::None;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(&selector))
        return MatchBasedOnRuleHash::ClassB;
    if (selector.match() == CSSSelector::Id)
        return MatchBasedOnRuleHash::ClassA;
    if (selector.match() == CSSSelector::Class)
        return MatchBasedOnRuleHash::ClassB;
    return MatchBasedOnRuleHash::None;
}

static bool selectorCanMatchPseudoElement(const CSSSelector& rootSelector)
{
    const CSSSelector* selector = &rootSelector;
    do {
        if (selector->matchesPseudoElement())
            return true;

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (selectorCanMatchPseudoElement(*subSelector))
                    return true;
            }
        }

        selector = selector->tagHistory();
    } while (selector);
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static bool containsUncommonAttributeSelector(const CSSSelector& rootSelector, bool matchesRightmostElement)
{
    const CSSSelector* selector = &rootSelector;
    do {
        if (selector->isAttributeSelector()) {
            // FIXME: considering non-rightmost simple selectors is necessary because of the style sharing of cousins.
            // It is a primitive solution which disable a lot of style sharing on pages that rely on attributes for styling.
            // We should investigate better ways of doing this.
            if (!isCommonAttributeSelectorAttribute(selector->attribute()) || !matchesRightmostElement)
                return true;
        }

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (containsUncommonAttributeSelector(*subSelector, matchesRightmostElement))
                    return true;
            }
        }

        if (selector->relation() != CSSSelector::Subselector)
            matchesRightmostElement = false;

        selector = selector->tagHistory();
    } while (selector);
    return false;
}

static inline bool containsUncommonAttributeSelector(const CSSSelector& rootSelector)
{
    return containsUncommonAttributeSelector(rootSelector, true);
}

static inline PropertyWhitelistType determinePropertyWhitelistType(const CSSSelector* selector)
{
    for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
#if ENABLE(VIDEO_TRACK)
        if (component->match() == CSSSelector::PseudoElement && (component->pseudoElementType() == CSSSelector::PseudoElementCue || component->value() == TextTrackCue::cueShadowPseudoId()))
            return PropertyWhitelistCue;
#endif
        if (component->match() == CSSSelector::PseudoElement && component->pseudoElementType() == CSSSelector::PseudoElementMarker)
            return PropertyWhitelistMarker;

        if (const auto* selectorList = selector->selectorList()) {
            for (const auto* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                auto whitelistType = determinePropertyWhitelistType(subSelector);
                if (whitelistType != PropertyWhitelistNone)
                    return whitelistType;
            }
        }
    }
    return PropertyWhitelistNone;
}

RuleData::RuleData(StyleRule* rule, unsigned selectorIndex, unsigned selectorListIndex, unsigned position)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_selectorListIndex(selectorListIndex)
    , m_position(position)
    , m_matchBasedOnRuleHash(static_cast<unsigned>(computeMatchBasedOnRuleHash(*selector())))
    , m_canMatchPseudoElement(selectorCanMatchPseudoElement(*selector()))
    , m_containsUncommonAttributeSelector(WebCore::containsUncommonAttributeSelector(*selector()))
    , m_linkMatchType(SelectorChecker::determineLinkMatchType(selector()))
    , m_propertyWhitelistType(determinePropertyWhitelistType(selector()))
    , m_descendantSelectorIdentifierHashes(SelectorFilter::collectHashes(*selector()))
{
    ASSERT(m_position == position);
    ASSERT(m_selectorIndex == selectorIndex);
}

RuleSet::RuleSet() = default;

RuleSet::~RuleSet() = default;

void RuleSet::addToRuleSet(const AtomString& key, AtomRuleMap& map, const RuleData& ruleData)
{
    if (key.isNull())
        return;
    auto& rules = map.add(key, nullptr).iterator->value;
    if (!rules)
        rules = makeUnique<RuleDataVector>();
    rules->append(ruleData);
}

static unsigned rulesCountForName(const RuleSet::AtomRuleMap& map, const AtomString& name)
{
    if (const auto* rules = map.get(name))
        return rules->size();
    return 0;
}

static bool isHostSelectorMatchingInShadowTree(const CSSSelector& startSelector)
{
    auto* leftmostSelector = &startSelector;
    bool hasDescendantOrChildRelation = false;
    while (auto* previous = leftmostSelector->tagHistory()) {
        hasDescendantOrChildRelation = leftmostSelector->hasDescendantOrChildRelation();
        leftmostSelector = previous;
    }
    if (!hasDescendantOrChildRelation)
        return false;

    return leftmostSelector->match() == CSSSelector::PseudoClass && leftmostSelector->pseudoClassType() == CSSSelector::PseudoClassHost;
}

void RuleSet::addRule(StyleRule* rule, unsigned selectorIndex, unsigned selectorListIndex)
{
    RuleData ruleData(rule, selectorIndex, selectorListIndex, m_ruleCount++);
    m_features.collectFeatures(ruleData);

    unsigned classBucketSize = 0;
    const CSSSelector* idSelector = nullptr;
    const CSSSelector* tagSelector = nullptr;
    const CSSSelector* classSelector = nullptr;
    const CSSSelector* linkSelector = nullptr;
    const CSSSelector* focusSelector = nullptr;
    const CSSSelector* hostPseudoClassSelector = nullptr;
    const CSSSelector* customPseudoElementSelector = nullptr;
    const CSSSelector* slottedPseudoElementSelector = nullptr;
#if ENABLE(VIDEO_TRACK)
    const CSSSelector* cuePseudoElementSelector = nullptr;
#endif
    const CSSSelector* selector = ruleData.selector();
    do {
        switch (selector->match()) {
        case CSSSelector::Id:
            idSelector = selector;
            break;
        case CSSSelector::Class: {
            auto& className = selector->value();
            if (!classSelector) {
                classSelector = selector;
                classBucketSize = rulesCountForName(m_classRules, className);
            } else if (classBucketSize) {
                unsigned newClassBucketSize = rulesCountForName(m_classRules, className);
                if (newClassBucketSize < classBucketSize) {
                    classSelector = selector;
                    classBucketSize = newClassBucketSize;
                }
            }
            break;
        }
        case CSSSelector::Tag:
            if (selector->tagQName().localName() != starAtom())
                tagSelector = selector;
            break;
        case CSSSelector::PseudoElement:
            switch (selector->pseudoElementType()) {
            case CSSSelector::PseudoElementWebKitCustom:
            case CSSSelector::PseudoElementWebKitCustomLegacyPrefixed:
                customPseudoElementSelector = selector;
                break;
            case CSSSelector::PseudoElementSlotted:
                slottedPseudoElementSelector = selector;
                break;
#if ENABLE(VIDEO_TRACK)
            case CSSSelector::PseudoElementCue:
                cuePseudoElementSelector = selector;
                break;
#endif
            default:
                break;
            }
            break;
        case CSSSelector::PseudoClass:
            switch (selector->pseudoClassType()) {
            case CSSSelector::PseudoClassLink:
            case CSSSelector::PseudoClassVisited:
            case CSSSelector::PseudoClassAnyLink:
            case CSSSelector::PseudoClassAnyLinkDeprecated:
                linkSelector = selector;
                break;
            case CSSSelector::PseudoClassFocus:
                focusSelector = selector;
                break;
            case CSSSelector::PseudoClassHost:
                hostPseudoClassSelector = selector;
                break;
            default:
                break;
            }
            break;
        case CSSSelector::Unknown:
        case CSSSelector::Exact:
        case CSSSelector::Set:
        case CSSSelector::List:
        case CSSSelector::Hyphen:
        case CSSSelector::Contain:
        case CSSSelector::Begin:
        case CSSSelector::End:
        case CSSSelector::PagePseudoClass:
            break;
        }
        if (selector->relation() != CSSSelector::Subselector)
            break;
        selector = selector->tagHistory();
    } while (selector);

#if ENABLE(VIDEO_TRACK)
    if (cuePseudoElementSelector) {
        m_cuePseudoRules.append(ruleData);
        return;
    }
#endif

    if (slottedPseudoElementSelector) {
        // ::slotted pseudo elements work accross shadow boundary making filtering difficult.
        ruleData.disableSelectorFiltering();
        m_slottedPseudoElementRules.append(ruleData);
        return;
    }

    if (customPseudoElementSelector) {
        // FIXME: Custom pseudo elements are handled by the shadow tree's selector filter. It doesn't know about the main DOM.
        ruleData.disableSelectorFiltering();
        addToRuleSet(customPseudoElementSelector->value(), m_shadowPseudoElementRules, ruleData);
        return;
    }

    if (!m_hasHostPseudoClassRulesMatchingInShadowTree)
        m_hasHostPseudoClassRulesMatchingInShadowTree = isHostSelectorMatchingInShadowTree(*ruleData.selector());

    if (hostPseudoClassSelector) {
        m_hostPseudoClassRules.append(ruleData);
        return;
    }

    if (idSelector) {
        addToRuleSet(idSelector->value(), m_idRules, ruleData);
        return;
    }

    if (classSelector) {
        addToRuleSet(classSelector->value(), m_classRules, ruleData);
        return;
    }

    if (linkSelector) {
        m_linkPseudoClassRules.append(ruleData);
        return;
    }

    if (focusSelector) {
        m_focusPseudoClassRules.append(ruleData);
        return;
    }

    if (tagSelector) {
        addToRuleSet(tagSelector->tagQName().localName(), m_tagLocalNameRules, ruleData);
        addToRuleSet(tagSelector->tagLowercaseLocalName(), m_tagLowercaseLocalNameRules, ruleData);
        return;
    }

    // If we didn't find a specialized map to stick it in, file under universal rules.
    m_universalRules.append(ruleData);
}

void RuleSet::addPageRule(StyleRulePage* rule)
{
    m_pageRules.append(rule);
}

void RuleSet::addChildRules(const Vector<RefPtr<StyleRuleBase>>& rules, const MediaQueryEvaluator& medium, StyleResolver* resolver, bool isInitiatingElementInUserAgentShadowTree)
{
    for (auto& rule : rules) {
        if (is<StyleRule>(*rule))
            addStyleRule(downcast<StyleRule>(rule.get()));
        else if (is<StyleRulePage>(*rule))
            addPageRule(downcast<StyleRulePage>(rule.get()));
        else if (is<StyleRuleMedia>(*rule)) {
            auto& mediaRule = downcast<StyleRuleMedia>(*rule);
            if ((!mediaRule.mediaQueries() || medium.evaluate(*mediaRule.mediaQueries(), resolver)))
                addChildRules(mediaRule.childRules(), medium, resolver, isInitiatingElementInUserAgentShadowTree);
        } else if (is<StyleRuleFontFace>(*rule) && resolver) {
            // Add this font face to our set.
            resolver->document().fontSelector().addFontFaceRule(downcast<StyleRuleFontFace>(*rule.get()), isInitiatingElementInUserAgentShadowTree);
            resolver->invalidateMatchedPropertiesCache();
        } else if (is<StyleRuleKeyframes>(*rule) && resolver)
            resolver->addKeyframeStyle(downcast<StyleRuleKeyframes>(*rule));
        else if (is<StyleRuleSupports>(*rule) && downcast<StyleRuleSupports>(*rule).conditionIsSupported())
            addChildRules(downcast<StyleRuleSupports>(*rule).childRules(), medium, resolver, isInitiatingElementInUserAgentShadowTree);
#if ENABLE(CSS_DEVICE_ADAPTATION)
        else if (is<StyleRuleViewport>(*rule) && resolver) {
            resolver->viewportStyleResolver()->addViewportRule(downcast<StyleRuleViewport>(rule.get()));
        }
#endif
    }
}

void RuleSet::addRulesFromSheet(StyleSheetContents& sheet, const MediaQueryEvaluator& medium, StyleResolver* resolver)
{
    for (auto& rule : sheet.importRules()) {
        if (rule->styleSheet() && (!rule->mediaQueries() || medium.evaluate(*rule->mediaQueries(), resolver)))
            addRulesFromSheet(*rule->styleSheet(), medium, resolver);
    }

    // FIXME: Skip Content Security Policy check when stylesheet is in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    bool isInitiatingElementInUserAgentShadowTree = false;
    addChildRules(sheet.childRules(), medium, resolver, isInitiatingElementInUserAgentShadowTree);

    if (m_autoShrinkToFitEnabled)
        shrinkToFit();
}

void RuleSet::addStyleRule(StyleRule* rule)
{
    unsigned selectorListIndex = 0;
    for (size_t selectorIndex = 0; selectorIndex != notFound; selectorIndex = rule->selectorList().indexOfNextSelectorAfter(selectorIndex))
        addRule(rule, selectorIndex, selectorListIndex++);
}

bool RuleSet::hasShadowPseudoElementRules() const
{
    if (!m_shadowPseudoElementRules.isEmpty())
        return true;
#if ENABLE(VIDEO_TRACK)
    if (!m_cuePseudoRules.isEmpty())
        return true;
#endif
    return false;
}

static inline void shrinkMapVectorsToFit(RuleSet::AtomRuleMap& map)
{
    for (auto& vector : map.values())
        vector->shrinkToFit();
}

void RuleSet::shrinkToFit()
{
    shrinkMapVectorsToFit(m_idRules);
    shrinkMapVectorsToFit(m_classRules);
    shrinkMapVectorsToFit(m_tagLocalNameRules);
    shrinkMapVectorsToFit(m_tagLowercaseLocalNameRules);
    shrinkMapVectorsToFit(m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
#if ENABLE(VIDEO_TRACK)
    m_cuePseudoRules.shrinkToFit();
#endif
    m_hostPseudoClassRules.shrinkToFit();
    m_slottedPseudoElementRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_pageRules.shrinkToFit();
    m_features.shrinkToFit();
}

} // namespace WebCore
