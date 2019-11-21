/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#include "RuleData.h"
#include "RuleFeature.h"
#include "SelectorCompiler.h"
#include "StyleRule.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;
struct MediaQueryDynamicResults;

namespace Style {

class Resolver;

class RuleSet {
    WTF_MAKE_NONCOPYABLE(RuleSet); WTF_MAKE_FAST_ALLOCATED;
public:
    struct RuleSetSelectorPair {
        RuleSetSelectorPair(const CSSSelector* selector, std::unique_ptr<RuleSet> ruleSet)
            : selector(selector), ruleSet(WTFMove(ruleSet))
        { }
        RuleSetSelectorPair(const RuleSetSelectorPair& pair)
            : selector(pair.selector), ruleSet(const_cast<RuleSetSelectorPair*>(&pair)->ruleSet.release())
        { }

        const CSSSelector* selector;
        std::unique_ptr<RuleSet> ruleSet;
    };

    RuleSet();
    ~RuleSet();

    typedef Vector<RuleData, 1> RuleDataVector;
    typedef HashMap<AtomString, std::unique_ptr<RuleDataVector>> AtomRuleMap;

    void addRulesFromSheet(StyleSheetContents&, const MediaQueryEvaluator&, Style::Resolver* = nullptr);

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
    const RuleDataVector& partPseudoElementRules() const { return m_partPseudoElementRules; }
    const RuleDataVector* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const RuleDataVector* universalRules() const { return &m_universalRules; }

    const Vector<StyleRulePage*>& pageRules() const { return m_pageRules; }

    unsigned ruleCount() const { return m_ruleCount; }

    bool hasShadowPseudoElementRules() const;
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }

private:
    void addChildRules(const Vector<RefPtr<StyleRuleBase>>&, const MediaQueryEvaluator& medium, Style::Resolver*, MediaQueryDynamicResults&);

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
    RuleDataVector m_partPseudoElementRules;
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

} // namespace Style
} // namespace WebCore
