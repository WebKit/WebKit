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

#ifndef CSSStyleSelector_h
#define CSSStyleSelector_h

#include "CSSRule.h"
#include "LinkHash.h"
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "SelectorChecker.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

enum ESmartMinimumForFontSize { DoNotUseSmartMinimumForFontSize, UseSmartMinimumForFontFize };

class CSSFontSelector;
class CSSPageRule;
class CSSPrimitiveValue;
class CSSProperty;
class CSSFontFace;
class CSSFontFaceRule;
class CSSImageGeneratorValue;
class CSSImageValue;
class CSSRuleList;
class CSSSelector;
class CSSStyleApplyProperty;
class CSSStyleRule;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class CustomFilterOperation;
class CustomFilterParameter;
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
class StyleImage;
class StylePendingImage;
class StylePropertySet;
class StyleShader;
class StyleSheet;
class StyleSheetList;
class StyledElement;
class WebKitCSSKeyframeRule;
class WebKitCSSKeyframesRule;
class WebKitCSSFilterValue;
class WebKitCSSRegionRule;
class WebKitCSSShaderValue;

#if ENABLE(CSS_SHADERS)
typedef Vector<RefPtr<CustomFilterParameter> > CustomFilterParameterList;
#endif

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

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class CSSStyleSelector {
    WTF_MAKE_NONCOPYABLE(CSSStyleSelector); WTF_MAKE_FAST_ALLOCATED;
public:
    CSSStyleSelector(Document*, StyleSheetList* authorSheets, CSSStyleSheet* mappedElementSheet,
                     CSSStyleSheet* pageUserSheet, const Vector<RefPtr<CSSStyleSheet> >* pageGroupUserSheets, const Vector<RefPtr<CSSStyleSheet> >* documentUserSheets,
                     bool strictParsing, bool matchAuthorAndUserStyles);
    ~CSSStyleSelector();

#if ENABLE(STYLE_SCOPED)
    static Element* determineScopingElement(const CSSStyleSheet*);
#endif

    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParent(Element* parent);
    void popParent(Element* parent);

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, bool allowSharing = true, bool resolveForRootDefault = false, RenderRegion* regionForStyling = 0);

    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(PseudoId, Element*, RenderStyle* parentStyle = 0);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);

    static PassRefPtr<RenderStyle> styleForDocument(Document*, CSSFontSelector* = 0);

    RenderStyle* style() const { return m_style.get(); }
    RenderStyle* parentStyle() const { return m_parentStyle; }
    RenderStyle* rootElementStyle() const { return m_rootElementStyle; }
    Element* element() const { return m_element; }
    Document* document() const { return m_checker.document(); }
    FontDescription fontDescription() { return style()->fontDescription(); }
    FontDescription parentFontDescription() {return parentStyle()->fontDescription(); }
    void setFontDescription(FontDescription fontDescription) { m_fontDirty |= style()->setFontDescription(fontDescription); }
    void setZoom(float f) { m_fontDirty |= style()->setZoom(f); }
    void setEffectiveZoom(float f) { m_fontDirty |= style()->setEffectiveZoom(f); }
    void setTextSizeAdjust(bool b) { m_fontDirty |= style()->setTextSizeAdjust(b); }
    bool hasParentNode() const { return m_parentNode; }
    
    void appendAuthorStylesheets(unsigned firstNew, const Vector<RefPtr<StyleSheet> >&);
    
    // Find the ids or classes the selectors on a stylesheet are scoped to. The selectors only apply to elements in subtrees where the root element matches the scope.
    static bool determineStylesheetSelectorScopes(CSSStyleSheet*, HashSet<AtomicStringImpl*>& idScopes, HashSet<AtomicStringImpl*>& classScopes);

