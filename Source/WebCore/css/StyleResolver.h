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

#ifndef StyleResolver_h
#define StyleResolver_h

#include "CSSToStyleMap.h"
#include "CSSValueList.h"
#include "DocumentRuleSets.h"
#include "InspectorCSSOMWrappers.h"
#include "LinkHash.h"
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "RuleFeature.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "ScrollTypes.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleInheritedData.h"
#include "ViewportStyleResolver.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSCursorImageValue;
class CSSFontSelector;
class CSSFontFace;
class CSSFontFaceRule;
class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSImageValue;
class CSSPageRule;
class CSSPrimitiveValue;
class CSSProperty;
class CSSSelector;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class Document;
class DeprecatedStyleBuilder;
class Element;
class Frame;
class FrameView;
class URL;
class KeyframeList;
class KeyframeValue;
class MediaQueryEvaluator;
class Node;
class RenderRegion;
class RenderScrollbar;
class RuleData;
class RuleSet;
class Settings;
class StyleImage;
class StyleKeyframe;
class StylePendingImage;
class StyleProperties;
class StyleRule;
class StyleRuleKeyframes;
class StyleRulePage;
class StyleRuleRegion;
class StyleSheet;
class StyleSheetList;
class StyledElement;
class ViewportStyleResolver;
class WebKitCSSFilterValue;

class MediaQueryResult {
    WTF_MAKE_NONCOPYABLE(MediaQueryResult); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaQueryResult(const MediaQueryExp& expr, bool result)
        : m_expression(expr)
        , m_result(result)
    {
    }

    MediaQueryExp m_expression;
    bool m_result;
};

enum StyleSharingBehavior {
    AllowStyleSharing,
    DisallowStyleSharing,
};

// MatchOnlyUserAgentRules is used in media queries, where relative units
// are interpreted according to the document root element style, and styled only
// from the User Agent Stylesheet rules.

enum RuleMatchingBehavior {
    MatchAllRules,
    MatchAllRulesExcludingSMIL,
    MatchOnlyUserAgentRules,
};

class PseudoStyleRequest {
public:
    PseudoStyleRequest(PseudoId pseudoId, RenderScrollbar* scrollbar = 0, ScrollbarPart scrollbarPart = NoPart)
        : pseudoId(pseudoId)
        , scrollbarPart(scrollbarPart)
        , scrollbar(scrollbar)
    {
    }

    PseudoId pseudoId;
    ScrollbarPart scrollbarPart;
    RenderScrollbar* scrollbar;
};

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class StyleResolver {
    WTF_MAKE_NONCOPYABLE(StyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    StyleResolver(Document&, bool matchAuthorAndUserStyles);
    ~StyleResolver();

    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element*);
    void popParentElement(Element*);

    PassRef<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules, RenderRegion* regionForStyling = nullptr);

    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    PassRef<RenderStyle> styleForPage(int pageIndex);
    PassRef<RenderStyle> defaultStyleForElement();

    RenderStyle* style() const { return m_state.style(); }
    RenderStyle* parentStyle() const { return m_state.parentStyle(); }
    RenderStyle* rootElementStyle() const { return m_state.rootElementStyle(); }
    Element* element() { return m_state.element(); }
    Document& document() { return m_document; }

    // FIXME: It could be better to call m_ruleSets.appendAuthorStyleSheets() directly after we factor StyleRsolver further.
    // https://bugs.webkit.org/show_bug.cgi?id=108890
    void appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet>>&);

    DocumentRuleSets& ruleSets() { return m_ruleSets; }
    const DocumentRuleSets& ruleSets() const { return m_ruleSets; }
    SelectorFilter& selectorFilter() { return m_selectorFilter; }

    const MediaQueryEvaluator& mediaQueryEvaluator() const { return *m_medium; }

private:
    void initElement(Element*);
    RenderStyle* locateSharedStyle();
    bool styleSharingCandidateMatchesRuleSet(RuleSet*);
    Node* locateCousinList(Element* parent, unsigned& visitedNodeCount) const;
    StyledElement* findSiblingForStyleSharing(Node*, unsigned& count) const;
    bool canShareStyleWithElement(StyledElement*) const;

    PassRef<RenderStyle> styleForKeyframe(const RenderStyle*, const StyleKeyframe*, KeyframeValue&);

