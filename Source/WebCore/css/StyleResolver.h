/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "CSSRule.h"
#include "CSSToStyleMap.h"
#include "CSSValueList.h"
#include "LinkHash.h"
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "RuleFeature.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleInheritedData.h"
#include "StyleScopeResolver.h"
#include "ViewportStyleResolver.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

enum ESmartMinimumForFontSize { DoNotUseSmartMinimumForFontSize, UseSmartMinimumForFontFize };

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
class CSSRuleList;
class CSSSelector;
class CSSStyleRule;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class CustomFilterOperation;
class CustomFilterParameter;
class CustomFilterParameterList;
class Document;
class Element;
class Frame;
class FrameView;
class KURL;
class KeyframeList;
class KeyframeValue;
class MediaQueryEvaluator;
class Node;
class RenderRegion;
class RuleData;
class RuleSet;
class Settings;
class StaticCSSRuleList;
class StyleBuilder;
class StyleScopeResolver;
class StyleImage;
class StyleKeyframe;
class StylePendingImage;
class StylePropertySet;
class StyleRule;
#if ENABLE(SHADOW_DOM)
class StyleRuleHost;
#endif
class StyleRuleKeyframes;
class StyleRulePage;
class StyleRuleRegion;
class StyleShader;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class StyledElement;
class ViewportStyleResolver;
class WebKitCSSFilterValue;
class WebKitCSSShaderValue;
class WebKitCSSSVGDocumentValue;

class MediaQueryResult {
    WTF_MAKE_NONCOPYABLE(MediaQueryResult); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaQueryResult(const MediaQueryExp& expr, bool result)
        : m_expression(expr)
        , m_result(result)
    {
    }
    void reportMemoryUsage(MemoryObjectInfo*) const;

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

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class StyleResolver {
    WTF_MAKE_NONCOPYABLE(StyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    StyleResolver(Document*, bool matchAuthorAndUserStyles);
    ~StyleResolver();

    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element*);
    void popParentElement(Element*);
    void pushParentShadowRoot(const ShadowRoot*);
    void popParentShadowRoot(const ShadowRoot*);
#if ENABLE(SHADOW_DOM)
    void addHostRule(StyleRuleHost* rule, bool hasDocumentSecurityOrigin, const ContainerNode* scope) { ensureScopeResolver()->addHostRule(rule, hasDocumentSecurityOrigin, scope); }
#endif

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules, RenderRegion* regionForStyling = 0);

    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(PseudoId, Element*, RenderStyle* parentStyle);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);
    PassRefPtr<RenderStyle> defaultStyleForElement();
    PassRefPtr<RenderStyle> styleForText(Text*);

    static PassRefPtr<RenderStyle> styleForDocument(Document*, CSSFontSelector* = 0);

    RenderStyle* style() const { return m_style.get(); }
    RenderStyle* parentStyle() const { return m_parentStyle; }
    RenderStyle* rootElementStyle() const { return m_rootElementStyle; }
    Element* element() const { return m_element; }
    Document* document() const { return m_document; }
    const FontDescription& fontDescription() { return style()->fontDescription(); }
    const FontDescription& parentFontDescription() { return parentStyle()->fontDescription(); }
    void setFontDescription(const FontDescription& fontDescription) { m_fontDirty |= style()->setFontDescription(fontDescription); }
    void setZoom(float f) { m_fontDirty |= style()->setZoom(f); }
    void setEffectiveZoom(float f) { m_fontDirty |= style()->setEffectiveZoom(f); }
    void setTextSizeAdjust(bool b) { m_fontDirty |= style()->setTextSizeAdjust(b); }
    void setWritingMode(WritingMode writingMode) { m_fontDirty |= style()->setWritingMode(writingMode); }
    void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= style()->setTextOrientation(textOrientation); }
    bool hasParentNode() const { return m_parentNode; }

    void resetAuthorStyle();
    void appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >&);

private:
#if ENABLE(STYLE_SCOPED) || ENABLE(SHADOW_DOM)
    StyleScopeResolver* ensureScopeResolver()
    {
#if ENABLE(STYLE_SCOPED)
#if ENABLE(SHADOW_DOM)
        ASSERT(RuntimeEnabledFeatures::shadowDOMEnabled() || RuntimeEnabledFeatures::styleScopedEnabled());
#else
        ASSERT(RuntimeEnabledFeatures::styleScopedEnabled());
#endif
#else
        ASSERT(RuntimeEnabledFeatures::shadowDOMEnabled());
#endif
        if (!m_scopeResolver)
            m_scopeResolver = adoptPtr(new StyleScopeResolver());
        return m_scopeResolver.get();
    }
