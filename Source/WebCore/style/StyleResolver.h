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
#include "RenderStyle.h"
#include "RuleSet.h"
#include "StyleBuilderState.h"
#include "StyleScopeRuleSets.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSStyleSheet;
class Document;
class Element;
class KeyframeList;
class KeyframeValue;
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

struct ElementStyle {
    ElementStyle(std::unique_ptr<RenderStyle> renderStyle, std::unique_ptr<Relations> relations = { })
        : renderStyle(WTFMove(renderStyle))
        , relations(WTFMove(relations))
    { }

    std::unique_ptr<RenderStyle> renderStyle;
    std::unique_ptr<Relations> relations;
};

class Resolver {
    WTF_MAKE_NONCOPYABLE(Resolver); WTF_MAKE_FAST_ALLOCATED;
public:
    Resolver(Document&);
    ~Resolver();

    ElementStyle styleForElement(const Element&, const RenderStyle* parentStyle, const RenderStyle* parentBoxStyle = nullptr, RuleMatchingBehavior = RuleMatchingBehavior::MatchAllRules, const SelectorFilter* = nullptr);

    void keyframeStylesForAnimation(const Element&, const RenderStyle*, KeyframeList&);

    WEBCORE_EXPORT std::unique_ptr<RenderStyle> pseudoStyleForElement(const Element&, const PseudoElementRequest&, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle = nullptr, const SelectorFilter* = nullptr);

    std::unique_ptr<RenderStyle> styleForPage(int pageIndex);
    std::unique_ptr<RenderStyle> defaultStyleForElement(const Element*);

    Document& document() { return m_document; }
    const Document& document() const { return m_document; }
    const Settings& settings() const { return m_document.settings(); }

    void appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&);

    ScopeRuleSets& ruleSets() { return m_ruleSets; }
    const ScopeRuleSets& ruleSets() const { return m_ruleSets; }

    const MediaQueryEvaluator& mediaQueryEvaluator() const { return m_mediaQueryEvaluator; }

    const RenderStyle* overrideDocumentElementStyle() const { return m_overrideDocumentElementStyle; }
    void setOverrideDocumentElementStyle(const RenderStyle* style) { m_overrideDocumentElementStyle = style; }

    // FIXME: Remove and pass this through the animation system normally.
    void setParentElementStyleForKeyframes(const RenderStyle* style) { m_parentElementStyleForKeyframes = style; }

    void addCurrentSVGFontFaceRules();

    std::unique_ptr<RenderStyle> styleForKeyframe(const Element&, const RenderStyle*, const StyleRuleKeyframe*, KeyframeValue&);
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
    Optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueries();

    void addKeyframeStyle(Ref<StyleRuleKeyframes>&&);

    bool usesFirstLineRules() const { return m_ruleSets.features().usesFirstLineRules; }
    bool usesFirstLetterRules() const { return m_ruleSets.features().usesFirstLetterRules; }
    
    void invalidateMatchedDeclarationsCache();
    void clearCachedDeclarationsAffectedByViewportUnits();

    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }

private:
    friend class PageRuleCollector;

    class State {
    public:
        State() { }
        State(const Element&, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle = nullptr);

    public:
        const Element* element() const { return m_element; }

        void setStyle(std::unique_ptr<RenderStyle>);
        RenderStyle* style() const { return m_style.get(); }
        std::unique_ptr<RenderStyle> takeStyle() { return WTFMove(m_style); }

        void setParentStyle(std::unique_ptr<RenderStyle>);
        const RenderStyle* parentStyle() const { return m_parentStyle; }
        const RenderStyle* rootElementStyle() const { return m_rootElementStyle; }

        const RenderStyle* userAgentAppearanceStyle() const { return m_userAgentAppearanceStyle.get(); }
        void setUserAgentAppearanceStyle(std::unique_ptr<RenderStyle> style) { m_userAgentAppearanceStyle = WTFMove(style); }

    private:
        const Element* m_element { nullptr };
        std::unique_ptr<RenderStyle> m_style;
        const RenderStyle* m_parentStyle { nullptr };
        std::unique_ptr<const RenderStyle> m_ownedParentStyle;
        const RenderStyle* m_rootElementStyle { nullptr };

        std::unique_ptr<RenderStyle> m_userAgentAppearanceStyle;
    };

    BuilderContext builderContext(const State&);

    enum class UseMatchedDeclarationsCache { Yes, No };
    void applyMatchedProperties(State&, const MatchResult&, UseMatchedDeclarationsCache = UseMatchedDeclarationsCache::Yes);

    ScopeRuleSets m_ruleSets;

    typedef HashMap<AtomStringImpl*, RefPtr<StyleRuleKeyframes>> KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

    MediaQueryEvaluator m_mediaQueryEvaluator;
    std::unique_ptr<RenderStyle> m_rootDefaultStyle;

    Document& m_document;

    const RenderStyle* m_overrideDocumentElementStyle { nullptr };
    const RenderStyle* m_parentElementStyleForKeyframes { nullptr };

    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    MatchedDeclarationsCache m_matchedDeclarationsCache;

    bool m_matchAuthorAndUserStyles { true };
    // See if we still have crashes where Resolver gets deleted early.
    bool m_isDeleted { false };
};

inline bool Resolver::hasSelectorForAttribute(const Element& element, const AtomString &attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    if (element.isHTMLElement())
        return m_ruleSets.features().attributeCanonicalLocalNamesInRules.contains(attributeName);
    return m_ruleSets.features().attributeLocalNamesInRules.contains(attributeName);
}

inline bool Resolver::hasSelectorForId(const AtomString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_ruleSets.features().idsInRules.contains(idValue);
}

} // namespace Style
} // namespace WebCore