public:
    // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
    enum CSSRuleFilter {
        UAAndUserCSSRules   = 1 << 1,
        AuthorCSSRules      = 1 << 2,
        EmptyCSSRules       = 1 << 3,
        CrossOriginCSSRules = 1 << 4,
        AllButEmptyCSSRules = UAAndUserCSSRules | AuthorCSSRules | CrossOriginCSSRules,
        AllCSSRules         = AllButEmptyCSSRules | EmptyCSSRules,
    };
    Vector<RefPtr<StyleRuleBase>> styleRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    Vector<RefPtr<StyleRuleBase>> pseudoStyleRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);

public:
    void applyPropertyToStyle(CSSPropertyID, CSSValue*, RenderStyle*);

    void applyPropertyToCurrentStyle(CSSPropertyID, CSSValue*);

    void updateFont();
    void initializeFontStyle(Settings*);

    void setFontSize(FontDescription&, float size);

public:
    bool useSVGZoomRules();

    static bool colorFromPrimitiveValueIsDerivedFromElement(CSSPrimitiveValue*);
    Color colorFromPrimitiveValue(CSSPrimitiveValue*, bool forVisitedLink = false) const;

    bool hasSelectorForId(const AtomicString&) const;
    bool hasSelectorForClass(const AtomicString&) const;
    bool hasSelectorForAttribute(const AtomicString&) const;

    CSSFontSelector* fontSelector() const { return m_fontSelector.get(); }
#if ENABLE(CSS_DEVICE_ADAPTATION)
    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }
#endif

    void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool affectedByViewportChange() const;

    void addKeyframeStyle(PassRefPtr<StyleRuleKeyframes>);

    bool checkRegionStyle(Element* regionElement);

    bool usesSiblingRules() const { return !m_ruleSets.features().siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_ruleSets.features().usesFirstLineRules; }
    bool usesBeforeAfterRules() const { return m_ruleSets.features().usesBeforeAfterRules; }
    
    void invalidateMatchedPropertiesCache();

#if ENABLE(CSS_FILTERS)
    bool createFilterOperations(CSSValue* inValue, FilterOperations& outOperations);
    void loadPendingSVGDocuments();
#endif // ENABLE(CSS_FILTERS)

    void loadPendingResources();

    int viewportPercentageValue(CSSPrimitiveValue& unit, int percentage);

    struct RuleRange {
        RuleRange(int& firstRuleIndex, int& lastRuleIndex): firstRuleIndex(firstRuleIndex), lastRuleIndex(lastRuleIndex) { }
        int& firstRuleIndex;
        int& lastRuleIndex;
    };

    struct MatchRanges {
        MatchRanges() : firstUARule(-1), lastUARule(-1), firstAuthorRule(-1), lastAuthorRule(-1), firstUserRule(-1), lastUserRule(-1) { }
        int firstUARule;
        int lastUARule;
        int firstAuthorRule;
        int lastAuthorRule;
        int firstUserRule;
        int lastUserRule;
        RuleRange UARuleRange() { return RuleRange(firstUARule, lastUARule); }
        RuleRange authorRuleRange() { return RuleRange(firstAuthorRule, lastAuthorRule); }
        RuleRange userRuleRange() { return RuleRange(firstUserRule, lastUserRule); }
    };

    struct MatchedProperties {
        MatchedProperties();
        ~MatchedProperties();
        
        RefPtr<StyleProperties> properties;
        union {
            struct {
                unsigned linkMatchType : 2;
                unsigned whitelistType : 2;
            };
            // Used to make sure all memory is zero-initialized since we compute the hash over the bytes of this object.
            void* possiblyPaddedMember;
        };
    };

    struct MatchResult {
        MatchResult() : isCacheable(true) { }
        Vector<MatchedProperties, 64> matchedProperties;
        Vector<StyleRule*, 64> matchedRules;
        MatchRanges ranges;
        bool isCacheable;

        void addMatchedProperties(const StyleProperties&, StyleRule* = 0, unsigned linkMatchType = SelectorChecker::MatchAll, PropertyWhitelistType = PropertyWhitelistNone);
    };

private:
    // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
    void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForZoomChange(RenderStyle*, RenderStyle* parentStyle);
#if ENABLE(IOS_TEXT_AUTOSIZING)
    void checkForTextSizeAdjust(RenderStyle*);