#endif

    void initForStyleResolve(Element*, RenderStyle* parentStyle = 0, PseudoId = NOPSEUDO);
    void initElement(Element*);
    void collectFeatures();
    RenderStyle* locateSharedStyle();
    bool styleSharingCandidateMatchesRuleSet(RuleSet*);
    bool styleSharingCandidateMatchesHostRules();
    Node* locateCousinList(Element* parent, unsigned& visitedNodeCount) const;
    StyledElement* findSiblingForStyleSharing(Node*, unsigned& count) const;
    bool canShareStyleWithElement(StyledElement*) const;

    PassRefPtr<RenderStyle> styleForKeyframe(const RenderStyle*, const StyleKeyframe*, KeyframeValue&);

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
    PassRefPtr<CSSRuleList> styleRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtr<CSSRuleList> pseudoStyleRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);

    // Given a CSS keyword in the range (xx-small to -webkit-xxx-large), this function will return
    // the correct font size scaled relative to the user's default (medium).
    static float fontSizeForKeyword(Document*, int keyword, bool shouldUseFixedDefaultSize);

    // Given a font size in pixel, this function will return legacy font size between 1 and 7.
    static int legacyFontSize(Document*, int pixelFontSize, bool shouldUseFixedDefaultSize);

public:
    void applyPropertyToStyle(CSSPropertyID, CSSValue*, RenderStyle*);

    void applyPropertyToCurrentStyle(CSSPropertyID, CSSValue*);

    void updateFont();
    void initializeFontStyle(Settings*);

    static float getComputedSizeFromSpecifiedSize(Document*, float zoomFactor, bool isAbsoluteSize, float specifiedSize, ESmartMinimumForFontSize = UseSmartMinimumForFontFize);

    void setFontSize(FontDescription&, float size);

private:
    static float getComputedSizeFromSpecifiedSize(Document*, RenderStyle*, bool isAbsoluteSize, float specifiedSize, bool useSVGZoomRules);

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

    bool usesSiblingRules() const { return !m_features.siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_features.usesFirstLineRules; }
    bool usesBeforeAfterRules() const { return m_features.usesBeforeAfterRules; }

    static bool createTransformOperations(CSSValue* inValue, RenderStyle* inStyle, RenderStyle* rootStyle, TransformOperations& outOperations);
    
    void invalidateMatchedPropertiesCache();
    
    // WARNING. This will construct CSSOM wrappers for all style rules and cache then in a map for significant memory cost.
    // It is here to support inspector. Don't use for any regular engine functions.
    CSSStyleRule* ensureFullCSSOMWrapperForInspector(StyleRule*);

#if ENABLE(CSS_FILTERS)
    bool createFilterOperations(CSSValue* inValue, RenderStyle* inStyle, RenderStyle* rootStyle, FilterOperations& outOperations);
#if ENABLE(CSS_SHADERS)
    StyleShader* styleShader(CSSValue*);
    StyleShader* cachedOrPendingStyleShaderFromValue(WebKitCSSShaderValue*);
    bool parseCustomFilterParameterList(CSSValue*, CustomFilterParameterList&);
    PassRefPtr<CustomFilterParameter> parseCustomFilterParameter(const String& name, CSSValue*);
    PassRefPtr<CustomFilterParameter> parseCustomFilterArrayParameter(const String& name, CSSValueList*);
    PassRefPtr<CustomFilterParameter> parseCustomFilterNumberParameter(const String& name, CSSValueList*);
    PassRefPtr<CustomFilterParameter> parseCustomFilterTransformParameter(const String& name, CSSValueList*);
    PassRefPtr<CustomFilterOperation> createCustomFilterOperation(WebKitCSSFilterValue*);
    void loadPendingShaders();
#endif
#if ENABLE(SVG)
    void loadPendingSVGDocuments();
#endif
#endif // ENABLE(CSS_FILTERS)

    void loadPendingResources();

