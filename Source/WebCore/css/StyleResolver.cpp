/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2014 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
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
#include "StyleResolver.h"

#include "CSSBorderImage.h"
#include "CSSCalculationValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSDefaultStyleSheets.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPageRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSShadowValue.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableDependentValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "CalculationValue.h"
#include "ContentData.h"
#include "Counter.h"
#include "CounterContent.h"
#include "CursorList.h"
#include "ElementRuleCollector.h"
#include "FilterOperation.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLIFrameElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLProgressElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "LinkHash.h"
#include "LocaleToScriptMapping.h"
#include "MathMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageRuleCollector.h"
#include "Pair.h"
#include "PseudoElement.h"
#include "QuotesData.h"
#include "Rect.h"
#include "RenderGrid.h"
#include "RenderRegion.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderStyleConstants.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowData.h"
#include "ShadowRoot.h"
#include "StyleBuilder.h"
#include "StyleCachedImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StylePendingImage.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleScrollSnapPoints.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "TransformFunctions.h"
#include "TransformOperations.h"
#include "UserAgentStyleSheets.h"
#include "ViewportStyleResolver.h"
#include "VisitedLinkState.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSTransformValue.h"
#include "WebKitFontFamilyNames.h"
#include "XMLNames.h"
#include <bitset>
#include <wtf/StdLibExtras.h>
#include <wtf/TemporaryChange.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#endif

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#include "StyleCachedImageSet.h"
#endif

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "WebVTTElement.h"
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "LengthRepeat.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static const CSSPropertyID lastHighPriorityProperty = CSSPropertyFontSynthesis;
static const CSSPropertyID firstLowPriorityProperty = static_cast<CSSPropertyID>(lastHighPriorityProperty + 1);

static void extractDirectionAndWritingMode(const RenderStyle&, const StyleResolver::MatchResult&, TextDirection&, WritingMode&);

inline void StyleResolver::State::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = *m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

inline void StyleResolver::State::clear()
{
    m_element = nullptr;
    m_parentStyle = nullptr;
    m_regionForStyling = nullptr;
    m_pendingImageProperties.clear();
    m_filtersWithPendingSVGDocuments.clear();
    m_cssToLengthConversionData = CSSToLengthConversionData();
}

void StyleResolver::MatchResult::addMatchedProperties(const StyleProperties& properties, StyleRule* rule, unsigned linkMatchType, PropertyWhitelistType propertyWhitelistType)
{
    m_matchedProperties.grow(m_matchedProperties.size() + 1);
    StyleResolver::MatchedProperties& newProperties = m_matchedProperties.last();
    newProperties.properties = const_cast<StyleProperties*>(&properties);
    newProperties.linkMatchType = linkMatchType;
    newProperties.whitelistType = propertyWhitelistType;
    matchedRules.append(rule);

    if (isCacheable) {
        for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
            // Currently the property cache only copy the non-inherited values and resolve
            // the inherited ones.
            // Here we define some exception were we have to resolve some properties that are not inherited
            // by default. If those exceptions become too common on the web, it should be possible
            // to build a list of exception to resolve instead of completely disabling the cache.

            StyleProperties::PropertyReference current = properties.propertyAt(i);
            if (!current.isInherited()) {
                // If the property value is explicitly inherited, we need to apply further non-inherited properties
                // as they might override the value inherited here. For this reason we don't allow declarations with
                // explicitly inherited properties to be cached.
                const CSSValue& value = *current.value();
                if (value.isInheritedValue()) {
                    isCacheable = false;
                    break;
                }

                // The value currentColor has implicitely the same side effect. It depends on the value of color,
                // which is an inherited value, making the non-inherited property implicitely inherited.
                if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueCurrentcolor) {
                    isCacheable = false;
                    break;
                }
            }
        }
    }
}

StyleResolver::StyleResolver(Document& document)
    : m_matchedPropertiesCacheAdditionsSinceLastSweep(0)
    , m_matchedPropertiesCacheSweepTimer(*this, &StyleResolver::sweepMatchedPropertiesCache)
    , m_document(document)
    , m_matchAuthorAndUserStyles(m_document.settings() ? m_document.settings()->authorAndUserStylesEnabled() : true)
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
#endif
    , m_styleMap(this)
{
    Element* root = m_document.documentElement();

    CSSDefaultStyleSheets::initDefaultStyle(root);

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = m_document.view();
    if (view)
        m_medium = std::make_unique<MediaQueryEvaluator>(view->mediaType());
    else
        m_medium = std::make_unique<MediaQueryEvaluator>("all");

    if (root)
        m_rootDefaultStyle = styleForElement(*root, m_document.renderStyle(), MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = std::make_unique<MediaQueryEvaluator>(view->mediaType(), &view->frame(), m_rootDefaultStyle.get());

    m_ruleSets.resetAuthorStyle();

    m_ruleSets.initUserStyle(m_document.extensionStyleSheets(), *m_medium, *this);

#if ENABLE(SVG_FONTS)
    if (m_document.svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = m_document.svgExtensions()->svgFontFaceElements();
        for (auto* svgFontFaceElement : svgFontFaceElements)
            m_document.fontSelector().addFontFaceRule(svgFontFaceElement->fontFaceRule(), svgFontFaceElement->isInUserAgentShadowTree());
    }
#endif
}

void StyleResolver::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets)
{
    m_ruleSets.appendAuthorStyleSheets(styleSheets, m_medium.get(), m_inspectorCSSOMWrappers, this);
    if (auto renderView = document().renderView())
        renderView->style().fontCascade().update(&document().fontSelector());

#if ENABLE(CSS_DEVICE_ADAPTATION)
    viewportStyleResolver()->resolve();
#endif
}

// This is a simplified style setting function for keyframe styles
void StyleResolver::addKeyframeStyle(PassRefPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), rule);
}

StyleResolver::~StyleResolver()
{
    RELEASE_ASSERT(!m_inLoadPendingImages);

#if ENABLE(CSS_DEVICE_ADAPTATION)
    m_viewportStyleResolver->clearDocument();
#endif
}

void StyleResolver::sweepMatchedPropertiesCache()
{
    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to generate a new inlineStyle()
    // or presentationAttributeStyle(), potentially leaving this cache with the last ref on the old one.
    Vector<unsigned, 16> toRemove;
    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.begin();
    MatchedPropertiesCache::iterator end = m_matchedPropertiesCache.end();
    for (; it != end; ++it) {
        Vector<MatchedProperties>& matchedProperties = it->value.matchedProperties;
        for (size_t i = 0; i < matchedProperties.size(); ++i) {
            if (matchedProperties[i].properties->hasOneRef()) {
                toRemove.append(it->key);
                break;
            }
        }
    }
    for (size_t i = 0; i < toRemove.size(); ++i)
        m_matchedPropertiesCache.remove(toRemove[i]);

    m_matchedPropertiesCacheAdditionsSinceLastSweep = 0;
}

StyleResolver::State::State(Element& element, RenderStyle* parentStyle, const RenderRegion* regionForStyling, const SelectorFilter* selectorFilter)
    : m_element(&element)
    , m_parentStyle(parentStyle)
    , m_regionForStyling(regionForStyling)
    , m_elementLinkState(element.document().visitedLinkState().determineLinkState(element))
    , m_selectorFilter(selectorFilter)
{
    bool resetStyleInheritance = hasShadowRootParent(element) && downcast<ShadowRoot>(element.parentNode())->resetStyleInheritance();
    if (resetStyleInheritance)
        m_parentStyle = nullptr;

    auto& document = element.document();
    auto* documentElement = document.documentElement();
    m_rootElementStyle = (!documentElement || documentElement == &element) ? document.renderStyle() : documentElement->renderStyle();

    updateConversionData();
}

inline void StyleResolver::State::updateConversionData()
{
    m_cssToLengthConversionData = CSSToLengthConversionData(m_style.get(), m_rootElementStyle, m_element ? document().renderView() : nullptr);
}

inline void StyleResolver::State::setStyle(Ref<RenderStyle>&& style)
{
    m_style = WTFMove(style);
    updateConversionData();
}
static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

