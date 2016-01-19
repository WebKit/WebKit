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

#include "CSSToLengthConversionData.h"
#include "CSSToStyleMap.h"
#include "CSSValueList.h"
#include "DocumentRuleSets.h"
#include "InspectorCSSOMWrappers.h"
#include "LinkHash.h"
#include "MediaQueryEvaluator.h"
#include "RenderStyle.h"
#include "RuleFeature.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "ScrollTypes.h"
#include "SelectorChecker.h"
#include "StyleInheritedData.h"
#include "ViewportStyleResolver.h"
#include <bitset>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSCursorImageValue;
class CSSFontFace;
class CSSFontFaceRule;
class CSSFontValue;
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
class SelectorFilter;
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
class SVGSVGElement;
class ViewportStyleResolver;
class WebKitCSSFilterValue;
struct ResourceLoaderOptions;

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

enum CascadeLevel {
    UserAgentLevel,
    AuthorLevel,
    UserLevel
};

class PseudoStyleRequest {
public:
    PseudoStyleRequest(PseudoId pseudoId, RenderScrollbar* scrollbar = nullptr, ScrollbarPart scrollbarPart = NoPart)
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
    StyleResolver(Document&);
    ~StyleResolver();

    Ref<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules, const RenderRegion* regionForStyling = nullptr, const SelectorFilter* = nullptr);

    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    Ref<RenderStyle> styleForPage(int pageIndex);
    Ref<RenderStyle> defaultStyleForElement();

    RenderStyle* style() const { return m_state.style(); }
    RenderStyle* parentStyle() const { return m_state.parentStyle(); }
    RenderStyle* rootElementStyle() const { return m_state.rootElementStyle(); }
    Element* element() { return m_state.element(); }
    Document& document() { return m_document; }
    Settings* documentSettings() { return m_document.settings(); }

    void appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&);

    DocumentRuleSets& ruleSets() { return m_ruleSets; }
    const DocumentRuleSets& ruleSets() const { return m_ruleSets; }

    const MediaQueryEvaluator& mediaQueryEvaluator() const { return *m_medium; }

private:
    void initElement(Element*);
    RenderStyle* locateSharedStyle();
    bool styleSharingCandidateMatchesRuleSet(RuleSet*);
    Node* locateCousinList(Element* parent, unsigned& visitedNodeCount) const;
    StyledElement* findSiblingForStyleSharing(Node*, unsigned& count) const;
    bool canShareStyleWithElement(StyledElement&) const;

    Ref<RenderStyle> styleForKeyframe(const RenderStyle*, const StyleKeyframe*, KeyframeValue&);

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
    Vector<RefPtr<StyleRule>> styleRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    Vector<RefPtr<StyleRule>> pseudoStyleRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);

public:
    struct MatchResult;

    void applyPropertyToStyle(CSSPropertyID, CSSValue*, RenderStyle*);

    void applyPropertyToCurrentStyle(CSSPropertyID, CSSValue*);

    void updateFont();
    void initializeFontStyle(Settings*);

    void setFontSize(FontCascadeDescription&, float size);

public:
    bool useSVGZoomRules();
    bool useSVGZoomRulesForLength();

    static bool colorFromPrimitiveValueIsDerivedFromElement(CSSPrimitiveValue&);
    Color colorFromPrimitiveValue(CSSPrimitiveValue&, bool forVisitedLink = false) const;

    bool hasSelectorForId(const AtomicString&) const;
    bool hasSelectorForClass(const AtomicString&) const;
    bool hasSelectorForAttribute(const Element&, const AtomicString&) const;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }
#endif

    void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool hasMediaQueriesAffectedByViewportChange() const;

    void addKeyframeStyle(PassRefPtr<StyleRuleKeyframes>);

    bool checkRegionStyle(Element* regionElement);

    bool usesFirstLineRules() const { return m_ruleSets.features().usesFirstLineRules; }
    bool usesFirstLetterRules() const { return m_ruleSets.features().usesFirstLetterRules; }
    
    void invalidateMatchedPropertiesCache();

    void clearCachedPropertiesAffectedByViewportUnits();

    bool createFilterOperations(CSSValue& inValue, FilterOperations& outOperations);
    void loadPendingSVGDocuments();

    void loadPendingResources();

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
        Vector<StyleRule*, 64> matchedRules;
        MatchRanges ranges;
        bool isCacheable;

        const Vector<MatchedProperties, 64>& matchedProperties() const { return m_matchedProperties; }

        void addMatchedProperties(const StyleProperties&, StyleRule* = nullptr, unsigned linkMatchType = SelectorChecker::MatchAll, PropertyWhitelistType = PropertyWhitelistNone);
    private:
        Vector<MatchedProperties, 64> m_matchedProperties;
    };
    
    class CascadedProperties {
    public:
        CascadedProperties(TextDirection, WritingMode);

        struct Property {
            void apply(StyleResolver&, const MatchResult*);

            CSSPropertyID id;
            CascadeLevel level;
            CSSValue* cssValue[3];
        };

        bool hasProperty(CSSPropertyID) const;
        Property& property(CSSPropertyID);
        void addMatches(const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly = false);

        void set(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel);
        void setDeferred(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel);

        void applyDeferredProperties(StyleResolver&, const MatchResult*);

        HashMap<AtomicString, Property>& customProperties() { return m_customProperties; }
        bool hasCustomProperty(const String&) const;
        Property customProperty(const String&) const;
        
    private:
        void addStyleProperties(const StyleProperties&, StyleRule&, bool isImportant, bool inheritedOnly, PropertyWhitelistType, unsigned linkMatchType, CascadeLevel);
        static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel);

        Property m_properties[numCSSProperties + 2];
        std::bitset<numCSSProperties + 2> m_propertyIsPresent;

        Vector<Property, 8> m_deferredProperties;
        HashMap<AtomicString, Property> m_customProperties;

        TextDirection m_direction;
        WritingMode m_writingMode;
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
    std::unique_ptr<GridPosition> adjustNamedGridItemPosition(const NamedGridAreaMap&, const NamedGridLinesMap&, const GridPosition&, GridPositionSide) const;
#endif
    
    void adjustStyleForInterCharacterRuby();
    
    bool fastRejectSelector(const RuleData&) const;

    enum ShouldUseMatchedPropertiesCache { DoNotUseMatchedPropertiesCache = 0, UseMatchedPropertiesCache };
    void applyMatchedProperties(const MatchResult&, const Element*, ShouldUseMatchedPropertiesCache = UseMatchedPropertiesCache);

    void applyCascadedProperties(CascadedProperties&, int firstProperty, int lastProperty, const MatchResult*);
    void cascadeMatches(CascadedProperties&, const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);

    static bool isValidRegionStyleProperty(CSSPropertyID);
#if ENABLE(VIDEO_TRACK)
    static bool isValidCueStyleProperty(CSSPropertyID);