private:
    // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
    void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForZoomChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForTextSizeAdjust();

    void adjustRenderStyle(RenderStyle* styleToAdjust, RenderStyle* parentStyle, Element*);

    void addMatchedRule(const RuleData* rule) { m_matchedRules.append(rule); }

    struct MatchRanges {
        MatchRanges() : firstUARule(-1), lastUARule(-1), firstAuthorRule(-1), lastAuthorRule(-1), firstUserRule(-1), lastUserRule(-1) { }
        int firstUARule;
        int lastUARule;
        int firstAuthorRule;
        int lastAuthorRule;
        int firstUserRule;
        int lastUserRule;
    };

    struct MatchedProperties {
        MatchedProperties() : possiblyPaddedMember(0) { }
        void reportMemoryUsage(MemoryObjectInfo*) const;
        
        RefPtr<StylePropertySet> properties;
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
    };

    struct MatchRequest {
        MatchRequest(RuleSet* ruleSet, bool includeEmptyRules = false, const ContainerNode* scope = 0)
            : ruleSet(ruleSet)
            , includeEmptyRules(includeEmptyRules)
            , scope(scope) { }
        const RuleSet* ruleSet;
        const bool includeEmptyRules;
        const ContainerNode* scope;
    };

    static void addMatchedProperties(MatchResult&, const StylePropertySet* properties, StyleRule* = 0, unsigned linkMatchType = SelectorChecker::MatchAll, PropertyWhitelistType = PropertyWhitelistNone);
    void addElementStyleProperties(MatchResult&, const StylePropertySet*, bool isCacheable = true);

    void matchAllRules(MatchResult&, bool includeSMILProperties);
    void matchUARules(MatchResult&);
    void matchUARules(MatchResult&, RuleSet*);
    void matchAuthorRules(MatchResult&, bool includeEmptyRules);
    void matchUserRules(MatchResult&, bool includeEmptyRules);
    void matchScopedAuthorRules(MatchResult&, bool includeEmptyRules);
    void matchHostRules(MatchResult&, bool includeEmptyRules);

    void collectMatchingRules(const MatchRequest&, int& firstRuleIndex, int& lastRuleIndex);
    void collectMatchingRulesForRegion(const MatchRequest&, int& firstRuleIndex, int& lastRuleIndex);
    void collectMatchingRulesForList(const Vector<RuleData>*, const MatchRequest&, int& firstRuleIndex, int& lastRuleIndex);

    bool fastRejectSelector(const RuleData&) const;
    void sortMatchedRules();
    void sortAndTransferMatchedRules(MatchResult&);

    bool ruleMatches(const RuleData&, const ContainerNode* scope);
    bool checkRegionSelector(CSSSelector* regionSelector, Element* regionElement);
    void applyMatchedProperties(const MatchResult&, const Element*);
    enum StyleApplicationPass {
#if ENABLE(CSS_VARIABLES)
        VariableDefinitions,
#endif
        HighPriorityProperties,
        LowPriorityProperties
    };
    template <StyleApplicationPass pass>
    void applyMatchedProperties(const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);
    template <StyleApplicationPass pass>
    void applyProperties(const StylePropertySet* properties, StyleRule*, bool isImportant, bool inheritedOnly, PropertyWhitelistType = PropertyWhitelistNone);
#if ENABLE(CSS_VARIABLES)
    void resolveVariables(CSSPropertyID, CSSValue*, Vector<std::pair<CSSPropertyID, String> >& knownExpressions);
#endif
    static bool isValidRegionStyleProperty(CSSPropertyID);
#if ENABLE(VIDEO_TRACK)
    static bool isValidCueStyleProperty(CSSPropertyID);
#endif
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);
    Settings* documentSettings() { return m_document->settings(); }

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    OwnPtr<RuleSet> m_authorStyle;
    OwnPtr<RuleSet> m_userStyle;

    RuleFeatureSet m_features;
    OwnPtr<RuleSet> m_siblingRuleSet;
    OwnPtr<RuleSet> m_uncommonAttributeRuleSet;

    bool m_hasUAAppearance;
    BorderData m_borderData;
    FillLayer m_backgroundData;
    Color m_backgroundColor;

    typedef HashMap<AtomicStringImpl*, RefPtr<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

public:
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue*);
    PassRefPtr<StyleImage> cachedOrPendingFromValue(CSSPropertyID, CSSImageValue*);
    PassRefPtr<StyleImage> generatedOrPendingFromValue(CSSPropertyID, CSSImageGeneratorValue*);
#if ENABLE(CSS_IMAGE_SET)
    PassRefPtr<StyleImage> setOrPendingFromValue(CSSPropertyID, CSSImageSetValue*);
