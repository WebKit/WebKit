/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "CSSSelector.h"
#include "ElementRuleCollector.h"
#include "InspectorCSSOMWrappers.h"
#include "MatchedDeclarationsCache.h"
#include "MediaQueryEvaluator.h"
#include "RuleSet.h"
#include "StyleScopeRuleSets.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class BlendingKeyframe;
class BlendingKeyframes;
class CSSStyleSheet;
class Document;
class Element;
class RenderStyle;
class RuleData;
class RuleSet;
class SelectorFilter;
class Settings;
class StyleRuleKeyframe;
class StyleProperties;
class StyleRule;
class StyleRuleKeyframes;
class StyleRulePage;
class StyleSheet;
class StyleSheetList;
class ViewportStyleResolver;

// MatchOnlyUserAgentRules is used in media queries, where relative units
// are interpreted according to the document root element style, and styled only
// from the User Agent Stylesheet rules.

enum class RuleMatchingBehavior: uint8_t {
    MatchAllRules,
    MatchAllRulesExcludingSMIL,
    MatchOnlyUserAgentRules,
};

namespace Style {

struct ResolvedStyle;
struct SelectorMatchingState;

struct ResolutionContext {
    const RenderStyle* parentStyle;
    const RenderStyle* parentBoxStyle { nullptr };
    // This needs to be provided during style resolution when up-to-date document element style is not available via DOM.
    const RenderStyle* documentElementStyle { nullptr };
    SelectorMatchingState* selectorMatchingState { nullptr };
    bool isSVGUseTreeRoot { false };
};

class Resolver : public RefCounted<Resolver>, public CanMakeSingleThreadWeakPtr<Resolver> {
    WTF_MAKE_ISO_ALLOCATED(Resolver);
public:
    // Style resolvers are shared between shadow trees with identical styles. That's why we don't simply provide a Style::Scope.
    enum class ScopeType : bool { Document, ShadowTree };
    static Ref<Resolver> create(Document&, ScopeType);
    ~Resolver();

    ResolvedStyle styleForElement(const Element&, const ResolutionContext&, RuleMatchingBehavior = RuleMatchingBehavior::MatchAllRules);

    void keyframeStylesForAnimation(const Element&, const RenderStyle& elementStyle, const ResolutionContext&, BlendingKeyframes&);

    WEBCORE_EXPORT std::optional<ResolvedStyle> styleForPseudoElement(const Element&, const PseudoElementRequest&, const ResolutionContext&);

    std::unique_ptr<RenderStyle> styleForPage(int pageIndex);
    std::unique_ptr<RenderStyle> defaultStyleForElement(const Element*);

    Document& document();
    const Document& document() const;
    const Settings& settings() const;

    ScopeType scopeType() const { return m_scopeType; }

    void appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&);

    ScopeRuleSets& ruleSets() { return m_ruleSets; }
    const ScopeRuleSets& ruleSets() const { return m_ruleSets; }

    const MQ::MediaQueryEvaluator& mediaQueryEvaluator() const { return m_mediaQueryEvaluator; }

    void addCurrentSVGFontFaceRules();

    std::unique_ptr<RenderStyle> styleForKeyframe(const Element&, const RenderStyle& elementStyle, const ResolutionContext&, const StyleRuleKeyframe&, BlendingKeyframe&);
    bool isAnimationNameValid(const String&);

    // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
    enum CSSRuleFilter {
        UAAndUserCSSRules   = 1 << 1,
        AuthorCSSRules      = 1 << 2,
        EmptyCSSRules       = 1 << 3,
        AllButEmptyCSSRules = UAAndUserCSSRules | AuthorCSSRules,
        AllCSSRules         = AllButEmptyCSSRules | EmptyCSSRules,
    };
    Vector<RefPtr<const StyleRule>> styleRulesForElement(const Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    Vector<RefPtr<const StyleRule>> pseudoStyleRulesForElement(const Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);

    bool hasSelectorForId(const AtomString&) const;
    bool hasSelectorForAttribute(const Element&, const AtomString&) const;

    bool hasViewportDependentMediaQueries() const;
    std::optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueries();

    void addKeyframeStyle(Ref<StyleRuleKeyframes>&&);
    Vector<Ref<StyleRuleKeyframe>> keyframeRulesForName(const AtomString&) const;

    bool usesFirstLineRules() const { return m_ruleSets.features().usesFirstLineRules; }
    bool usesFirstLetterRules() const { return m_ruleSets.features().usesFirstLetterRules; }
    
    void invalidateMatchedDeclarationsCache();
    void clearCachedDeclarationsAffectedByViewportUnits();

    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }

    bool isSharedBetweenShadowTrees() const { return m_isSharedBetweenShadowTrees; }
    void setSharedBetweenShadowTrees() { m_isSharedBetweenShadowTrees = true; }

    const RenderStyle* rootDefaultStyle() const { return m_rootDefaultStyle.get(); }

private:
    Resolver(Document&, ScopeType);
    void initialize();

    class State;

    BuilderContext builderContext(const State&);

    void applyMatchedProperties(State&, const MatchResult&);

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    const ScopeType m_scopeType;

    ScopeRuleSets m_ruleSets;

    typedef HashMap<AtomString, RefPtr<StyleRuleKeyframes>> KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

    MQ::MediaQueryEvaluator m_mediaQueryEvaluator;
    std::unique_ptr<RenderStyle> m_rootDefaultStyle;

    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    MatchedDeclarationsCache m_matchedDeclarationsCache;

    bool m_matchAuthorAndUserStyles { true };
    bool m_isSharedBetweenShadowTrees { false };
};

} // namespace Style
} // namespace WebCore