#endif

    void adjustRenderStyle(RenderStyle& styleToAdjust, const RenderStyle& parentStyle, Element*);
#if ENABLE(CSS_GRID_LAYOUT)
    void adjustGridItemPosition(RenderStyle& styleToAdjust, const RenderStyle& parentStyle) const;
    std::unique_ptr<GridPosition> adjustNamedGridItemPosition(const NamedGridAreaMap&, const NamedGridLinesMap&, const GridPosition&, GridPositionSide) const;
#endif

    bool fastRejectSelector(const RuleData&) const;

    enum ShouldUseMatchedPropertiesCache { DoNotUseMatchedPropertiesCache = 0, UseMatchedPropertiesCache };
    void applyMatchedProperties(const MatchResult&, const Element*, ShouldUseMatchedPropertiesCache = UseMatchedPropertiesCache);

    class CascadedProperties;

    void applyCascadedProperties(CascadedProperties&, int firstProperty, int lastProperty);
    void cascadeMatches(CascadedProperties&, const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);

    static bool isValidRegionStyleProperty(CSSPropertyID);
#if ENABLE(VIDEO_TRACK)
    static bool isValidCueStyleProperty(CSSPropertyID);
#endif
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);
    Settings* documentSettings() { return m_document.settings(); }

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    DocumentRuleSets m_ruleSets;

    typedef HashMap<AtomicStringImpl*, RefPtr<StyleRuleKeyframes>> KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

public:
    typedef HashMap<CSSPropertyID, RefPtr<CSSValue>> PendingImagePropertyMap;

    class State {
        WTF_MAKE_NONCOPYABLE(State);
    public:
        State()
        : m_element(0)
        , m_styledElement(0)
        , m_parentStyle(0)
        , m_rootElementStyle(0)
        , m_regionForStyling(0)
        , m_elementLinkState(NotInsideLink)
        , m_elementAffectedByClassRules(false)
        , m_applyPropertyToRegularStyle(true)
        , m_applyPropertyToVisitedLinkStyle(false)
        , m_lineHeightValue(0)
        , m_fontDirty(false)
        , m_hasUAAppearance(false)
        , m_backgroundData(BackgroundFillLayer) { }

    public:
        void initElement(Element*);
        void initForStyleResolve(Document&, Element*, RenderStyle* parentStyle = 0, RenderRegion* regionForStyling = 0);
        void clear();

        Document& document() const { return m_element->document(); }
        Element* element() const { return m_element; }
        StyledElement* styledElement() const { return m_styledElement; }
        void setStyle(PassRef<RenderStyle> style) { m_style = std::move(style); }
        RenderStyle* style() const { return m_style.get(); }
        PassRef<RenderStyle> takeStyle() { return m_style.releaseNonNull(); }

        void setParentStyle(PassRef<RenderStyle> parentStyle) { m_parentStyle = std::move(parentStyle); }
        RenderStyle* parentStyle() const { return m_parentStyle.get(); }
        RenderStyle* rootElementStyle() const { return m_rootElementStyle; }

        const RenderRegion* regionForStyling() const { return m_regionForStyling; }
        EInsideLink elementLinkState() const { return m_elementLinkState; }
        void setElementAffectedByClassRules(bool isAffected) { m_elementAffectedByClassRules = isAffected; }
        bool elementAffectedByClassRules() const { return m_elementAffectedByClassRules; }

        void setApplyPropertyToRegularStyle(bool isApply) { m_applyPropertyToRegularStyle = isApply; }
        void setApplyPropertyToVisitedLinkStyle(bool isApply) { m_applyPropertyToVisitedLinkStyle = isApply; }
        bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
        bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }
        PendingImagePropertyMap& pendingImageProperties() { return m_pendingImageProperties; }
#if ENABLE(CSS_FILTERS)
        Vector<RefPtr<ReferenceFilterOperation>>& filtersWithPendingSVGDocuments() { return m_filtersWithPendingSVGDocuments; }