Ref<RenderStyle> StyleResolver::styleForElement(Element& element, RenderStyle* parentStyle, RuleMatchingBehavior matchingBehavior, const RenderRegion* regionForStyling, const SelectorFilter* selectorFilter)
{
    RELEASE_ASSERT(!m_inLoadPendingImages);

    m_state = State(element, parentStyle, regionForStyling, selectorFilter);
    State& state = m_state;

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle(), isAtShadowBoundary(&element) ? RenderStyle::AtShadowBoundary : RenderStyle::NotAtShadowBoundary);
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    if (element.isLink()) {
        state.style()->setIsLink(true);
        EInsideLink linkState = state.elementLinkState();
        if (linkState != NotInsideLink) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassVisited);
            if (forceVisited)
                linkState = InsideVisitedLink;
        }
        state.style()->setInsideLink(linkState);
    }

    CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(element);

    ElementRuleCollector collector(element, state.style(), m_ruleSets, m_state.selectorFilter());
    collector.setRegionForStyling(regionForStyling);
    collector.setMedium(m_medium.get());

    if (matchingBehavior == MatchOnlyUserAgentRules)
        collector.matchUARules();
    else
        collector.matchAllRules(m_matchAuthorAndUserStyles, matchingBehavior != MatchAllRulesExcludingSMIL);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *state.parentStyle(), &element);

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    state.clear(); // Clear out for the next resolve.

    // Now return the style.
    return state.takeStyle();
}

Ref<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    RELEASE_ASSERT(!m_inLoadPendingImages);

    MatchResult result;
    result.addMatchedProperties(keyframe->properties());

    ASSERT(!m_state.style());

    State& state = m_state;

    // Create the style
    state.setStyle(RenderStyle::clone(elementStyle));
    state.setParentStyle(RenderStyle::clone(elementStyle));

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state.style(), result, direction, writingMode);

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    CascadedProperties cascade(direction, writingMode);
    cascade.addMatches(result, false, 0, result.matchedProperties().size() - 1);
    
    // Resolve custom properties first.
    applyCascadedProperties(cascade, CSSPropertyCustom, CSSPropertyCustom, &result);

    applyCascadedProperties(cascade, firstCSSProperty, lastHighPriorityProperty, &result);

    // If our font got dirtied, update it now.
    updateFont();

    // Now do rest of the properties.
    applyCascadedProperties(cascade, firstLowPriorityProperty, lastCSSProperty, &result);

    // If our font got dirtied by one of the non-essential font props, update it a second time.
    updateFont();

    cascade.applyDeferredProperties(*this, &result);

    adjustRenderStyle(*state.style(), *state.parentStyle(), nullptr);

    // Start loading resources referenced by this style.
    loadPendingResources();
    
    // Add all the animating properties to the keyframe.
    unsigned propertyCount = keyframe->properties().propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSPropertyID property = keyframe->properties().propertyAt(i).id();
        // Timing-function within keyframes is special, because it is not animated; it just
        // describes the timing function between this keyframe and the next.
        if (property != CSSPropertyWebkitAnimationTimingFunction && property != CSSPropertyAnimationTimingFunction)
            keyframeValue.addProperty(property);
    }

    return state.takeStyle();
}

void StyleResolver::keyframeStylesForAnimation(Element& element, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name
    if (list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const StyleRuleKeyframes* keyframesRule = it->value.get();

    // Construct and populate the style for each keyframe
    const Vector<RefPtr<StyleKeyframe>>& keyframes = keyframesRule->keyframes();
    for (unsigned i = 0; i < keyframes.size(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        m_state = State(element, nullptr);

        const StyleKeyframe* keyframe = keyframes[i].get();

        KeyframeValue keyframeValue(0, nullptr);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, keyframe, keyframeValue));

        // Add this keyframe style to all the indicated key times
        for (auto& key: keyframe->keys()) {
            keyframeValue.setKey(key);
            list.insert(keyframeValue);
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe)
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key()) {
        static StyleKeyframe* zeroPercentKeyframe;
        if (!zeroPercentKeyframe) {
            zeroPercentKeyframe = &StyleKeyframe::create(MutableStyleProperties::create()).leakRef();
            zeroPercentKeyframe->setKeyText("0%");
        }
        KeyframeValue keyframeValue(0, nullptr);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, zeroPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe)
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        static StyleKeyframe* hundredPercentKeyframe;
        if (!hundredPercentKeyframe) {
            hundredPercentKeyframe = &StyleKeyframe::create(MutableStyleProperties::create()).leakRef();
            hundredPercentKeyframe->setKeyText("100%");
        }
        KeyframeValue keyframeValue(1, nullptr);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element& element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle& parentStyle)
{
    m_state = State(element, &parentStyle);

    State& state = m_state;

    if (m_state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(m_state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.

    // Check UA, user and author rules.
    ElementRuleCollector collector(element, m_state.style(), m_ruleSets, m_state.selectorFilter());
    collector.setPseudoStyleRequest(pseudoStyleRequest);
    collector.setMedium(m_medium.get());
    collector.matchUARules();

    if (m_matchAuthorAndUserStyles) {
        collector.matchUserRules(false);
        collector.matchAuthorRules(false);
    }

    if (collector.matchedResult().matchedProperties().isEmpty())
        return nullptr;

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *m_state.parentStyle(), nullptr);

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    // Start loading resources referenced by this style.
    loadPendingResources();

    // Now return the style.
    return state.takeStyle();
}

Ref<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    RELEASE_ASSERT(!m_inLoadPendingImages);

    auto* documentElement = m_document.documentElement();
    if (!documentElement)
        return RenderStyle::create();

    m_state = State(*documentElement, m_document.renderStyle());

    m_state.setStyle(RenderStyle::create());
    m_state.style()->inheritFrom(m_state.rootElementStyle());

    PageRuleCollector collector(m_state, m_ruleSets);
    collector.matchAllPageRules(pageIndex);

    MatchResult& result = collector.matchedResult();

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*m_state.style(), result, direction, writingMode);

    CascadedProperties cascade(direction, writingMode);
    cascade.addMatches(result, false, 0, result.matchedProperties().size() - 1);

    // Resolve custom properties first.
    applyCascadedProperties(cascade, CSSPropertyCustom, CSSPropertyCustom, &result);

    applyCascadedProperties(cascade, firstCSSProperty, lastHighPriorityProperty, &result);

    // If our font got dirtied, update it now.
    updateFont();

    applyCascadedProperties(cascade, firstLowPriorityProperty, lastCSSProperty, &result);

    cascade.applyDeferredProperties(*this, &result);

    // Start loading resources referenced by this style.
    loadPendingResources();

    // Now return the style.
    return m_state.takeStyle();
}

Ref<RenderStyle> StyleResolver::defaultStyleForElement()
{
    m_state.setStyle(RenderStyle::create());
    // Make sure our fonts are initialized if we don't inherit them from our parent style.
    initializeFontStyle(documentSettings());
    if (documentSettings())
        m_state.style()->fontCascade().update(&document().fontSelector());
    else
        m_state.style()->fontCascade().update(nullptr);

    return m_state.takeStyle();
}