#endif
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);

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
            : m_element(nullptr)
            , m_styledElement(nullptr)
            , m_parentStyle(nullptr)
            , m_rootElementStyle(nullptr)
            , m_regionForStyling(nullptr)
            , m_elementLinkState(NotInsideLink)
            , m_elementAffectedByClassRules(false)
            , m_applyPropertyToRegularStyle(true)
            , m_applyPropertyToVisitedLinkStyle(false)
            , m_fontDirty(false)
            , m_fontSizeHasViewportUnits(false)
            , m_hasUAAppearance(false)
            , m_backgroundData(BackgroundFillLayer)
        {
        }

    public:
        void initElement(Element*);
        void initForStyleResolve(Document&, Element*, RenderStyle* parentStyle, const RenderRegion* regionForStyling = nullptr, const SelectorFilter* = nullptr);
        void clear();

        Document& document() const { return m_element->document(); }
        Element* element() const { return m_element; }
        StyledElement* styledElement() const { return m_styledElement; }
        void setStyle(Ref<RenderStyle>&&);
        RenderStyle* style() const { return m_style.get(); }
        Ref<RenderStyle> takeStyle() { return m_style.releaseNonNull(); }

        void setParentStyle(Ref<RenderStyle>&& parentStyle) { m_parentStyle = WTFMove(parentStyle); }
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
        
        Vector<RefPtr<ReferenceFilterOperation>>& filtersWithPendingSVGDocuments() { return m_filtersWithPendingSVGDocuments; }

        void setFontDirty(bool isDirty) { m_fontDirty = isDirty; }
        bool fontDirty() const { return m_fontDirty; }
        void setFontSizeHasViewportUnits(bool hasViewportUnits) { m_fontSizeHasViewportUnits = hasViewportUnits; }
        bool fontSizeHasViewportUnits() const { return m_fontSizeHasViewportUnits; }

        void cacheBorderAndBackground();
        bool hasUAAppearance() const { return m_hasUAAppearance; }
        BorderData borderData() const { return m_borderData; }
        FillLayer backgroundData() const { return m_backgroundData; }
        Color backgroundColor() const { return m_backgroundColor; }

        const FontCascadeDescription& fontDescription() { return m_style->fontDescription(); }
        const FontCascadeDescription& parentFontDescription() { return m_parentStyle->fontDescription(); }
        void setFontDescription(const FontCascadeDescription& fontDescription) { m_fontDirty |= m_style->setFontDescription(fontDescription); }
        void setZoom(float f) { m_fontDirty |= m_style->setZoom(f); }
        void setEffectiveZoom(float f) { m_fontDirty |= m_style->setEffectiveZoom(f); }
        void setWritingMode(WritingMode writingMode) { m_fontDirty |= m_style->setWritingMode(writingMode); }
        void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= m_style->setTextOrientation(textOrientation); }

        bool useSVGZoomRules() const { return m_element && m_element->isSVGElement(); }

        CSSToLengthConversionData cssToLengthConversionData() const { return m_cssToLengthConversionData; }

        CascadeLevel cascadeLevel() const { return m_cascadeLevel; }
        void setCascadeLevel(CascadeLevel level) { m_cascadeLevel = level; }
        
        CascadedProperties* authorRollback() const { return m_authorRollback.get(); }
        CascadedProperties* userRollback() const { return m_userRollback.get(); }
        
        void setAuthorRollback(std::unique_ptr<CascadedProperties>& rollback) { m_authorRollback = WTFMove(rollback); }
        void setUserRollback(std::unique_ptr<CascadedProperties>& rollback) { m_userRollback = WTFMove(rollback); }

        const SelectorFilter* selectorFilter() const { return m_selectorFilter; }
        
    private:
        void updateConversionData();

        Element* m_element;
        RefPtr<RenderStyle> m_style;
        StyledElement* m_styledElement;
        RefPtr<RenderStyle> m_parentStyle;
        RenderStyle* m_rootElementStyle;

        // Required to ASSERT in applyProperties.
        const RenderRegion* m_regionForStyling;
        
        EInsideLink m_elementLinkState;

        bool m_elementAffectedByClassRules;

        bool m_applyPropertyToRegularStyle;
        bool m_applyPropertyToVisitedLinkStyle;

        PendingImagePropertyMap m_pendingImageProperties;

        Vector<RefPtr<ReferenceFilterOperation>> m_filtersWithPendingSVGDocuments;

        bool m_fontDirty;
        bool m_fontSizeHasViewportUnits;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        FillLayer m_backgroundData;
        Color m_backgroundColor;

        CSSToLengthConversionData m_cssToLengthConversionData;
        
        CascadeLevel m_cascadeLevel { UserAgentLevel };
        std::unique_ptr<CascadedProperties> m_authorRollback;
        std::unique_ptr<CascadedProperties> m_userRollback;

        const SelectorFilter* m_selectorFilter { nullptr };
    };

    State& state() { return m_state; }

    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue&);
    PassRefPtr<StyleImage> cachedOrPendingFromValue(CSSPropertyID, CSSImageValue&);
    PassRefPtr<StyleImage> generatedOrPendingFromValue(CSSPropertyID, CSSImageGeneratorValue&);