private:
    void initForStyleResolve(Element*, RenderStyle* parentStyle = 0, PseudoId = NOPSEUDO);
    void initElement(Element*);
    void collectFeatures();
    RenderStyle* locateSharedStyle();
    bool matchesRuleSet(RuleSet*);
    Node* locateCousinList(Element* parent, unsigned& visitedNodeCount) const;
    StyledElement* findSiblingForStyleSharing(Node*, unsigned& count) const;
    bool canShareStyleWithElement(StyledElement*) const;

    PassRefPtr<RenderStyle> styleForKeyframe(const RenderStyle*, const WebKitCSSKeyframeRule*, KeyframeValue&);

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
    void setStyle(PassRefPtr<RenderStyle> s) { m_style = s; } // Used by the document when setting up its root style.

    void applyPropertyToStyle(int id, CSSValue*, RenderStyle*);

    void applyPropertyToCurrentStyle(int id, CSSValue*);

    void updateFont();

    static float getComputedSizeFromSpecifiedSize(Document*, float zoomFactor, bool isAbsoluteSize, float specifiedSize, ESmartMinimumForFontSize = UseSmartMinimumForFontFize);

    void setFontSize(FontDescription&, float size);

private:
    static float getComputedSizeFromSpecifiedSize(Document*, RenderStyle*, bool isAbsoluteSize, float specifiedSize, bool useSVGZoomRules);

public:
    bool useSVGZoomRules();

    Color colorFromPrimitiveValue(CSSPrimitiveValue*, bool forVisitedLink = false) const;

    bool hasSelectorForAttribute(const AtomicString&) const;

    CSSFontSelector* fontSelector() const { return m_fontSelector.get(); }

    void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);

    bool affectedByViewportChange() const;

    void allVisitedStateChanged() { m_checker.allVisitedStateChanged(); }
    void visitedStateChanged(LinkHash visitedHash) { m_checker.visitedStateChanged(visitedHash); }

    void addKeyframeStyle(PassRefPtr<WebKitCSSKeyframesRule>);

    bool checkRegionStyle(Element* regionElement);

    bool usesSiblingRules() const { return !m_features.siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_features.usesFirstLineRules; }
    bool usesBeforeAfterRules() const { return m_features.usesBeforeAfterRules; }
    bool usesLinkRules() const { return m_features.usesLinkRules; }

    static bool createTransformOperations(CSSValue* inValue, RenderStyle* inStyle, RenderStyle* rootStyle, TransformOperations& outOperations);
    
    void invalidateMatchedDeclarationCache();

#if ENABLE(CSS_FILTERS)
    bool createFilterOperations(CSSValue* inValue, RenderStyle* inStyle, RenderStyle* rootStyle, FilterOperations& outOperations);
#if ENABLE(CSS_SHADERS)
    StyleShader* styleShader(CSSValue*);
    StyleShader* cachedOrPendingStyleShaderFromValue(WebKitCSSShaderValue*);
    bool parseCustomFilterParameterList(CSSValue*, CustomFilterParameterList&);
    PassRefPtr<CustomFilterParameter> parseCustomFilterNumberParamter(const String& name, CSSValueList*);
    PassRefPtr<CustomFilterOperation> createCustomFilterOperation(WebKitCSSFilterValue*);
    void loadPendingShaders();
#endif
#endif // ENABLE(CSS_FILTERS)

    struct RuleSelectorPair {
        RuleSelectorPair(CSSStyleRule* rule, CSSSelector* selector) : rule(rule), selector(selector) { }
        CSSStyleRule* rule;
        CSSSelector* selector;
    };
    struct Features {
        Features();
        ~Features();
        void add(const CSSStyleSelector::Features&);
        void clear();
        HashSet<AtomicStringImpl*> idsInRules;
        HashSet<AtomicStringImpl*> attrsInRules;
        Vector<RuleSelectorPair> siblingRules;
        Vector<RuleSelectorPair> uncommonAttributeRules;
        bool usesFirstLineRules;
        bool usesBeforeAfterRules;
        bool usesLinkRules;
    };