static void addIntrinsicMargins(RenderStyle& style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2 * style.effectiveZoom();

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "hasQuirk" to decide the margin wasn't set is kind of lame.
    if (style.width().isIntrinsicOrAuto()) {
        if (style.marginLeft().hasQuirk())
            style.setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style.marginRight().hasQuirk())
            style.setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style.height().isAuto()) {
        if (style.marginTop().hasQuirk())
            style.setMarginTop(Length(intrinsicMargin, Fixed));
        if (style.marginBottom().hasQuirk())
            style.setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

static EDisplay equivalentBlockDisplay(EDisplay display, bool isFloating, bool strictParsing)
{
    switch (display) {
    case BLOCK:
    case TABLE:
    case BOX:
    case FLEX:
    case WEBKIT_FLEX:
#if ENABLE(CSS_GRID_LAYOUT)
    case GRID:
#endif
        return display;

    case LIST_ITEM:
        // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk, but only in quirks mode.
        if (!strictParsing && isFloating)
            return BLOCK;
        return display;
    case INLINE_TABLE:
        return TABLE;
    case INLINE_BOX:
        return BOX;
    case INLINE_FLEX:
    case WEBKIT_INLINE_FLEX:
        return FLEX;
#if ENABLE(CSS_GRID_LAYOUT)
    case INLINE_GRID:
        return GRID;
#endif

    case INLINE:
    case COMPACT:
    case INLINE_BLOCK:
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
    case TABLE_ROW:
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
    case TABLE_CELL:
    case TABLE_CAPTION:
        return BLOCK;
    case NONE:
        ASSERT_NOT_REACHED();
        return NONE;
    }
    ASSERT_NOT_REACHED();
    return BLOCK;
}

// CSS requires text-decoration to be reset at each DOM element for tables, 
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements.
static bool doesNotInheritTextDecoration(const RenderStyle& style, Element* e)
{
    return style.display() == TABLE || style.display() == INLINE_TABLE
        || style.display() == INLINE_BLOCK || style.display() == INLINE_BOX || isAtShadowBoundary(e)
        || style.isFloating() || style.hasOutOfFlowPosition();
}

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
static bool isScrollableOverflow(EOverflow overflow)
{
    return overflow == OSCROLL || overflow == OAUTO || overflow == OOVERLAY;
}
#endif

void StyleResolver::adjustStyleForInterCharacterRuby()
{
    RenderStyle* style = m_state.style();
    if (style->rubyPosition() != RubyPositionInterCharacter || !m_state.element() || !m_state.element()->hasTagName(rtTag))
        return;
    style->setTextAlign(CENTER);
    if (style->isHorizontalWritingMode())
        style->setWritingMode(LeftToRightWritingMode);
}

void StyleResolver::adjustRenderStyle(RenderStyle& style, const RenderStyle& parentStyle, Element *e)
{
    // Cache our original display.
    style.setOriginalDisplay(style.display());

    if (style.display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
        // these tags to retain their display types.
        if (document().inQuirksMode() && e) {
            if (e->hasTagName(tdTag)) {
                style.setDisplay(TABLE_CELL);
                style.setFloating(NoFloat);
            } else if (is<HTMLTableElement>(*e))
                style.setDisplay(style.isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        if (e && (e->hasTagName(tdTag) || e->hasTagName(thTag))) {
            if (style.whiteSpace() == KHTML_NOWRAP) {
                // Figure out if we are really nowrapping or if we should just
                // use normal instead. If the width of the cell is fixed, then
                // we don't actually use NOWRAP.
                if (style.width().isFixed())
                    style.setWhiteSpace(NORMAL);
                else
                    style.setWhiteSpace(NOWRAP);
            }
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (is<HTMLTableElement>(e) && (style.textAlign() == WEBKIT_LEFT || style.textAlign() == WEBKIT_CENTER || style.textAlign() == WEBKIT_RIGHT))
            style.setTextAlign(TASTART);

        // Frames and framesets never honor position:relative or position:absolute. This is necessary to
        // fix a crash where a site tries to position these objects. They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style.setPosition(StaticPosition);
            style.setDisplay(BLOCK);
        }

        // Ruby text does not support float or position. This might change with evolution of the specification.
        if (e && e->hasTagName(rtTag)) {
            style.setPosition(StaticPosition);
            style.setFloating(NoFloat);
        }

        // FIXME: We shouldn't be overriding start/-webkit-auto like this. Do it in html.css instead.
        // Table headers with a text-align of -webkit-auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style.textAlign() == TASTART)
            style.setTextAlign(CENTER);

        if (e && e->hasTagName(legendTag))
            style.setDisplay(BLOCK);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (e && e->document().documentElement() == e))
            style.setDisplay(equivalentBlockDisplay(style.display(), style.isFloating(), !document().inQuirksMode()));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == INLINE && style.styleType() == NOPSEUDO && style.writingMode() != parentStyle.writingMode())
            style.setDisplay(INLINE_BLOCK);

        // After performing the display mutation, check table rows. We do not honor position:relative or position:sticky on
        // table rows or cells. This has been established for position:relative in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW_GROUP
            || style.display() == TABLE_FOOTER_GROUP || style.display() == TABLE_ROW)
            && style.position() == RelativePosition)
            style.setPosition(StaticPosition);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style.display() == TABLE_COLUMN || style.display() == TABLE_COLUMN_GROUP || style.display() == TABLE_FOOTER_GROUP
            || style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW || style.display() == TABLE_ROW_GROUP
            || style.display() == TABLE_CELL)
            style.setWritingMode(parentStyle.writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style.writingMode() != TopToBottomWritingMode && (style.display() == BOX || style.display() == INLINE_BOX))
            style.setWritingMode(TopToBottomWritingMode);

        if (parentStyle.isDisplayFlexibleOrGridBox()) {
            style.setFloating(NoFloat);
            style.setDisplay(equivalentBlockDisplay(style.display(), style.isFloating(), !document().inQuirksMode()));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style.position() == StaticPosition && !parentStyle.isDisplayFlexibleOrGridBox())
        style.setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoZIndex() && ((e && e->document().documentElement() == e)
        || style.opacity() < 1.0f
        || style.hasTransformRelatedProperty()
        || style.hasMask()
        || style.clipPath()
        || style.boxReflect()
        || style.hasFilter()
#if ENABLE(FILTERS_LEVEL_2)
        || style.hasBackdropFilter()
#endif
        || style.hasBlendMode()
        || style.hasIsolation()
        || style.position() == StickyPosition
        || (style.position() == FixedPosition && documentSettings() && documentSettings()->fixedPositionCreatesStackingContext())
        || style.hasFlowFrom()
        || style.willChangeCreatesStackingContext()
        ))
        style.setZIndex(0);

    // Textarea considers overflow visible as auto.
    if (is<HTMLTextAreaElement>(e)) {
        style.setOverflowX(style.overflowX() == OVISIBLE ? OAUTO : style.overflowX());
        style.setOverflowY(style.overflowY() == OVISIBLE ? OAUTO : style.overflowY());
    }

    // Disallow -webkit-user-modify on :pseudo and ::pseudo elements.
    if (e && !e->shadowPseudoId().isNull())
        style.setUserModify(READ_ONLY);

    if (doesNotInheritTextDecoration(style, e))
        style.setTextDecorationsInEffect(style.textDecoration());
    else
        style.addToTextDecorationsInEffect(style.textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style.overflowX() == OMARQUEE && style.overflowY() != OMARQUEE)
        style.setOverflowY(OMARQUEE);
    else if (style.overflowY() == OMARQUEE && style.overflowX() != OMARQUEE)
        style.setOverflowX(OMARQUEE);
    else if (style.overflowX() == OVISIBLE && style.overflowY() != OVISIBLE) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style.setOverflowX(OAUTO);
    } else if (style.overflowY() == OVISIBLE && style.overflowX() != OVISIBLE)
        style.setOverflowY(OAUTO);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // Style::createForDocument().
    if ((style.overflowY() == OPAGEDX || style.overflowY() == OPAGEDY) && !(e && (e->hasTagName(htmlTag) || e->hasTagName(bodyTag))))
        style.setColumnStylesFromPaginationMode(WebCore::paginationModeForRenderStyle(style));

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style.display() == TABLE || style.display() == INLINE_TABLE
        || style.display() == TABLE_ROW_GROUP || style.display() == TABLE_ROW) {
        if (style.overflowX() != OVISIBLE && style.overflowX() != OHIDDEN)
            style.setOverflowX(OVISIBLE);
        if (style.overflowY() != OVISIBLE && style.overflowY() != OHIDDEN)
            style.setOverflowY(OVISIBLE);
    }

    // Menulists should have visible overflow
    if (style.appearance() == MenulistPart) {
        style.setOverflowX(OVISIBLE);
        style.setOverflowY(OVISIBLE);
    }

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    // Touch overflow scrolling creates a stacking context.
    if (style.hasAutoZIndex() && style.useTouchOverflowScrolling() && (isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY())))
        style.setZIndex(0);
#endif

    // Cull out any useless layers and also repeat patterns into additional layers.
    style.adjustBackgroundLayers();
    style.adjustMaskLayers();

    // Do the same for animations and transitions.
    style.adjustAnimations();
    style.adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (is<HTMLFormControlElement>(e) && style.fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!is<HTMLInputElement>(*e) || !downcast<HTMLInputElement>(*e).isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(*this, style, e, m_state.hasUAAppearance(), m_state.borderData(), m_state.backgroundData(), m_state.backgroundColor());

    // If we have first-letter pseudo style, do not share this style.
    if (style.hasPseudoStyle(FIRST_LETTER))
        style.setUnique();

    // FIXME: when dropping the -webkit prefix on transform-style, we should also have opacity < 1 cause flattening.
    if (style.preserves3D() && (style.overflowX() != OVISIBLE
        || style.overflowY() != OVISIBLE
        || style.hasFilter()
#if ENABLE(FILTERS_LEVEL_2)
        || style.hasBackdropFilter()
#endif
        || style.hasBlendMode()))
        style.setTransformStyle3D(TransformStyle3DFlat);

    if (e && e->isSVGElement()) {
        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style.setPosition(RenderStyle::initialPosition());

        // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
        // not be scaled again.
        if (e->hasTagName(SVGNames::foreignObjectTag))
            style.setEffectiveZoom(RenderStyle::initialZoom());

        // SVG text layout code expects us to be a block-level style element.
        if ((e->hasTagName(SVGNames::foreignObjectTag) || e->hasTagName(SVGNames::textTag)) && style.isDisplayInlineType())
            style.setDisplay(BLOCK);
    }

    // If the inherited value of justify-items includes the legacy keyword, 'auto'
    // computes to the the inherited value.
    if (parentStyle.justifyItemsPositionType() == LegacyPosition && style.justifyItemsPosition() == ItemPositionAuto)
        style.setJustifyItems(parentStyle.justifyItems());
}