#if ENABLE(CSS_IMAGE_SET)
    PassRefPtr<StyleImage> setOrPendingFromValue(CSSPropertyID, CSSImageSetValue&);
#endif
    PassRefPtr<StyleImage> cursorOrPendingFromValue(CSSPropertyID, CSSCursorImageValue&);

    bool applyPropertyToRegularStyle() const { return m_state.applyPropertyToRegularStyle(); }
    bool applyPropertyToVisitedLinkStyle() const { return m_state.applyPropertyToVisitedLinkStyle(); }

    CascadeLevel cascadeLevel() const { return m_state.cascadeLevel(); }
    void setCascadeLevel(CascadeLevel level) { m_state.setCascadeLevel(level); }
    
    CascadedProperties* cascadedPropertiesForRollback(const MatchResult&);

    CSSToStyleMap* styleMap() { return &m_styleMap; }
    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }
    const FontCascadeDescription& fontDescription() { return m_state.fontDescription(); }
    const FontCascadeDescription& parentFontDescription() { return m_state.parentFontDescription(); }
    void setFontDescription(const FontCascadeDescription& fontDescription) { m_state.setFontDescription(fontDescription); }
    void setZoom(float f) { m_state.setZoom(f); }
    void setEffectiveZoom(float f) { m_state.setEffectiveZoom(f); }
    void setWritingMode(WritingMode writingMode) { m_state.setWritingMode(writingMode); }
    void setTextOrientation(TextOrientation textOrientation) { m_state.setTextOrientation(textOrientation); }

private:
    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

    bool canShareStyleWithControl(StyledElement&) const;

    void applyProperty(CSSPropertyID, CSSValue*, SelectorChecker::LinkMatchMask = SelectorChecker::MatchDefault, const MatchResult* = nullptr);
    RefPtr<CSSValue> resolvedVariableValue(CSSPropertyID, const CSSVariableDependentValue&);

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
    void sweepMatchedPropertiesCache();

    bool classNamesAffectedByRules(const SpaceSplitString&) const;
    bool sharingCandidateHasIdenticalStyleAffectingAttributes(StyledElement&) const;

    unsigned m_matchedPropertiesCacheAdditionsSinceLastSweep;

    typedef HashMap<unsigned, MatchedPropertiesCacheItem> MatchedPropertiesCache;
    MatchedPropertiesCache m_matchedPropertiesCache;

    Timer m_matchedPropertiesCacheSweepTimer;

    std::unique_ptr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    Document& m_document;

    bool m_matchAuthorAndUserStyles;

    Vector<std::unique_ptr<MediaQueryResult>> m_viewportDependentMediaQueryResults;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;
#endif

    CSSToStyleMap m_styleMap;
    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    State m_state;

    // Try to catch a crash. https://bugs.webkit.org/show_bug.cgi?id=141561.
    bool m_inLoadPendingImages { false };

    friend bool operator==(const MatchedProperties&, const MatchedProperties&);
    friend bool operator!=(const MatchedProperties&, const MatchedProperties&);
    friend bool operator==(const MatchRanges&, const MatchRanges&);
    friend bool operator!=(const MatchRanges&, const MatchRanges&);
};

inline bool StyleResolver::hasSelectorForAttribute(const Element& element, const AtomicString &attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    if (element.isHTMLElement())
        return m_ruleSets.features().attributeCanonicalLocalNamesInRules.contains(attributeName.impl());
    return m_ruleSets.features().attributeLocalNamesInRules.contains(attributeName.impl());
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

    SelectorChecker selectorChecker(regionElement->document());
    for (const CSSSelector* s = regionSelector; s; s = CSSSelectorList::next(s)) {
        SelectorChecker::CheckingContext selectorCheckingContext(SelectorChecker::Mode::QueryingRules);
        unsigned ignoredSpecificity;
        if (selectorChecker.match(*s, *regionElement, selectorCheckingContext, ignoredSpecificity))
            return true;
    }
    return false;
}

} // namespace WebCore

#endif // StyleResolver_h
