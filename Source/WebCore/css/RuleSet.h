/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2014 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "RuleFeature.h"
#include "SelectorCompiler.h"
#include "SelectorFilter.h"
#include "StyleRule.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

enum PropertyWhitelistType {
    PropertyWhitelistNone   = 0,
    PropertyWhitelistMarker,
#if ENABLE(VIDEO_TRACK)
    PropertyWhitelistCue
#endif
};

class CSSSelector;
class ContainerNode;
class MediaQueryEvaluator;
class Node;
class StyleResolver;
class StyleSheetContents;

enum class MatchBasedOnRuleHash : unsigned {
    None,
    Universal,
    ClassA,
    ClassB,
    ClassC
};

class RuleData {
public:
    static const unsigned maximumSelectorComponentCount = 8192;

    RuleData(StyleRule*, unsigned selectorIndex, unsigned selectorListIndex, unsigned position);

    unsigned position() const { return m_position; }
    StyleRule* rule() const { return m_rule.get(); }
    const CSSSelector* selector() const { return m_rule->selectorList().selectorAt(m_selectorIndex); }
    unsigned selectorIndex() const { return m_selectorIndex; }
    unsigned selectorListIndex() const { return m_selectorListIndex; }

    bool canMatchPseudoElement() const { return m_canMatchPseudoElement; }
    MatchBasedOnRuleHash matchBasedOnRuleHash() const { return static_cast<MatchBasedOnRuleHash>(m_matchBasedOnRuleHash); }
    bool containsUncommonAttributeSelector() const { return m_containsUncommonAttributeSelector; }
    unsigned linkMatchType() const { return m_linkMatchType; }
    PropertyWhitelistType propertyWhitelistType() const { return static_cast<PropertyWhitelistType>(m_propertyWhitelistType); }
    const SelectorFilter::Hashes& descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

    void disableSelectorFiltering() { m_descendantSelectorIdentifierHashes[0] = 0; }

private:
    RefPtr<StyleRule> m_rule;
    unsigned m_selectorIndex : 16;
    unsigned m_selectorListIndex : 16;
    // This number was picked fairly arbitrarily. We can probably lower it if we need to.
    // Some simple testing showed <100,000 RuleData's on large sites.
    unsigned m_position : 18;
    unsigned m_matchBasedOnRuleHash : 3;
    unsigned m_canMatchPseudoElement : 1;
    unsigned m_containsUncommonAttributeSelector : 1;
    unsigned m_linkMatchType : 2; //  SelectorChecker::LinkMatchMask
    unsigned m_propertyWhitelistType : 2;
    SelectorFilter::Hashes m_descendantSelectorIdentifierHashes;
};
    
struct SameSizeAsRuleData {
    void* a;
    unsigned b;
    unsigned c;
    unsigned d[4];
};

COMPILE_ASSERT(sizeof(RuleData) == sizeof(SameSizeAsRuleData), RuleData_should_stay_small);

class RuleSet {
    WTF_MAKE_NONCOPYABLE(RuleSet); WTF_MAKE_FAST_ALLOCATED;
public:
    struct RuleSetSelectorPair {
        RuleSetSelectorPair(const CSSSelector* selector, std::unique_ptr<RuleSet> ruleSet) : selector(selector), ruleSet(WTFMove(ruleSet)) { }
        RuleSetSelectorPair(const RuleSetSelectorPair& pair) : selector(pair.selector), ruleSet(const_cast<RuleSetSelectorPair*>(&pair)->ruleSet.release()) { }

        const CSSSelector* selector;
        std::unique_ptr<RuleSet> ruleSet;
    };

    RuleSet();
    ~RuleSet();

    typedef Vector<RuleData, 1> RuleDataVector;
    typedef HashMap<AtomString, std::unique_ptr<RuleDataVector>> AtomRuleMap;

    void addRulesFromSheet(StyleSheetContents&, const MediaQueryEvaluator&, StyleResolver* = 0);

    void addStyleRule(StyleRule*);
    void addRule(StyleRule*, unsigned selectorIndex, unsigned selectorListIndex);
    void addPageRule(StyleRulePage*);
    void addToRuleSet(const AtomString& key, AtomRuleMap&, const RuleData&);
    void shrinkToFit();
    void disableAutoShrinkToFit() { m_autoShrinkToFitEnabled = false; }

    const RuleFeatureSet& features() const { return m_features; }

    const RuleDataVector* idRules(const AtomString& key) const { return m_idRules.get(key); }
    const RuleDataVector* classRules(const AtomString& key) const { return m_classRules.get(key); }
    const RuleDataVector* tagRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* shadowPseudoElementRules(const AtomString& key) const { return m_shadowPseudoElementRules.get(key); }
    const RuleDataVector* linkPseudoClassRules() const { return &m_linkPseudoClassRules; }
#if ENABLE(VIDEO_TRACK)
    const RuleDataVector* cuePseudoRules() const { return &m_cuePseudoRules; }
#endif
    const RuleDataVector& hostPseudoClassRules() const { return m_hostPseudoClassRules; }
    const RuleDataVector& slottedPseudoElementRules() const { return m_slottedPseudoElementRules; }
    const RuleDataVector* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const RuleDataVector* universalRules() const { return &m_universalRules; }

    const Vector<StyleRulePage*>& pageRules() const { return m_pageRules; }

    unsigned ruleCount() const { return m_ruleCount; }

    bool hasShadowPseudoElementRules() const;
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }

private:
    void addChildRules(const Vector<RefPtr<StyleRuleBase>>&, const MediaQueryEvaluator& medium, StyleResolver*, bool isInitiatingElementInUserAgentShadowTree);

    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_tagLocalNameRules;
    AtomRuleMap m_tagLowercaseLocalNameRules;
    AtomRuleMap m_shadowPseudoElementRules;
    RuleDataVector m_linkPseudoClassRules;
#if ENABLE(VIDEO_TRACK)
    RuleDataVector m_cuePseudoRules;
#endif
    RuleDataVector m_hostPseudoClassRules;
    RuleDataVector m_slottedPseudoElementRules;
    RuleDataVector m_focusPseudoClassRules;
    RuleDataVector m_universalRules;
    Vector<StyleRulePage*> m_pageRules;
    unsigned m_ruleCount { 0 };
    bool m_hasHostPseudoClassRulesMatchingInShadowTree { false };
    bool m_autoShrinkToFitEnabled { true };
    RuleFeatureSet m_features;
};

inline const RuleSet::RuleDataVector* RuleSet::tagRules(const AtomString& key, bool isHTMLName) const
{
    const AtomRuleMap* tagRules;
    if (isHTMLName)
        tagRules = &m_tagLowercaseLocalNameRules;
    else
        tagRules = &m_tagLocalNameRules;
    return tagRules->get(key);
}

} // namespace WebCore

namespace WTF {

// RuleData is simple enough that initializing to 0 and moving with memcpy will totally work.
template<> struct VectorTraits<WebCore::RuleData> : SimpleClassVectorTraits { };

} // namespace WTF