#endif

        void setLineHeightValue(CSSValue* value) { m_lineHeightValue = value; }
        CSSValue* lineHeightValue() { return m_lineHeightValue; }
        void setFontDirty(bool isDirty) { m_fontDirty = isDirty; }
        bool fontDirty() const { return m_fontDirty; }

        void cacheBorderAndBackground();
        bool hasUAAppearance() const { return m_hasUAAppearance; }
        BorderData borderData() const { return m_borderData; }
        FillLayer backgroundData() const { return m_backgroundData; }
        Color backgroundColor() const { return m_backgroundColor; }

        const FontDescription& fontDescription() { return m_style->fontDescription(); }
        const FontDescription& parentFontDescription() { return m_parentStyle->fontDescription(); }
        void setFontDescription(const FontDescription& fontDescription) { m_fontDirty |= m_style->setFontDescription(fontDescription); }
        void setZoom(float f) { m_fontDirty |= m_style->setZoom(f); }
        void setEffectiveZoom(float f) { m_fontDirty |= m_style->setEffectiveZoom(f); }
        void setWritingMode(WritingMode writingMode) { m_fontDirty |= m_style->setWritingMode(writingMode); }
        void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= m_style->setTextOrientation(textOrientation); }

        bool useSVGZoomRules() const { return m_element && m_element->isSVGElement(); }

    private:
        // FIXME(bug 108563): to make it easier to review, these member
        // variables are public. However we should add methods to access
        // these variables.
        Element* m_element;
        RefPtr<RenderStyle> m_style;
        StyledElement* m_styledElement;
        RefPtr<RenderStyle> m_parentStyle;
        RenderStyle* m_rootElementStyle;

        // Required to ASSERT in applyProperties.
        RenderRegion* m_regionForStyling;
        
        EInsideLink m_elementLinkState;

        bool m_elementAffectedByClassRules;

        bool m_applyPropertyToRegularStyle;
        bool m_applyPropertyToVisitedLinkStyle;

        PendingImagePropertyMap m_pendingImageProperties;
#if ENABLE(CSS_FILTERS)
        Vector<RefPtr<ReferenceFilterOperation>> m_filtersWithPendingSVGDocuments;
#endif
        CSSValue* m_lineHeightValue;
        bool m_fontDirty;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        FillLayer m_backgroundData;
        Color m_backgroundColor;
    };

    State& state() { return m_state; }

    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue*);
    PassRefPtr<StyleImage> cachedOrPendingFromValue(CSSPropertyID, CSSImageValue*);
    PassRefPtr<StyleImage> generatedOrPendingFromValue(CSSPropertyID, CSSImageGeneratorValue&);
#if ENABLE(CSS_IMAGE_SET)
    PassRefPtr<StyleImage> setOrPendingFromValue(CSSPropertyID, CSSImageSetValue*);
#endif
    PassRefPtr<StyleImage> cursorOrPendingFromValue(CSSPropertyID, CSSCursorImageValue*);

    bool applyPropertyToRegularStyle() const { return m_state.applyPropertyToRegularStyle(); }
    bool applyPropertyToVisitedLinkStyle() const { return m_state.applyPropertyToVisitedLinkStyle(); }

    static Length convertToIntLength(const CSSPrimitiveValue*, const RenderStyle*, const RenderStyle* rootStyle, double multiplier = 1);
    static Length convertToFloatLength(const CSSPrimitiveValue*, const RenderStyle*, const RenderStyle* rootStyle, double multiplier = 1);

    CSSToStyleMap* styleMap() { return &m_styleMap; }
    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }
    const FontDescription& fontDescription() { return m_state.fontDescription(); }
    const FontDescription& parentFontDescription() { return m_state.parentFontDescription(); }
    void setFontDescription(const FontDescription& fontDescription) { m_state.setFontDescription(fontDescription); }
    void setZoom(float f) { m_state.setZoom(f); }
    void setEffectiveZoom(float f) { m_state.setEffectiveZoom(f); }
    void setWritingMode(WritingMode writingMode) { m_state.setWritingMode(writingMode); }
    void setTextOrientation(TextOrientation textOrientation) { m_state.setTextOrientation(textOrientation); }

private:
    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

private:
    bool canShareStyleWithControl(StyledElement*) const;

    void applyProperty(CSSPropertyID, CSSValue*);

    void applySVGProperty(CSSPropertyID, CSSValue*);

    PassRefPtr<StyleImage> loadPendingImage(const StylePendingImage&, const ResourceLoaderOptions&);
    PassRefPtr<StyleImage> loadPendingImage(const StylePendingImage&);
    void loadPendingImages();
#if ENABLE(CSS_SHAPES)
    void loadPendingShapeImage(ShapeValue*);