bool StyleResolver::checkRegionStyle(Element* regionElement)
{
    unsigned rulesSize = m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().size();
    for (unsigned i = 0; i < rulesSize; ++i) {
        ASSERT(m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().at(i).ruleSet.get());
        if (checkRegionSelector(m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().at(i).selector, regionElement))
            return true;
    }

    if (m_ruleSets.userStyle()) {
        rulesSize = m_ruleSets.userStyle()->regionSelectorsAndRuleSets().size();
        for (unsigned i = 0; i < rulesSize; ++i) {
            ASSERT(m_ruleSets.userStyle()->regionSelectorsAndRuleSets().at(i).ruleSet.get());
            if (checkRegionSelector(m_ruleSets.userStyle()->regionSelectorsAndRuleSets().at(i).selector, regionElement))
                return true;
        }
    }

    return false;
}

static void checkForOrientationChange(RenderStyle* style)
{
    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    std::tie(fontOrientation, glyphOrientation) = style->fontAndGlyphOrientation();

    const auto& fontDescription = style->fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    auto newFontDescription = fontDescription;
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::updateFont()
{
    if (!m_state.fontDirty())
        return;

    RenderStyle* style = m_state.style();
#if ENABLE(IOS_TEXT_AUTOSIZING)
    checkForTextSizeAdjust(style);
#endif
    checkForGenericFamilyChange(style, m_state.parentStyle());
    checkForZoomChange(style, m_state.parentStyle());
    checkForOrientationChange(style);
    style->fontCascade().update(&document().fontSelector());
    if (m_state.fontSizeHasViewportUnits())
        style->setHasViewportUnits(true);
    m_state.setFontDirty(false);
}

Vector<RefPtr<StyleRule>> StyleResolver::styleRulesForElement(Element* e, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(e, NOPSEUDO, rulesToInclude);
}

Vector<RefPtr<StyleRule>> StyleResolver::pseudoStyleRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!element || !element->document().haveStylesheetsLoaded())
        return Vector<RefPtr<StyleRule>>();

    m_state = State(*element, nullptr);

    ElementRuleCollector collector(*element, nullptr, m_ruleSets, m_state.selectorFilter());
    collector.setMode(SelectorChecker::Mode::CollectingRules);
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));
    collector.setMedium(m_medium.get());

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        collector.matchUARules();
        
        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            collector.matchUserRules(rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules)) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));

        // Check the rules in author sheets.
        collector.matchAuthorRules(rulesToInclude & EmptyCSSRules);
    }

    return collector.matchedRuleList();
}

static bool shouldApplyPropertyInParseOrder(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyWebkitTextDecorationLine:
    case CSSPropertyWebkitTextDecorationStyle:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextDecorationSkip:
    case CSSPropertyWebkitTextUnderlinePosition:
    case CSSPropertyTextDecoration:
        return true;
    default:
        return false;
    }
}

static bool elementTypeHasAppearanceFromUAStyle(const Element& element)
{
    // NOTE: This is just a hard-coded list of elements that have some -webkit-appearance value in html.css
    const auto& localName = element.localName();
    return localName == HTMLNames::inputTag
        || localName == HTMLNames::textareaTag
        || localName == HTMLNames::buttonTag
        || localName == HTMLNames::progressTag
        || localName == HTMLNames::selectTag
        || localName == HTMLNames::meterTag
        || localName == HTMLNames::isindexTag;
}

unsigned StyleResolver::computeMatchedPropertiesHash(const MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

bool operator==(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return a.firstUARule == b.firstUARule
        && a.lastUARule == b.lastUARule
        && a.firstAuthorRule == b.firstAuthorRule
        && a.lastAuthorRule == b.lastAuthorRule
        && a.firstUserRule == b.firstUserRule
        && a.lastUserRule == b.lastUserRule;
}

bool operator!=(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return !(a == b);
}

bool operator==(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return a.properties == b.properties && a.linkMatchType == b.linkMatchType;
}

bool operator!=(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return !(a == b);
}

const StyleResolver::MatchedPropertiesCacheItem* StyleResolver::findFromMatchedPropertiesCache(unsigned hash, const MatchResult& matchResult)
{
    ASSERT(hash);

    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.find(hash);
    if (it == m_matchedPropertiesCache.end())
        return nullptr;
    MatchedPropertiesCacheItem& cacheItem = it->value;

    size_t size = matchResult.matchedProperties().size();
    if (size != cacheItem.matchedProperties.size())
        return nullptr;
    for (size_t i = 0; i < size; ++i) {
        if (matchResult.matchedProperties()[i] != cacheItem.matchedProperties[i])
            return nullptr;
    }
    if (cacheItem.ranges != matchResult.ranges)
        return nullptr;
    return &cacheItem;
}

void StyleResolver::addToMatchedPropertiesCache(const RenderStyle* style, const RenderStyle* parentStyle, unsigned hash, const MatchResult& matchResult)
{
    static const unsigned matchedDeclarationCacheAdditionsBetweenSweeps = 100;
    if (++m_matchedPropertiesCacheAdditionsSinceLastSweep >= matchedDeclarationCacheAdditionsBetweenSweeps
        && !m_matchedPropertiesCacheSweepTimer.isActive()) {
        static const unsigned matchedDeclarationCacheSweepTimeInSeconds = 60;
        m_matchedPropertiesCacheSweepTimer.startOneShot(matchedDeclarationCacheSweepTimeInSeconds);
    }

    ASSERT(hash);
    MatchedPropertiesCacheItem cacheItem;
    cacheItem.matchedProperties.appendVector(matchResult.matchedProperties());
    cacheItem.ranges = matchResult.ranges;
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    cacheItem.renderStyle = RenderStyle::clone(style);
    cacheItem.parentRenderStyle = RenderStyle::clone(parentStyle);
    m_matchedPropertiesCache.add(hash, WTFMove(cacheItem));
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

void StyleResolver::clearCachedPropertiesAffectedByViewportUnits()
{
    Vector<unsigned, 16> toRemove;
    for (auto& cacheKeyValue : m_matchedPropertiesCache) {
        if (cacheKeyValue.value.renderStyle->hasViewportUnits())
            toRemove.append(cacheKeyValue.key);
    }
    for (auto key : toRemove)
        m_matchedPropertiesCache.remove(key);
}

static bool isCacheableInMatchedPropertiesCache(const Element& element, const RenderStyle* style, const RenderStyle* parentStyle)
{
    // FIXME: Writing mode and direction properties modify state when applying to document element by calling
    // Document::setWritingMode/DirectionSetOnDocumentElement. We can't skip the applying by caching.
    if (&element == element.document().documentElement())
        return false;
    if (style->unique() || (style->styleType() != NOPSEUDO && parentStyle->unique()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    if (style->writingMode() != RenderStyle::initialWritingMode() || style->direction() != RenderStyle::initialDirection())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void extractDirectionAndWritingMode(const RenderStyle& style, const StyleResolver::MatchResult& matchResult, TextDirection& direction, WritingMode& writingMode)
{
    direction = style.direction();
    writingMode = style.writingMode();

    bool hadImportantWebkitWritingMode = false;
    bool hadImportantDirection = false;

    for (const auto& matchedProperties : matchResult.matchedProperties()) {
        for (unsigned i = 0, count = matchedProperties.properties->propertyCount(); i < count; ++i) {
            auto property = matchedProperties.properties->propertyAt(i);
            if (!property.value()->isPrimitiveValue())
                continue;
            switch (property.id()) {
            case CSSPropertyWebkitWritingMode:
                if (!hadImportantWebkitWritingMode || property.isImportant()) {
                    writingMode = downcast<CSSPrimitiveValue>(*property.value());
                    hadImportantWebkitWritingMode = property.isImportant();
                }
                break;
            case CSSPropertyDirection:
                if (!hadImportantDirection || property.isImportant()) {
                    direction = downcast<CSSPrimitiveValue>(*property.value());
                    hadImportantDirection = property.isImportant();
                }
                break;
            default:
                break;
            }
        }
    }
}

void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, const Element& element, ShouldUseMatchedPropertiesCache shouldUseMatchedPropertiesCache)
{
    State& state = m_state;
    unsigned cacheHash = shouldUseMatchedPropertiesCache && matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties().data(), matchResult.matchedProperties().size()) : 0;
    bool applyInheritedOnly = false;
    const MatchedPropertiesCacheItem* cacheItem = nullptr;
    if (cacheHash && (cacheItem = findFromMatchedPropertiesCache(cacheHash, matchResult))
        && isCacheableInMatchedPropertiesCache(element, state.style(), state.parentStyle())) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the 
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cacheItem->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cacheItem->parentRenderStyle.get()) && !isAtShadowBoundary(&element)) {
            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cacheItem->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true; 
    }

    // Directional properties (*-before/after) are aliases that depend on the TextDirection and WritingMode.
    // These must be resolved before we can begin the property cascade.
    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state.style(), matchResult, direction, writingMode);

    if (elementTypeHasAppearanceFromUAStyle(*state.element())) {
        // FIXME: This is such a hack.
        // Find out if there's a -webkit-appearance property in effect from the UA sheet.
        // If so, we cache the border and background styles so that RenderTheme::adjustStyle()
        // can look at them later to figure out if this is a styled form control or not.
        CascadedProperties cascade(direction, writingMode);
        cascade.addMatches(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
        cascade.addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

        applyCascadedProperties(cascade, CSSPropertyWebkitRubyPosition, CSSPropertyWebkitRubyPosition, &matchResult);
        adjustStyleForInterCharacterRuby();
    
        // Resolve custom variables first.
        applyCascadedProperties(cascade, CSSPropertyCustom, CSSPropertyCustom, &matchResult);

        // Start by applying properties that other properties may depend on.
        applyCascadedProperties(cascade, firstCSSProperty, lastHighPriorityProperty, &matchResult);
    
        updateFont();
        applyCascadedProperties(cascade, firstLowPriorityProperty, lastCSSProperty, &matchResult);

        state.cacheBorderAndBackground();
    }

    CascadedProperties cascade(direction, writingMode);
    cascade.addMatches(matchResult, false, 0, matchResult.matchedProperties().size() - 1, applyInheritedOnly);
    cascade.addMatches(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    cascade.addMatches(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    cascade.addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
    
    // Resolve custom properties first.
    applyCascadedProperties(cascade, CSSPropertyCustom, CSSPropertyCustom, &matchResult);

    applyCascadedProperties(cascade, CSSPropertyWebkitRubyPosition, CSSPropertyWebkitRubyPosition, &matchResult);
    
    // Adjust the font size to be smaller if ruby-position is inter-character.
    adjustStyleForInterCharacterRuby();

    // Start by applying properties that other properties may depend on.
    applyCascadedProperties(cascade, firstCSSProperty, lastHighPriorityProperty, &matchResult);

    // If the effective zoom value changes, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->effectiveZoom() != state.style()->effectiveZoom())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // If our font got dirtied, update it now.
    updateFont();

    // If the font changed, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->fontDescription() != state.style()->fontDescription())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // Apply properties that no other properties depend on.
    applyCascadedProperties(cascade, firstLowPriorityProperty, lastCSSProperty, &matchResult);

    // Finally, some properties must be applied in the order they were parsed.
    // There are some CSS properties that affect the same RenderStyle values,
    // so to preserve behavior, we queue them up during cascade and flush here.
    cascade.applyDeferredProperties(*this, &matchResult);

    // Start loading resources referenced by this style.
    loadPendingResources();
    
    ASSERT(!state.fontDirty());
    
    if (cacheItem || !cacheHash)
        return;
    if (!isCacheableInMatchedPropertiesCache(*state.element(), state.style(), state.parentStyle()))
        return;
    addToMatchedPropertiesCache(state.style(), state.parentStyle(), cacheHash, matchResult);
}

void StyleResolver::applyPropertyToStyle(CSSPropertyID id, CSSValue* value, RenderStyle* style)
{
    m_state = State();
    m_state.setParentStyle(*style);
    m_state.setStyle(*style);
    applyPropertyToCurrentStyle(id, value);
}

void StyleResolver::applyPropertyToCurrentStyle(CSSPropertyID id, CSSValue* value)
{
    if (value)
        applyProperty(id, value);
}

inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyFill:
    case CSSPropertyStroke:
        return true;
    default:
        break;
    }

    return false;
}

// http://dev.w3.org/csswg/css3-regions/#the-at-region-style-rule
// FIXME: add incremental support for other region styling properties.
inline bool StyleResolver::isValidRegionStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyColor:
        return true;
    default:
        break;
    }

    return false;
}