#endif
    PassRefPtr<StyleImage> cursorOrPendingFromValue(CSSPropertyID, CSSCursorImageValue*);

    bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
    bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }

    static Length convertToIntLength(CSSPrimitiveValue*, RenderStyle*, RenderStyle* rootStyle, double multiplier = 1);
    static Length convertToFloatLength(CSSPrimitiveValue*, RenderStyle*, RenderStyle* rootStyle, double multiplier = 1);

    CSSToStyleMap* styleMap() { return &m_styleMap; }

    void reportMemoryUsage(MemoryObjectInfo*) const;
    
private:
    static RenderStyle* s_styleNotYetAvailable;

    void collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet> >&, RuleSet& userStyle);

    void cacheBorderAndBackground();

private:
    bool canShareStyleWithControl(StyledElement*) const;

    void applyProperty(CSSPropertyID, CSSValue*);

#if ENABLE(SVG)
    void applySVGProperty(CSSPropertyID, CSSValue*);
#endif

    PassRefPtr<StyleImage> loadPendingImage(StylePendingImage*);
    void loadPendingImages();

    static unsigned computeMatchedPropertiesHash(const MatchedProperties*, unsigned size);
    struct MatchedPropertiesCacheItem {
        void reportMemoryUsage(MemoryObjectInfo*) const;
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

    // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
    // merge sorting.
    Vector<const RuleData*, 32> m_matchedRules;

    RefPtr<StaticCSSRuleList> m_ruleList;

    typedef HashMap<CSSPropertyID, RefPtr<CSSValue> > PendingImagePropertyMap;
    PendingImagePropertyMap m_pendingImageProperties;

    OwnPtr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    PseudoId m_dynamicPseudo;
    PseudoId m_pseudoStyle;

    Document* m_document;
    SelectorChecker m_selectorChecker;
    SelectorFilter m_selectorFilter;

    RefPtr<RenderStyle> m_style;
    RenderStyle* m_parentStyle;
    RenderStyle* m_rootElementStyle;
    Element* m_element;
    StyledElement* m_styledElement;
    RenderRegion* m_regionForStyling;
    EInsideLink m_elementLinkState;
    bool m_elementAffectedByClassRules;
    ContainerNode* m_parentNode;
    CSSValue* m_lineHeightValue;
    bool m_fontDirty;
    bool m_matchAuthorAndUserStyles;
    bool m_sameOriginOnly;
    bool m_distributedToInsertionPoint;

    RefPtr<CSSFontSelector> m_fontSelector;
    Vector<OwnPtr<MediaQueryResult> > m_viewportDependentMediaQueryResults;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;
#endif

    bool m_applyPropertyToRegularStyle;
    bool m_applyPropertyToVisitedLinkStyle;
    const StyleBuilder& m_styleBuilder;
    
    HashMap<StyleRule*, RefPtr<CSSStyleRule> > m_styleRuleToCSSOMWrapperMap;
    HashSet<RefPtr<CSSStyleSheet> > m_styleSheetCSSOMWrapperSet;

#if ENABLE(CSS_SHADERS)
    bool m_hasPendingShaders;
#endif

#if ENABLE(CSS_FILTERS) && ENABLE(SVG)
    HashMap<FilterOperation*, RefPtr<WebKitCSSSVGDocumentValue> > m_pendingSVGDocuments;
#endif

    OwnPtr<StyleScopeResolver> m_scopeResolver;
    CSSToStyleMap m_styleMap;

    friend class StyleBuilder;
    friend bool operator==(const MatchedProperties&, const MatchedProperties&);
    friend bool operator!=(const MatchedProperties&, const MatchedProperties&);
    friend bool operator==(const MatchRanges&, const MatchRanges&);
    friend bool operator!=(const MatchRanges&, const MatchRanges&);
};

inline bool StyleResolver::hasSelectorForAttribute(const AtomicString &attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    return m_features.attrsInRules.contains(attributeName.impl());
}

inline bool StyleResolver::hasSelectorForClass(const AtomicString& classValue) const
{
    ASSERT(!classValue.isEmpty());
    return m_features.classesInRules.contains(classValue.impl());
}

inline bool StyleResolver::hasSelectorForId(const AtomicString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_features.idsInRules.contains(idValue.impl());
}

} // namespace WebCore

#endif // StyleResolver_h