private:
    // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
    void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForZoomChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForTextSizeAdjust();

    void adjustRenderStyle(RenderStyle* styleToAdjust, RenderStyle* parentStyle, Element*);

    void addMatchedRule(const RuleData* rule) { m_matchedRules.append(rule); }
    void addMatchedDeclaration(StylePropertySet*, unsigned linkMatchType = SelectorChecker::MatchAll);

    struct MatchRanges {
        MatchRanges() : firstUARule(-1), lastUARule(-1), firstAuthorRule(-1), lastAuthorRule(-1), firstUserRule(-1), lastUserRule(-1) { }
        int firstUARule;
        int lastUARule;
        int firstAuthorRule;
        int lastAuthorRule;
        int firstUserRule;
        int lastUserRule;
    };

    struct MatchResult {
        MatchResult() : isCacheable(true) { }
        MatchRanges ranges;
        bool isCacheable;
    };
    void matchAllRules(MatchResult&);
    void matchUARules(MatchResult&);
    void matchRules(RuleSet*, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    void matchAuthorRules(int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    void matchScopedAuthorRules(int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    void collectMatchingRules(RuleSet*, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    void collectMatchingRulesForRegion(RuleSet*, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    void collectMatchingRulesForList(const Vector<RuleData>*, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules);
    bool fastRejectSelector(const RuleData&) const;
    void sortMatchedRules();
    void sortAndTransferMatchedRules();

    bool checkSelector(const RuleData&);
    bool checkRegionSelector(CSSSelector* regionSelector, Element* regionElement);
    void applyMatchedDeclarations(const MatchResult&);
    template <bool firstPass>
    void applyDeclarations(bool important, int startIndex, int endIndex, bool inheritedOnly);
    template <bool firstPass>
    void applyDeclaration(StylePropertySet*, bool isImportant, bool inheritedOnly);

    static bool isValidRegionStyleProperty(int id);

    void matchPageRules(RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(const Vector<RuleData>*, bool isLeftPage, bool isFirstPage, const String& pageName);
    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    OwnPtr<RuleSet> m_authorStyle;
    OwnPtr<RuleSet> m_userStyle;

    Features m_features;
    OwnPtr<RuleSet> m_siblingRuleSet;
    OwnPtr<RuleSet> m_uncommonAttributeRuleSet;

    bool m_hasUAAppearance;
    BorderData m_borderData;
    FillLayer m_backgroundData;
    Color m_backgroundColor;

    typedef HashMap<AtomicStringImpl*, RefPtr<WebKitCSSKeyframesRule> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

public:
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue*);
    PassRefPtr<StyleImage> cachedOrPendingFromValue(CSSPropertyID, CSSImageValue*);
    PassRefPtr<StyleImage> generatedOrPendingFromValue(CSSPropertyID, CSSImageGeneratorValue*);

    bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
    bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }

private:
    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

    void mapFillAttachment(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillClip(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillComposite(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillOrigin(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillImage(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillRepeatX(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillRepeatY(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillSize(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillXPosition(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillYPosition(CSSPropertyID, FillLayer*, CSSValue*);

    void mapAnimationDelay(Animation*, CSSValue*);
    void mapAnimationDirection(Animation*, CSSValue*);
    void mapAnimationDuration(Animation*, CSSValue*);
    void mapAnimationFillMode(Animation*, CSSValue*);
    void mapAnimationIterationCount(Animation*, CSSValue*);
    void mapAnimationName(Animation*, CSSValue*);
    void mapAnimationPlayState(Animation*, CSSValue*);
    void mapAnimationProperty(Animation*, CSSValue*);
    void mapAnimationTimingFunction(Animation*, CSSValue*);

public:
    void mapNinePieceImage(CSSPropertyID, CSSValue*, NinePieceImage&);
    void mapNinePieceImageSlice(CSSValue*, NinePieceImage&);
    LengthBox mapNinePieceImageQuad(CSSValue*);
    void mapNinePieceImageRepeat(CSSValue*, NinePieceImage&);
private:
    bool canShareStyleWithControl(StyledElement*) const;

    void applyProperty(int id, CSSValue*);
#if ENABLE(SVG)
    void applySVGProperty(int id, CSSValue*);
#endif

    PassRefPtr<StyleImage> loadPendingImage(StylePendingImage*);
    void loadPendingImages();

    struct MatchedStyleDeclaration {
        MatchedStyleDeclaration() : possiblyPaddedMember(0) { }

        RefPtr<StylePropertySet> styleDeclaration;
        union {
            unsigned linkMatchType;
            // Used to make sure all memory is zero-initialized since we compute the hash over the bytes of this object.
            void* possiblyPaddedMember;
        };
    };
    static unsigned computeDeclarationHash(MatchedStyleDeclaration*, unsigned size);
    struct MatchedStyleDeclarationCacheItem {
        Vector<MatchedStyleDeclaration> matchedStyleDeclarations;
        MatchRanges ranges;
        RefPtr<RenderStyle> renderStyle;
        RefPtr<RenderStyle> parentRenderStyle;
    };
    const MatchedStyleDeclarationCacheItem* findFromMatchedDeclarationCache(unsigned hash, const MatchResult&);
    void addToMatchedDeclarationCache(const RenderStyle*, const RenderStyle* parentStyle, unsigned hash, const MatchResult&);

    // Every N additions to the matched declaration cache trigger a sweep where entries holding
    // the last reference to a style declaration are garbage collected.
    void sweepMatchedDeclarationCache();

    // We collect the set of decls that match in |m_matchedDecls|. We then walk the
    // set of matched decls four times, once for those properties that others depend on (like font-size),
    // and then a second time for all the remaining properties. We then do the same two passes
    // for any !important rules.
    Vector<MatchedStyleDeclaration, 64> m_matchedDecls;
    unsigned m_matchedDeclarationCacheAdditionsSinceLastSweep;

    typedef HashMap<unsigned, MatchedStyleDeclarationCacheItem> MatchedStyleDeclarationCache;
    MatchedStyleDeclarationCache m_matchedStyleDeclarationCache;

    // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
    // merge sorting.
    Vector<const RuleData*, 32> m_matchedRules;

    RefPtr<CSSRuleList> m_ruleList;

    HashSet<int> m_pendingImageProperties; // Hash of CSSPropertyIDs

    OwnPtr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    PseudoId m_dynamicPseudo;

    SelectorChecker m_checker;

    RefPtr<RenderStyle> m_style;
    RenderStyle* m_parentStyle;
    RenderStyle* m_rootElementStyle;
    Element* m_element;
    StyledElement* m_styledElement;
    RenderRegion* m_regionForStyling;
    EInsideLink m_elementLinkState;
    ContainerNode* m_parentNode;
    CSSValue* m_lineHeightValue;
    bool m_fontDirty;
    bool m_matchAuthorAndUserStyles;
    bool m_sameOriginOnly;

    RefPtr<CSSFontSelector> m_fontSelector;
    Vector<OwnPtr<MediaQueryResult> > m_viewportDependentMediaQueryResults;

    bool m_applyPropertyToRegularStyle;
    bool m_applyPropertyToVisitedLinkStyle;
    const CSSStyleApplyProperty& m_applyProperty;

#if ENABLE(CSS_SHADERS)
    bool m_hasPendingShaders;
#endif

#if ENABLE(STYLE_SCOPED)
    typedef HashMap<const Element*, OwnPtr<RuleSet> > ScopedRuleSetMap;

    RuleSet* scopedRuleSetForElement(const Element*) const;

    void setupScopingElementStack(const Element*);
    bool scopingElementStackIsConsistent(const Element* parent) const { return parent && parent == m_scopingElementStackParent; }

    ScopedRuleSetMap m_scopedAuthorStyles;
    
    struct ScopeStackFrame {
        ScopeStackFrame() : m_element(0), m_ruleSet(0) { }
        ScopeStackFrame(const Element* element, RuleSet* ruleSet) : m_element(element), m_ruleSet(ruleSet) { }
        const Element* m_element;
        RuleSet* m_ruleSet;
    };
    // Vector (used as stack) that keeps track of scoping elements (i.e., elements with a <style scoped> child)
    // encountered during tree iteration for style resolution.
    Vector<ScopeStackFrame> m_scopingElementStack;
    // Element last seen as parent element when updating m_scopingElementStack.
    // This is used to decide whether m_scopingElementStack is consistent, separately from SelectorChecker::m_parentStack.
    const Element* m_scopingElementStackParent;
#endif

    friend class CSSStyleApplyProperty;
    friend bool operator==(const MatchedStyleDeclaration&, const MatchedStyleDeclaration&);
    friend bool operator!=(const MatchedStyleDeclaration&, const MatchedStyleDeclaration&);
    friend bool operator==(const MatchRanges&, const MatchRanges&);
    friend bool operator!=(const MatchRanges&, const MatchRanges&);
};

} // namespace WebCore

#endif // CSSStyleSelector_h