#if ENABLE(VIDEO_TRACK)
inline bool StyleResolver::isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    default:
        break;
    }
    return false;
}
#endif
// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool StyleResolver::useSVGZoomRules()
{
    return m_state.element() && m_state.element()->isSVGElement();
}

// Scale with/height properties on inline SVG root.
bool StyleResolver::useSVGZoomRulesForLength()
{
    return is<SVGElement>(m_state.element()) && !(is<SVGSVGElement>(*m_state.element()) && m_state.element()->parentNode());
}

StyleResolver::CascadedProperties* StyleResolver::cascadedPropertiesForRollback(const MatchResult& matchResult)
{
    ASSERT(cascadeLevel() != UserAgentLevel);
    
    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state().style(), matchResult, direction, writingMode);

    if (cascadeLevel() == AuthorLevel) {
        CascadedProperties* authorRollback = state().authorRollback();
        if (authorRollback)
            return authorRollback;
        
        auto newAuthorRollback(std::make_unique<CascadedProperties>(direction, writingMode));
        
        // This special rollback cascade contains UA rules and user rules but no author rules.
        newAuthorRollback->addMatches(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
        newAuthorRollback->addMatches(matchResult, false, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, false);
        newAuthorRollback->addMatches(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, false);
        newAuthorRollback->addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
    
        state().setAuthorRollback(newAuthorRollback);
        return state().authorRollback();
    }
    
    if (cascadeLevel() == UserLevel) {
        CascadedProperties* userRollback = state().userRollback();
        if (userRollback)
            return userRollback;
        
        auto newUserRollback(std::make_unique<CascadedProperties>(direction, writingMode));
        
        // This special rollback cascade contains only UA rules.
        newUserRollback->addMatches(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
        newUserRollback->addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
    
        state().setUserRollback(newUserRollback);
        return state().userRollback();
    }
    
    return nullptr;
}

void StyleResolver::applyProperty(CSSPropertyID id, CSSValue* value, SelectorChecker::LinkMatchMask linkMatchMask, const MatchResult* matchResult)
{
    ASSERT_WITH_MESSAGE(!isShorthandCSSProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    State& state = m_state;
    
    RefPtr<CSSValue> valueToApply = value;
    if (value->isVariableDependentValue()) {
        valueToApply = resolvedVariableValue(id, *downcast<CSSVariableDependentValue>(value));
        if (!valueToApply) {
            if (CSSProperty::isInheritedProperty(id))
                valueToApply = CSSValuePool::singleton().createInheritedValue();
            else
                valueToApply = CSSValuePool::singleton().createExplicitInitialValue();
        }
    }

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, state.style()->direction(), state.style()->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask, matchResult);
    }
    
    CSSValue* valueToCheckForInheritInitial = valueToApply.get();
    CSSCustomPropertyValue* customPropertyValue = nullptr;
    
    if (id == CSSPropertyCustom) {
        customPropertyValue = &downcast<CSSCustomPropertyValue>(*valueToApply);
        valueToCheckForInheritInitial = customPropertyValue->value().get();
    }

    bool isInherit = state.parentStyle() && valueToCheckForInheritInitial->isInheritedValue();
    bool isInitial = valueToCheckForInheritInitial->isInitialValue() || (!state.parentStyle() && valueToCheckForInheritInitial->isInheritedValue());
    
    bool isUnset = valueToCheckForInheritInitial->isUnsetValue();
    bool isRevert = valueToCheckForInheritInitial->isRevertValue();

    if (isRevert) {
        if (cascadeLevel() == UserAgentLevel || !matchResult)
            isUnset = true;
        else {
            // Fetch the correct rollback object from the state, building it if necessary.
            // This requires having the original MatchResult available.
            auto* rollback = cascadedPropertiesForRollback(*matchResult);
            ASSERT(rollback);

            // With the cascade built, we need to obtain the property and apply it. If the property is
            // not present, then we behave like "unset." Otherwise we apply the property instead of
            // our own.
            if (customPropertyValue) {
                if (rollback->hasCustomProperty(customPropertyValue->name())) {
                    auto property = rollback->customProperty(customPropertyValue->name());
                    if (property.cssValue[linkMatchMask])
                        applyProperty(property.id, property.cssValue[linkMatchMask], linkMatchMask, matchResult);
                    return;
                }
            } else if (rollback->hasProperty(id)) {
                auto& property = rollback->property(id);
                if (property.cssValue[linkMatchMask])
                    applyProperty(property.id, property.cssValue[linkMatchMask], linkMatchMask, matchResult);
                return;
            }
        
            isUnset = true;
        }
    }
    
    if (isUnset) {
        if (CSSProperty::isInheritedProperty(id))
            isInherit = true;
        else
            isInitial = true;
    }

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();
    
    if (id == CSSPropertyCustom) {
        CSSCustomPropertyValue* customProperty = &downcast<CSSCustomPropertyValue>(*valueToApply);
        if (isInherit) {
            RefPtr<CSSValue> customVal = state.parentStyle()->getCustomPropertyValue(customProperty->name());
            if (!customVal)
                customVal = CSSCustomPropertyValue::createInvalid();
            state.style()->setCustomPropertyValue(customProperty->name(), customVal);
        } else if (isInitial)
            state.style()->setCustomPropertyValue(customProperty->name(), CSSCustomPropertyValue::createInvalid());
        else
            state.style()->setCustomPropertyValue(customProperty->name(), customProperty->value());
        return;
    }

    // Use the generated StyleBuilder.
    StyleBuilder::applyProperty(id, *this, *valueToApply, isInitial, isInherit);
}