#endif

    static unsigned computeMatchedPropertiesHash(const MatchedProperties*, unsigned size);
    struct MatchedPropertiesCacheItem {
        Vector<MatchedProperties> matchedProperties;
        MatchRanges ranges;
        RefPtr<RenderStyle> renderStyle;
        RefPtr<RenderStyle> parentRenderStyle;
    };
    const MatchedPropertiesCacheItem* findFromMatchedPropertiesCache(unsigned hash, const MatchResult&);
    void addToMatchedPropertiesCache(const RenderStyle*, const RenderStyle* parentStyle, unsigned hash, const MatchResult&);

    // Every N additions to the matched declaration cache trigger a sweep where entries holding
    // the last reference to a style declaration are garbage collected.
    void sweepMatchedPropertiesCache(Timer<StyleResolver>*);

    bool classNamesAffectedByRules(const SpaceSplitString&) const;
    bool sharingCandidateHasIdenticalStyleAffectingAttributes(StyledElement*) const;


    unsigned m_matchedPropertiesCacheAdditionsSinceLastSweep;

    typedef HashMap<unsigned, MatchedPropertiesCacheItem> MatchedPropertiesCache;
    MatchedPropertiesCache m_matchedPropertiesCache;

    Timer<StyleResolver> m_matchedPropertiesCacheSweepTimer;

    std::unique_ptr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    Document& m_document;
    SelectorFilter m_selectorFilter;

    bool m_matchAuthorAndUserStyles;

    RefPtr<CSSFontSelector> m_fontSelector;
    Vector<std::unique_ptr<MediaQueryResult>> m_viewportDependentMediaQueryResults;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;
#endif

    const DeprecatedStyleBuilder& m_deprecatedStyleBuilder;

    CSSToStyleMap m_styleMap;
    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    State m_state;

    friend class DeprecatedStyleBuilder;
    friend bool operator==(const MatchedProperties&, const MatchedProperties&);
    friend bool operator!=(const MatchedProperties&, const MatchedProperties&);
    friend bool operator==(const MatchRanges&, const MatchRanges&);
    friend bool operator!=(const MatchRanges&, const MatchRanges&);
};

inline bool StyleResolver::hasSelectorForAttribute(const AtomicString &attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    return m_ruleSets.features().attrsInRules.contains(attributeName.impl());
}

inline bool StyleResolver::hasSelectorForClass(const AtomicString& classValue) const
{
    ASSERT(!classValue.isEmpty());
    return m_ruleSets.features().classesInRules.contains(classValue.impl());
}

inline bool StyleResolver::hasSelectorForId(const AtomicString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_ruleSets.features().idsInRules.contains(idValue.impl());
}

inline bool checkRegionSelector(const CSSSelector* regionSelector, Element* regionElement)
{
    if (!regionSelector || !regionElement)
        return false;

    SelectorChecker selectorChecker(regionElement->document(), SelectorChecker::QueryingRules);
    for (const CSSSelector* s = regionSelector; s; s = CSSSelectorList::next(s)) {
        SelectorChecker::SelectorCheckingContext selectorCheckingContext(s, regionElement, SelectorChecker::VisitedMatchDisabled);
        PseudoId ignoreDynamicPseudo = NOPSEUDO;
        if (selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo))
            return true;
    }
    return false;
}

class StyleResolverParentPusher {
public:
    StyleResolverParentPusher(Element* parent)
        : m_parent(parent)
        , m_pushedStyleResolver(0)
    { }
    void push()
    {
        if (m_pushedStyleResolver)
            return;
        m_pushedStyleResolver = &m_parent->document().ensureStyleResolver();
        m_pushedStyleResolver->pushParentElement(m_parent);
    }
    ~StyleResolverParentPusher()
    {
        if (!m_pushedStyleResolver)
            return;
        // This tells us that our pushed style selector is in a bad state,
        // so we should just bail out in that scenario.
        ASSERT(m_pushedStyleResolver == &m_parent->document().ensureStyleResolver());
        if (m_pushedStyleResolver != &m_parent->document().ensureStyleResolver())
            return;
        m_pushedStyleResolver->popParentElement(m_parent);
    }
    
private:
    Element* m_parent;
    StyleResolver* m_pushedStyleResolver;
};

} // namespace WebCore

#endif // StyleResolver_h