RefPtr<CSSValue> StyleResolver::resolvedVariableValue(CSSPropertyID propID, const CSSVariableDependentValue& value)
{
    CSSParser parser(m_state.document());
    return parser.parseVariableDependentValue(propID, value, m_state.style()->customProperties());
}

PassRefPtr<StyleImage> StyleResolver::styleImage(CSSPropertyID property, CSSValue& value)
{
    if (is<CSSImageValue>(value))
        return cachedOrPendingFromValue(property, downcast<CSSImageValue>(value));

    if (is<CSSImageGeneratorValue>(value)) {
        if (is<CSSGradientValue>(value))
            return generatedOrPendingFromValue(property, *downcast<CSSGradientValue>(value).gradientWithStylesResolved(this));
        return generatedOrPendingFromValue(property, downcast<CSSImageGeneratorValue>(value));
    }

#if ENABLE(CSS_IMAGE_SET)
    if (is<CSSImageSetValue>(value))
        return setOrPendingFromValue(property, downcast<CSSImageSetValue>(value));
#endif

    if (is<CSSCursorImageValue>(value))
        return cursorOrPendingFromValue(property, downcast<CSSCursorImageValue>(value));

    return nullptr;
}

PassRefPtr<StyleImage> StyleResolver::cachedOrPendingFromValue(CSSPropertyID property, CSSImageValue& value)
{
    RefPtr<StyleImage> image = value.cachedOrPendingImage();
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, &value);
    return image.release();
}

PassRefPtr<StyleImage> StyleResolver::generatedOrPendingFromValue(CSSPropertyID property, CSSImageGeneratorValue& value)
{
    if (is<CSSFilterImageValue>(value)) {
        // FilterImage needs to calculate FilterOperations.
        downcast<CSSFilterImageValue>(value).createFilterOperations(this);
    }

    if (value.isPending()) {
        m_state.pendingImageProperties().set(property, &value);
        return StylePendingImage::create(&value);
    }
    return StyleGeneratedImage::create(value);
}

#if ENABLE(CSS_IMAGE_SET)
PassRefPtr<StyleImage> StyleResolver::setOrPendingFromValue(CSSPropertyID property, CSSImageSetValue& value)
{
    RefPtr<StyleImage> image = value.cachedOrPendingImageSet(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, &value);
    return image.release();
}
#endif

PassRefPtr<StyleImage> StyleResolver::cursorOrPendingFromValue(CSSPropertyID property, CSSCursorImageValue& value)
{
    RefPtr<StyleImage> image = value.cachedOrPendingImage(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, &value);
    return image.release();
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
void StyleResolver::checkForTextSizeAdjust(RenderStyle* style)
{
    if (style->textSizeAdjust().isAuto())
        return;

    auto newFontDescription = style->fontDescription();
    if (!style->textSizeAdjust().isNone())
        newFontDescription.setComputedSize(newFontDescription.specifiedSize() * style->textSizeAdjust().multiplier());
    else
        newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    style->setFontDescription(newFontDescription);
}
#endif

void StyleResolver::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (!parentStyle)
        return;
    
    if (style->effectiveZoom() == parentStyle->effectiveZoom() && style->textZoom() == parentStyle->textZoom())
        return;

    const auto& childFont = style->fontDescription();
    auto newFontDescription = childFont;
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void StyleResolver::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const auto& childFont = style->fontDescription();

    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const auto& parentFont = parentStyle->fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;
    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (CSSValueID sizeIdentifier = childFont.keywordSizeAsIdentifier())
        size = Style::fontSizeForKeyword(sizeIdentifier, childFont.useFixedDefaultSize(), document());
    else {
        Settings* settings = documentSettings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
                childFont.specifiedSize() / fixedScaleFactor :
                childFont.specifiedSize() * fixedScaleFactor;
    }

    auto newFontDescription = childFont;
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::initializeFontStyle(Settings* settings)
{
    FontCascadeDescription fontDescription;
    if (settings)
        fontDescription.setRenderingMode(settings->fontRenderingMode());
    fontDescription.setOneFamily(standardFamily);
    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    setFontSize(fontDescription, Style::fontSizeForKeyword(CSSValueMedium, false, document()));
    setFontDescription(fontDescription);
}

void StyleResolver::setFontSize(FontCascadeDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), m_state.style(), document()));
}

static Color colorForCSSValue(CSSValueID cssValueId)
{
    struct ColorValue {
        CSSValueID cssValueId;
        RGBA32 color;
    };

    static const ColorValue colorValues[] = {
        { CSSValueAqua, 0xFF00FFFF },
        { CSSValueBlack, 0xFF000000 },
        { CSSValueBlue, 0xFF0000FF },
        { CSSValueFuchsia, 0xFFFF00FF },
        { CSSValueGray, 0xFF808080 },
        { CSSValueGreen, 0xFF008000  },
        { CSSValueGrey, 0xFF808080 },
        { CSSValueLime, 0xFF00FF00 },
        { CSSValueMaroon, 0xFF800000 },
        { CSSValueNavy, 0xFF000080 },
        { CSSValueOlive, 0xFF808000  },
        { CSSValueOrange, 0xFFFFA500 },
        { CSSValuePurple, 0xFF800080 },
        { CSSValueRed, 0xFFFF0000 },
        { CSSValueSilver, 0xFFC0C0C0 },
        { CSSValueTeal, 0xFF008080  },
        { CSSValueTransparent, 0x00000000 },
        { CSSValueWhite, 0xFFFFFFFF },
        { CSSValueYellow, 0xFFFFFF00 },
        { CSSValueInvalid, CSSValueInvalid }
    };

    for (const ColorValue* col = colorValues; col->cssValueId; ++col) {
        if (col->cssValueId == cssValueId)
            return col->color;
    }
    return RenderTheme::defaultTheme()->systemColor(cssValueId);
}

bool StyleResolver::colorFromPrimitiveValueIsDerivedFromElement(const CSSPrimitiveValue& value)
{
    int ident = value.getValueID();
    switch (ident) {
    case CSSValueWebkitText:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

Color StyleResolver::colorFromPrimitiveValue(const CSSPrimitiveValue& value, bool forVisitedLink) const
{
    if (value.isRGBColor())
        return Color(value.getRGBA32Value());

    const State& state = m_state;
    CSSValueID ident = value.getValueID();
    switch (ident) {
    case 0:
        return Color();
    case CSSValueWebkitText:
        return state.document().textColor();
    case CSSValueWebkitLink:
        return (state.element()->isLink() && forVisitedLink) ? state.document().visitedLinkColor() : state.document().linkColor();
    case CSSValueWebkitActivelink:
        return state.document().activeLinkColor();
    case CSSValueWebkitFocusRingColor:
        return RenderTheme::focusRingColor();
    case CSSValueCurrentcolor:
        return state.style()->color();
    default:
        return colorForCSSValue(ident);
    }
}

void StyleResolver::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(std::make_unique<MediaQueryResult>(*expr, result));
}

bool StyleResolver::hasMediaQueriesAffectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

static FilterOperation::OperationType filterOperationForType(WebKitCSSFilterValue::FilterOperationType type)
{
    switch (type) {
    case WebKitCSSFilterValue::ReferenceFilterOperation:
        return FilterOperation::REFERENCE;
    case WebKitCSSFilterValue::GrayscaleFilterOperation:
        return FilterOperation::GRAYSCALE;
    case WebKitCSSFilterValue::SepiaFilterOperation:
        return FilterOperation::SEPIA;
    case WebKitCSSFilterValue::SaturateFilterOperation:
        return FilterOperation::SATURATE;
    case WebKitCSSFilterValue::HueRotateFilterOperation:
        return FilterOperation::HUE_ROTATE;
    case WebKitCSSFilterValue::InvertFilterOperation:
        return FilterOperation::INVERT;
    case WebKitCSSFilterValue::OpacityFilterOperation:
        return FilterOperation::OPACITY;
    case WebKitCSSFilterValue::BrightnessFilterOperation:
        return FilterOperation::BRIGHTNESS;
    case WebKitCSSFilterValue::ContrastFilterOperation:
        return FilterOperation::CONTRAST;
    case WebKitCSSFilterValue::BlurFilterOperation:
        return FilterOperation::BLUR;
    case WebKitCSSFilterValue::DropShadowFilterOperation:
        return FilterOperation::DROP_SHADOW;
    case WebKitCSSFilterValue::UnknownFilterOperation:
        return FilterOperation::NONE;
    }
    return FilterOperation::NONE;
}

void StyleResolver::loadPendingSVGDocuments()
{
    State& state = m_state;

    // Crash reports indicate that we've seen calls to this function when our
    // style is NULL. We don't know exactly why this happens. Our guess is
    // reentering styleForElement().
    ASSERT(state.style());
    if (!state.style() || !state.style()->hasFilter() || state.filtersWithPendingSVGDocuments().isEmpty())
        return;

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.setContentSecurityPolicyImposition(m_state.element() && m_state.element()->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);

    CachedResourceLoader& cachedResourceLoader = state.document().cachedResourceLoader();
    
    for (auto& filterOperation : state.filtersWithPendingSVGDocuments())
        filterOperation->getOrCreateCachedSVGDocumentReference()->load(cachedResourceLoader, options);

    state.filtersWithPendingSVGDocuments().clear();
}

bool StyleResolver::createFilterOperations(const CSSValue& inValue, FilterOperations& outOperations)
{
    State& state = m_state;
    ASSERT(outOperations.isEmpty());
    
    if (is<CSSPrimitiveValue>(inValue)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(inValue);
        if (primitiveValue.getValueID() == CSSValueNone)
            return true;
    }
    
    if (!is<CSSValueList>(inValue))
        return false;

    FilterOperations operations;
    for (auto& currentValue : downcast<CSSValueList>(inValue)) {
        if (!is<WebKitCSSFilterValue>(currentValue.get()))
            continue;

        auto& filterValue = downcast<WebKitCSSFilterValue>(currentValue.get());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue.operationType());

        if (operationType == FilterOperation::REFERENCE) {
            if (filterValue.length() != 1)
                continue;
            auto& argument = *filterValue.itemWithoutBoundsCheck(0);

            if (!is<CSSPrimitiveValue>(argument))
                continue;

            auto& primitiveValue = downcast<CSSPrimitiveValue>(argument);
            String cssUrl = primitiveValue.getStringValue();
            URL url = m_state.document().completeURL(cssUrl);

            RefPtr<ReferenceFilterOperation> operation = ReferenceFilterOperation::create(cssUrl, url.fragmentIdentifier());
            if (SVGURIReference::isExternalURIReference(cssUrl, m_state.document()))
                state.filtersWithPendingSVGDocuments().append(operation);

            operations.operations().append(operation);
            continue;
        }

        // Check that all parameters are primitive values, with the
        // exception of drop shadow which has a CSSShadowValue parameter.
        const CSSPrimitiveValue* firstValue = nullptr;
        if (operationType != FilterOperation::DROP_SHADOW) {
            bool haveNonPrimitiveValue = false;
            for (unsigned j = 0; j < filterValue.length(); ++j) {
                if (!is<CSSPrimitiveValue>(*filterValue.itemWithoutBoundsCheck(j))) {
                    haveNonPrimitiveValue = true;
                    break;
                }
            }
            if (haveNonPrimitiveValue)
                continue;
            if (filterValue.length())
                firstValue = downcast<CSSPrimitiveValue>(filterValue.itemWithoutBoundsCheck(0));
        }

        switch (filterValue.operationType()) {
        case WebKitCSSFilterValue::GrayscaleFilterOperation:
        case WebKitCSSFilterValue::SepiaFilterOperation:
        case WebKitCSSFilterValue::SaturateFilterOperation: {
            double amount = 1;
            if (filterValue.length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::HueRotateFilterOperation: {
            double angle = 0;
            if (filterValue.length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case WebKitCSSFilterValue::InvertFilterOperation:
        case WebKitCSSFilterValue::BrightnessFilterOperation:
        case WebKitCSSFilterValue::ContrastFilterOperation:
        case WebKitCSSFilterValue::OpacityFilterOperation: {
            double amount = (filterValue.operationType() == WebKitCSSFilterValue::BrightnessFilterOperation) ? 0 : 1;
            if (filterValue.length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::BlurFilterOperation: {
            Length stdDeviation = Length(0, Fixed);
            if (filterValue.length() >= 1)
                stdDeviation = convertToFloatLength(firstValue, state.cssToLengthConversionData());
            if (stdDeviation.isUndefined())
                return false;

            operations.operations().append(BlurFilterOperation::create(stdDeviation));
            break;
        }
        case WebKitCSSFilterValue::DropShadowFilterOperation: {
            if (filterValue.length() != 1)
                return false;

            auto& cssValue = *filterValue.itemWithoutBoundsCheck(0);
            if (!is<CSSShadowValue>(cssValue))
                continue;

            auto& item = downcast<CSSShadowValue>(cssValue);
            int x = item.x->computeLength<int>(state.cssToLengthConversionData());
            int y = item.y->computeLength<int>(state.cssToLengthConversionData());
            IntPoint location(x, y);
            int blur = item.blur ? item.blur->computeLength<int>(state.cssToLengthConversionData()) : 0;
            Color color;
            if (item.color)
                color = colorFromPrimitiveValue(*item.color);

            operations.operations().append(DropShadowFilterOperation::create(location, blur, color.isValid() ? color : Color::transparent));
            break;
        }
        case WebKitCSSFilterValue::UnknownFilterOperation:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

PassRefPtr<StyleImage> StyleResolver::loadPendingImage(const StylePendingImage& pendingImage, const ResourceLoaderOptions& options)
{
    if (auto imageValue = pendingImage.cssImageValue())
        return imageValue->cachedImage(m_state.document().cachedResourceLoader(), options);

    if (auto imageGeneratorValue = pendingImage.cssImageGeneratorValue()) {
        imageGeneratorValue->loadSubimages(m_state.document().cachedResourceLoader(), options);
        return StyleGeneratedImage::create(*imageGeneratorValue);
    }

    if (auto cursorImageValue = pendingImage.cssCursorImageValue())
        return cursorImageValue->cachedImage(m_state.document().cachedResourceLoader(), options);

#if ENABLE(CSS_IMAGE_SET)
    if (auto imageSetValue = pendingImage.cssImageSetValue())
        return imageSetValue->cachedImageSet(m_state.document().cachedResourceLoader(), options);
#endif

    return nullptr;
}

PassRefPtr<StyleImage> StyleResolver::loadPendingImage(const StylePendingImage& pendingImage)
{
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.setContentSecurityPolicyImposition(m_state.element() && m_state.element()->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);
    return loadPendingImage(pendingImage, options);
}

#if ENABLE(CSS_SHAPES)
void StyleResolver::loadPendingShapeImage(ShapeValue* shapeValue)
{
    if (!shapeValue)
        return;

    StyleImage* image = shapeValue->image();
    if (!is<StylePendingImage>(image))
        return;

    auto& pendingImage = downcast<StylePendingImage>(*image);

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.setRequestOriginPolicy(PotentiallyCrossOriginEnabled);
    options.setAllowCredentials(DoNotAllowStoredCredentials);
    options.setContentSecurityPolicyImposition(m_state.element() && m_state.element()->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);

    shapeValue->setImage(loadPendingImage(pendingImage, options));
}
#endif

void StyleResolver::loadPendingImages()
{
    RELEASE_ASSERT(!m_inLoadPendingImages);
    TemporaryChange<bool> changeInLoadPendingImages(m_inLoadPendingImages, true);

    if (m_state.pendingImageProperties().isEmpty())
        return;

    auto end = m_state.pendingImageProperties().end().keys();
    for (auto it = m_state.pendingImageProperties().begin().keys(); it != end; ++it) {
        CSSPropertyID currentProperty = *it;

        switch (currentProperty) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = &m_state.style()->ensureBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                auto* styleImage = backgroundLayer->image();
                if (is<StylePendingImage>(styleImage))
                    backgroundLayer->setImage(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(m_state.style()->contentData()); contentData; contentData = contentData->next()) {
                if (is<ImageContentData>(*contentData)) {
                    auto& styleImage = downcast<ImageContentData>(*contentData).image();
                    if (is<StylePendingImage>(styleImage)) {
                        if (RefPtr<StyleImage> loadedImage = loadPendingImage(downcast<StylePendingImage>(styleImage)))
                            downcast<ImageContentData>(*contentData).setImage(loadedImage.release());
                    }
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = m_state.style()->cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    auto* styleImage = currentCursor.image();
                    if (is<StylePendingImage>(styleImage))
                        currentCursor.setImage(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            auto* styleImage = m_state.style()->listStyleImage();
            if (is<StylePendingImage>(styleImage))
                m_state.style()->setListStyleImage(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
            break;
        }
        case CSSPropertyBorderImageSource: {
            auto* styleImage = m_state.style()->borderImageSource();
            if (is<StylePendingImage>(styleImage))
                m_state.style()->setBorderImageSource(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = m_state.style()->boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                auto* styleImage = maskImage.image();
                if (is<StylePendingImage>(styleImage)) {
                    RefPtr<StyleImage> loadedImage = loadPendingImage(downcast<StylePendingImage>(*styleImage));
                    reflection->setMask(NinePieceImage(loadedImage.release(), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            auto* styleImage = m_state.style()->maskBoxImageSource();
            if (is<StylePendingImage>(styleImage))
                m_state.style()->setMaskBoxImageSource(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = &m_state.style()->ensureMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                auto* styleImage = maskLayer->image();
                if (is<StylePendingImage>(styleImage))
                    maskLayer->setImage(loadPendingImage(downcast<StylePendingImage>(*styleImage)));
            }
            break;
        }
#if ENABLE(CSS_SHAPES)
        case CSSPropertyWebkitShapeOutside:
            loadPendingShapeImage(m_state.style()->shapeOutside());
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
        }
    }

    m_state.pendingImageProperties().clear();
}

#ifndef NDEBUG
static bool inLoadPendingResources = false;
#endif

void StyleResolver::loadPendingResources()
{
    // We've seen crashes in all three of the functions below. Some of them
    // indicate that style() is NULL. This NULL check will cut down on total
    // crashes, while the ASSERT will help us find the cause in debug builds.
    ASSERT(style());
    if (!style())
        return;

#ifndef NDEBUG
    // Re-entering this function will probably mean trouble. Catch it in debug builds.
    ASSERT(!inLoadPendingResources);
    inLoadPendingResources = true;
#endif

    // Start loading images referenced by this style.
    loadPendingImages();

    // Start loading the SVG Documents referenced by this style.
    loadPendingSVGDocuments();

#ifndef NDEBUG
    inLoadPendingResources = false;
#endif
}

inline StyleResolver::MatchedProperties::MatchedProperties()
    : possiblyPaddedMember(nullptr)
{
}

StyleResolver::MatchedProperties::~MatchedProperties()
{
}

StyleResolver::CascadedProperties::CascadedProperties(TextDirection direction, WritingMode writingMode)
    : m_direction(direction)
    , m_writingMode(writingMode)
{
}

inline bool StyleResolver::CascadedProperties::hasProperty(CSSPropertyID id) const
{
    ASSERT(id < m_propertyIsPresent.size());
    return m_propertyIsPresent[id];
}

inline StyleResolver::CascadedProperties::Property& StyleResolver::CascadedProperties::property(CSSPropertyID id)
{
    return m_properties[id];
}

inline bool StyleResolver::CascadedProperties::hasCustomProperty(const String& name) const
{
    return m_customProperties.contains(name);
}

inline StyleResolver::CascadedProperties::Property StyleResolver::CascadedProperties::customProperty(const String& name) const
{
    return m_customProperties.get(name);
}

void StyleResolver::CascadedProperties::setPropertyInternal(Property& property, CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel)
{
    ASSERT(linkMatchType <= SelectorChecker::MatchAll);
    property.id = id;
    property.level = cascadeLevel;
    if (linkMatchType == SelectorChecker::MatchAll) {
        property.cssValue[0] = &cssValue;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else
        property.cssValue[linkMatchType] = &cssValue;
}

void StyleResolver::CascadedProperties::set(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel)
{
    if (CSSProperty::isDirectionAwareProperty(id))
        id = CSSProperty::resolveDirectionAwareProperty(id, m_direction, m_writingMode);

    ASSERT(!shouldApplyPropertyInParseOrder(id));

    auto& property = m_properties[id];
    ASSERT(id < m_propertyIsPresent.size());
    if (id == CSSPropertyCustom) {
        m_propertyIsPresent.set(id);
        const auto& customValue = downcast<CSSCustomPropertyValue>(cssValue);
        bool hasValue = customProperties().contains(customValue.name());
        if (!hasValue) {
            Property property;
            property.id = id;
            memset(property.cssValue, 0, sizeof(property.cssValue));
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel);
            customProperties().set(customValue.name(), property);
        } else {
            Property property = customProperties().get(customValue.name());
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel);
            customProperties().set(customValue.name(), property);
        }
        return;
    }
    
    if (!m_propertyIsPresent[id])
        memset(property.cssValue, 0, sizeof(property.cssValue));
    m_propertyIsPresent.set(id);
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel);
}

void StyleResolver::CascadedProperties::setDeferred(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(id));
    ASSERT(shouldApplyPropertyInParseOrder(id));

    Property property;
    memset(property.cssValue, 0, sizeof(property.cssValue));
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel);
    m_deferredProperties.append(property);
}

void StyleResolver::CascadedProperties::addStyleProperties(const StyleProperties& properties, StyleRule&, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType, unsigned linkMatchType, CascadeLevel cascadeLevel)
{
    for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
        auto current = properties.propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // We apply the inherited properties only when using the property cache.
            // A match with a value that is explicitely inherited should never have been cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        CSSPropertyID propertyID = current.id();

        if (propertyWhitelistType == PropertyWhitelistRegion && !StyleResolver::isValidRegionStyleProperty(propertyID))
            continue;
#if ENABLE(VIDEO_TRACK)
        if (propertyWhitelistType == PropertyWhitelistCue && !StyleResolver::isValidCueStyleProperty(propertyID))
            continue;
#endif

        if (shouldApplyPropertyInParseOrder(propertyID))
            setDeferred(propertyID, *current.value(), linkMatchType, cascadeLevel);
        else
            set(propertyID, *current.value(), linkMatchType, cascadeLevel);
    }
}

static CascadeLevel cascadeLevelForIndex(const StyleResolver::MatchResult& matchResult, int index)
{
    if (index >= matchResult.ranges.firstUARule && index <= matchResult.ranges.lastUARule)
        return UserAgentLevel;
    if (index >= matchResult.ranges.firstUserRule && index <= matchResult.ranges.lastUserRule)
        return UserLevel;
    return AuthorLevel;
}

void StyleResolver::CascadedProperties::addMatches(const MatchResult& matchResult, bool important, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties()[i];
        addStyleProperties(*matchedProperties.properties, *matchResult.matchedRules[i], important, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType), matchedProperties.linkMatchType,
            cascadeLevelForIndex(matchResult, i));
    }
}

void StyleResolver::CascadedProperties::applyDeferredProperties(StyleResolver& resolver, const MatchResult* matchResult)
{
    for (auto& property : m_deferredProperties)
        property.apply(resolver, matchResult);
}

void StyleResolver::CascadedProperties::Property::apply(StyleResolver& resolver, const MatchResult* matchResult)
{
    State& state = resolver.state();
    state.setCascadeLevel(level);

    if (cssValue[SelectorChecker::MatchDefault]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchDefault], SelectorChecker::MatchDefault, matchResult);
    }

    if (state.style()->insideLink() == NotInsideLink)
        return;

    if (cssValue[SelectorChecker::MatchLink]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchLink], SelectorChecker::MatchLink, matchResult);
    }

    if (cssValue[SelectorChecker::MatchVisited]) {
        state.setApplyPropertyToRegularStyle(false);
        state.setApplyPropertyToVisitedLinkStyle(true);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchVisited], SelectorChecker::MatchVisited, matchResult);
    }

    state.setApplyPropertyToRegularStyle(true);
    state.setApplyPropertyToVisitedLinkStyle(false);
}

void StyleResolver::applyCascadedProperties(CascadedProperties& cascade, int firstProperty, int lastProperty, const MatchResult* matchResult)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!cascade.hasProperty(propertyID))
            continue;
        if (propertyID == CSSPropertyCustom) {
            HashMap<AtomicString, CascadedProperties::Property>::iterator end = cascade.customProperties().end();
            for (HashMap<AtomicString, CascadedProperties::Property>::iterator it = cascade.customProperties().begin(); it != end; ++it)
                it->value.apply(*this, matchResult);
            continue;
        }
        auto& property = cascade.property(propertyID);
        ASSERT(!shouldApplyPropertyInParseOrder(propertyID));
        property.apply(*this, matchResult);
    }
    
    if (firstProperty == CSSPropertyCustom)
        m_state.style()->checkVariablesInCustomProperties();
}

} // namespace WebCore
