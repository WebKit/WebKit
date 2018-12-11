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

#include "CSSCalculationValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSDefaultStyleSheets.h"
#include "CSSFilterImageValue.h"
#include "CSSFontSelector.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSPaintImageValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSShadowValue.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CachedResourceLoader.h"
#include "ElementRuleCollector.h"
#include "FilterOperation.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLMarqueeElement.h"
#include "HTMLNames.h"
#include "HTMLSlotElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "Logging.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "PageRuleCollector.h"
#include "PaintWorkletGlobalScope.h"
#include "Pair.h"
#include "RenderScrollbar.h"
#include "RenderStyleConstants.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SharedStringHash.h"
#include "StyleBuilder.h"
#include "StyleColor.h"
#include "StyleCachedImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "TransformFunctions.h"
#include "TransformOperations.h"
#include "UserAgentStyleSheets.h"
#include "ViewportStyleResolver.h"
#include "VisitedLinkState.h"
#include "WebKitFontFamilyNames.h"
#include <bitset>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>


#if ENABLE(DASHBOARD_SUPPORT)
#endif

#if ENABLE(VIDEO_TRACK)
#endif

namespace WebCore {

using namespace HTMLNames;

static const CSSPropertyID firstLowPriorityProperty = static_cast<CSSPropertyID>(lastHighPriorityProperty + 1);

static void extractDirectionAndWritingMode(const RenderStyle&, const StyleResolver::MatchResult&, TextDirection&, WritingMode&);

inline void StyleResolver::State::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

inline void StyleResolver::State::clear()
{
    m_element = nullptr;
    m_parentStyle = nullptr;
    m_ownedParentStyle = nullptr;
    m_cssToLengthConversionData = CSSToLengthConversionData();
}

void StyleResolver::MatchResult::addMatchedProperties(const StyleProperties& properties, StyleRule* rule, unsigned linkMatchType, PropertyWhitelistType propertyWhitelistType, Style::ScopeOrdinal styleScopeOrdinal)
{
    m_matchedProperties.grow(m_matchedProperties.size() + 1);
    StyleResolver::MatchedProperties& newProperties = m_matchedProperties.last();
    newProperties.properties = const_cast<StyleProperties*>(&properties);
    newProperties.linkMatchType = linkMatchType;
    newProperties.whitelistType = propertyWhitelistType;
    newProperties.styleScopeOrdinal = styleScopeOrdinal;
    matchedRules.append(rule);

    if (styleScopeOrdinal != Style::ScopeOrdinal::Element)
        isCacheable = false;

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
                // which is an inherited value, making the non-inherited property implicitly inherited.
                if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueCurrentcolor) {
                    isCacheable = false;
                    break;
                }

                if (value.hasVariableReferences()) {
                    isCacheable = false;
                    break;
                }
            }
        }
    }
}

StyleResolver::StyleResolver(Document& document)
    : m_ruleSets(*this)
    , m_matchedPropertiesCacheSweepTimer(*this, &StyleResolver::sweepMatchedPropertiesCache)
    , m_document(document)
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
#endif
    , m_styleMap(this)
    , m_matchAuthorAndUserStyles(m_document.settings().authorAndUserStylesEnabled())
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
        m_mediaQueryEvaluator = MediaQueryEvaluator { view->mediaType() };
    else
        m_mediaQueryEvaluator = MediaQueryEvaluator { "all" };

    if (root) {
        m_rootDefaultStyle = styleForElement(*root, m_document.renderStyle(), nullptr, RuleMatchingBehavior::MatchOnlyUserAgentRules).renderStyle;
        // Turn off assertion against font lookups during style resolver initialization. We may need root style font for media queries.
        m_document.fontSelector().incrementIsComputingRootStyleFont();
        m_rootDefaultStyle->fontCascade().update(&m_document.fontSelector());
        m_rootDefaultStyle->fontCascade().primaryFont();
        m_document.fontSelector().decrementIsComputingRootStyleFont();
    }

    if (m_rootDefaultStyle && view)
        m_mediaQueryEvaluator = MediaQueryEvaluator { view->mediaType(), m_document, m_rootDefaultStyle.get() };

    m_ruleSets.resetAuthorStyle();
    m_ruleSets.resetUserAgentMediaQueryStyle();
}

void StyleResolver::addCurrentSVGFontFaceRules()
{
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
    m_ruleSets.appendAuthorStyleSheets(styleSheets, &m_mediaQueryEvaluator, m_inspectorCSSOMWrappers, this);

    if (auto renderView = document().renderView())
        renderView->style().fontCascade().update(&document().fontSelector());

#if ENABLE(CSS_DEVICE_ADAPTATION)
    viewportStyleResolver()->resolve();
#endif
}

// This is a simplified style setting function for keyframe styles
void StyleResolver::addKeyframeStyle(Ref<StyleRuleKeyframes>&& rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), WTFMove(rule));
}

StyleResolver::~StyleResolver()
{
    RELEASE_ASSERT(!m_isDeleted);
    m_isDeleted = true;

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

StyleResolver::State::State(const Element& element, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle, const SelectorFilter* selectorFilter)
    : m_element(&element)
    , m_parentStyle(parentStyle)
    , m_selectorFilter(selectorFilter)
    , m_elementLinkState(element.document().visitedLinkState().determineLinkState(element))
{
    bool resetStyleInheritance = hasShadowRootParent(element) && downcast<ShadowRoot>(element.parentNode())->resetStyleInheritance();
    if (resetStyleInheritance)
        m_parentStyle = nullptr;

    auto& document = element.document();
    auto* documentElement = document.documentElement();
    if (!documentElement || documentElement == &element)
        m_rootElementStyle = document.renderStyle();
    else
        m_rootElementStyle = documentElementStyle ? documentElementStyle : documentElement->renderStyle();

    updateConversionData();
}

inline void StyleResolver::State::updateConversionData()
{
    m_cssToLengthConversionData = CSSToLengthConversionData(m_style.get(), m_rootElementStyle, m_element ? m_element->document().renderView() : nullptr);
}

inline void StyleResolver::State::setStyle(std::unique_ptr<RenderStyle> style)
{
    m_style = WTFMove(style);
    updateConversionData();
}

inline void StyleResolver::State::setParentStyle(std::unique_ptr<RenderStyle> parentStyle)
{
    m_ownedParentStyle = WTFMove(parentStyle);
    m_parentStyle = m_ownedParentStyle.get();
}

static inline bool isAtShadowBoundary(const Element& element)
{
    auto* parentNode = element.parentNode();
    return parentNode && parentNode->isShadowRoot();
}

void StyleResolver::setNewStateWithElement(const Element& element)
{
    // Apply the declaration to the style. This is a simplified version of the logic in styleForElement.
    m_state = State(element, nullptr);
}

ElementStyle StyleResolver::styleForElement(const Element& element, const RenderStyle* parentStyle, const RenderStyle* parentBoxStyle, RuleMatchingBehavior matchingBehavior, const SelectorFilter* selectorFilter)
{
    RELEASE_ASSERT(!m_isDeleted);

    m_state = State(element, parentStyle, m_overrideDocumentElementStyle, selectorFilter);
    State& state = m_state;

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtr());
        state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    auto& style = *state.style();

    if (element.isLink()) {
        style.setIsLink(true);
        InsideLink linkState = state.elementLinkState();
        if (linkState != InsideLink::NotInside) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassVisited);
            if (forceVisited)
                linkState = InsideLink::InsideVisited;
        }
        style.setInsideLink(linkState);
    }

    CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(element);

    ElementRuleCollector collector(element, m_ruleSets, m_state.selectorFilter());
    collector.setMedium(&m_mediaQueryEvaluator);

    if (matchingBehavior == RuleMatchingBehavior::MatchOnlyUserAgentRules)
        collector.matchUARules();
    else
        collector.matchAllRules(m_matchAuthorAndUserStyles, matchingBehavior != RuleMatchingBehavior::MatchAllRulesExcludingSMIL);

    if (collector.matchedPseudoElementIds())
        style.setHasPseudoStyles(collector.matchedPseudoElementIds());

    // This is required for style sharing.
    if (collector.didMatchUncommonAttributeSelector())
        style.setUnique();

    auto elementStyleRelations = Style::commitRelationsToRenderStyle(style, element, collector.styleRelations());

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *state.parentStyle(), parentBoxStyle, &element);

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    state.clear(); // Clear out for the next resolve.

    return { state.takeStyle(), WTFMove(elementStyleRelations) };
}

std::unique_ptr<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleRuleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    RELEASE_ASSERT(!m_isDeleted);

    MatchResult result;
    result.addMatchedProperties(keyframe->properties());

    ASSERT(!m_state.style());

    State& state = m_state;

    // Create the style
    state.setStyle(RenderStyle::clonePtr(*elementStyle));
    state.setParentStyle(RenderStyle::clonePtr(*elementStyle));

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state.style(), result, direction, writingMode);

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    CascadedProperties cascade(direction, writingMode);
    cascade.addNormalMatches(result, 0, result.matchedProperties().size() - 1);

    ApplyCascadedPropertyState applyState { this, &cascade, &result };
    applyCascadedProperties(firstCSSProperty, lastHighPriorityProperty, applyState);

    // If our font got dirtied, update it now.
    updateFont();

    // Now resolve remaining custom properties and the rest, in any order
    for (auto it = cascade.customProperties().begin(); it != cascade.customProperties().end(); ++it)
        applyCascadedCustomProperty(it->key, applyState);
    applyCascadedProperties(firstLowPriorityProperty, lastCSSProperty, applyState);

    // If our font got dirtied by one of the non-essential font props, update it a second time.
    updateFont();

    cascade.applyDeferredProperties(*this, applyState);

    adjustRenderStyle(*state.style(), *state.parentStyle(), nullptr, nullptr);

    // Add all the animating properties to the keyframe.
    unsigned propertyCount = keyframe->properties().propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSPropertyID property = keyframe->properties().propertyAt(i).id();
        // Timing-function within keyframes is special, because it is not animated; it just
        // describes the timing function between this keyframe and the next.
        if (property != CSSPropertyAnimationTimingFunction)
            keyframeValue.addProperty(property);
    }

    return state.takeStyle();
}

bool StyleResolver::isAnimationNameValid(const String& name)
{
    return m_keyframesRuleMap.find(AtomicString(name).impl()) != m_keyframesRuleMap.end();
}

void StyleResolver::keyframeStylesForAnimation(const Element& element, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name.
    if (list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const StyleRuleKeyframes* keyframesRule = it->value.get();

    auto* keyframes = &keyframesRule->keyframes();
    Vector<Ref<StyleRuleKeyframe>> newKeyframesIfNecessary;

    bool hasDuplicateKeys = false;
    HashSet<double> keyframeKeys;
    for (auto& keyframe : *keyframes) {
        for (auto key : keyframe->keys()) {
            if (!keyframeKeys.add(key)) {
                hasDuplicateKeys = true;
                break;
            }
        }
        if (hasDuplicateKeys)
            break;
    }

    // FIXME: If HashMaps could have Ref<> as value types, we wouldn't need
    // to copy the HashMap into a Vector.
    if (hasDuplicateKeys) {
        // Merge duplicate key times.
        HashMap<double, RefPtr<StyleRuleKeyframe>> keyframesMap;

        for (auto& originalKeyframe : keyframesRule->keyframes()) {
            for (auto key : originalKeyframe->keys()) {
                if (auto keyframe = keyframesMap.get(key))
                    keyframe->mutableProperties().mergeAndOverrideOnConflict(originalKeyframe->properties());
                else {
                    auto StyleRuleKeyframe = StyleRuleKeyframe::create(MutableStyleProperties::create());
                    StyleRuleKeyframe.ptr()->setKey(key);
                    StyleRuleKeyframe.ptr()->mutableProperties().mergeAndOverrideOnConflict(originalKeyframe->properties());
                    keyframesMap.set(key, StyleRuleKeyframe.ptr());
                }
            }
        }

        for (auto& keyframe : keyframesMap.values())
            newKeyframesIfNecessary.append(*keyframe.get());

        keyframes = &newKeyframesIfNecessary;
    }

    // Construct and populate the style for each keyframe.
    for (auto& keyframe : *keyframes) {
        setNewStateWithElement(element);

        // Add this keyframe style to all the indicated key times
        for (auto key : keyframe->keys()) {
            KeyframeValue keyframeValue(0, nullptr);
            keyframeValue.setStyle(styleForKeyframe(elementStyle, keyframe.ptr(), keyframeValue));
            keyframeValue.setKey(key);
            if (auto timingFunctionCSSValue = keyframe->properties().getPropertyCSSValue(CSSPropertyAnimationTimingFunction))
                keyframeValue.setTimingFunction(TimingFunction::createFromCSSValue(*timingFunctionCSSValue.get()));
            list.insert(WTFMove(keyframeValue));
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe).
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key()) {
        static StyleRuleKeyframe* zeroPercentKeyframe;
        if (!zeroPercentKeyframe) {
            zeroPercentKeyframe = &StyleRuleKeyframe::create(MutableStyleProperties::create()).leakRef();
            zeroPercentKeyframe->setKey(0);
        }
        KeyframeValue keyframeValue(0, nullptr);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, zeroPercentKeyframe, keyframeValue));
        list.insert(WTFMove(keyframeValue));
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe).
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        static StyleRuleKeyframe* hundredPercentKeyframe;
        if (!hundredPercentKeyframe) {
            hundredPercentKeyframe = &StyleRuleKeyframe::create(MutableStyleProperties::create()).leakRef();
            hundredPercentKeyframe->setKey(1);
        }
        KeyframeValue keyframeValue(1, nullptr);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(WTFMove(keyframeValue));
    }
}

std::unique_ptr<RenderStyle> StyleResolver::pseudoStyleForElement(const Element& element, const PseudoStyleRequest& pseudoStyleRequest, const RenderStyle& parentStyle, const SelectorFilter* selectorFilter)
{
    m_state = State(element, &parentStyle, m_overrideDocumentElementStyle, selectorFilter);

    State& state = m_state;

    if (m_state.parentStyle()) {
        state.setStyle(RenderStyle::createPtr());
        state.style()->inheritFrom(*m_state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.

    // Check UA, user and author rules.
    ElementRuleCollector collector(element, m_ruleSets, m_state.selectorFilter());
    collector.setPseudoStyleRequest(pseudoStyleRequest);
    collector.setMedium(&m_mediaQueryEvaluator);
    collector.matchUARules();

    if (m_matchAuthorAndUserStyles) {
        collector.matchUserRules(false);
        collector.matchAuthorRules(false);
    }

    ASSERT(!collector.matchedPseudoElementIds());

    if (collector.matchedResult().matchedProperties().isEmpty())
        return nullptr;

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *m_state.parentStyle(), nullptr, nullptr);

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    // Now return the style.
    return state.takeStyle();
}

std::unique_ptr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    RELEASE_ASSERT(!m_isDeleted);

    auto* documentElement = m_document.documentElement();
    if (!documentElement)
        return RenderStyle::createPtr();

    m_state = State(*documentElement, m_document.renderStyle());

    m_state.setStyle(RenderStyle::createPtr());
    m_state.style()->inheritFrom(*m_state.rootElementStyle());

    PageRuleCollector collector(m_state, m_ruleSets);
    collector.matchAllPageRules(pageIndex);

    MatchResult& result = collector.matchedResult();

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*m_state.style(), result, direction, writingMode);

    CascadedProperties cascade(direction, writingMode);
    cascade.addNormalMatches(result, 0, result.matchedProperties().size() - 1);

    ApplyCascadedPropertyState applyState { this, &cascade, &result };
    applyCascadedProperties(firstCSSProperty, lastHighPriorityProperty, applyState);

    // If our font got dirtied, update it now.
    updateFont();

    // Now resolve remaining custom properties and the rest, in any order
    for (auto it = cascade.customProperties().begin(); it != cascade.customProperties().end(); ++it)
        applyCascadedCustomProperty(it->key, applyState);
    applyCascadedProperties(firstLowPriorityProperty, lastCSSProperty, applyState);

    cascade.applyDeferredProperties(*this, applyState);

    // Now return the style.
    return m_state.takeStyle();
}

std::unique_ptr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    m_state.setStyle(RenderStyle::createPtr());
    // Make sure our fonts are initialized if we don't inherit them from our parent style.
    initializeFontStyle();
    m_state.style()->fontCascade().update(&document().fontSelector());
    return m_state.takeStyle();
}

static void addIntrinsicMargins(RenderStyle& style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = clampToInteger(2 * style.effectiveZoom());

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

static DisplayType equivalentBlockDisplay(const RenderStyle& style, const Document& document)
{
    switch (auto display = style.display()) {
    case DisplayType::Block:
    case DisplayType::Table:
    case DisplayType::Box:
    case DisplayType::Flex:
    case DisplayType::WebKitFlex:
    case DisplayType::Grid:
        return display;

    case DisplayType::ListItem:
        // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk, but only in quirks mode.
        if (document.inQuirksMode() && style.isFloating())
            return DisplayType::Block;
        return display;
    case DisplayType::InlineTable:
        return DisplayType::Table;
    case DisplayType::InlineBox:
        return DisplayType::Box;
    case DisplayType::InlineFlex:
    case DisplayType::WebKitInlineFlex:
        return DisplayType::Flex;
    case DisplayType::InlineGrid:
        return DisplayType::Grid;

    case DisplayType::Inline:
    case DisplayType::Compact:
    case DisplayType::InlineBlock:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
        return DisplayType::Block;
    case DisplayType::Contents:
        ASSERT_NOT_REACHED();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED();
        return DisplayType::None;
    }
    ASSERT_NOT_REACHED();
    return DisplayType::Block;
}

// CSS requires text-decoration to be reset at each DOM element for tables, 
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements.
static bool doesNotInheritTextDecoration(const RenderStyle& style, const Element* element)
{
    return style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable
        || style.display() == DisplayType::InlineBlock || style.display() == DisplayType::InlineBox || (element && isAtShadowBoundary(*element))
        || style.isFloating() || style.hasOutOfFlowPosition();
}

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
static bool isScrollableOverflow(Overflow overflow)
{
    return overflow == Overflow::Scroll || overflow == Overflow::Auto;
}
#endif

void StyleResolver::adjustStyleForInterCharacterRuby()
{
    RenderStyle* style = m_state.style();
    if (style->rubyPosition() != RubyPosition::InterCharacter || !m_state.element() || !m_state.element()->hasTagName(rtTag))
        return;
    style->setTextAlign(TextAlignMode::Center);
    if (style->isHorizontalWritingMode())
        style->setWritingMode(LeftToRightWritingMode);
}

static bool hasEffectiveDisplayNoneForDisplayContents(const Element& element)
{
    // https://drafts.csswg.org/css-display-3/#unbox-html
    static NeverDestroyed<HashSet<AtomicString>> tagNames = [] {
        static const HTMLQualifiedName* const tagList[] = {
            &brTag.get(),
            &wbrTag.get(),
            &meterTag.get(),
            &appletTag.get(),
            &progressTag.get(),
            &canvasTag.get(),
            &embedTag.get(),
            &objectTag.get(),
            &audioTag.get(),
            &iframeTag.get(),
            &imgTag.get(),
            &videoTag.get(),
            &frameTag.get(),
            &framesetTag.get(),
            &inputTag.get(),
            &textareaTag.get(),
            &selectTag.get(),
        };
        HashSet<AtomicString> set;
        for (auto& name : tagList)
            set.add(name->localName());
        return set;
    }();

    // https://drafts.csswg.org/css-display-3/#unbox-svg
    // FIXME: <g>, <use> and <tspan> have special (?) behavior for display:contents in the current draft spec.
    if (is<SVGElement>(element))
        return true;
#if ENABLE(MATHML)
    // Not sure MathML code can handle it.
    if (is<MathMLElement>(element))
        return true;
#endif // ENABLE(MATHML)
    if (!is<HTMLElement>(element))
        return false;
    return tagNames.get().contains(element.localName());
}

static void adjustDisplayContentsStyle(RenderStyle& style, const Element* element)
{
    bool displayContentsEnabled = is<HTMLSlotElement>(element) || RuntimeEnabledFeatures::sharedFeatures().displayContentsEnabled();
    if (!displayContentsEnabled) {
        style.setDisplay(DisplayType::Inline);
        return;
    }
    if (!element) {
        if (style.styleType() != PseudoId::Before && style.styleType() != PseudoId::After)
            style.setDisplay(DisplayType::None);
        return;
    }
    if (element->document().documentElement() == element) {
        style.setDisplay(DisplayType::Block);
        return;
    }
    if (hasEffectiveDisplayNoneForDisplayContents(*element))
        style.setDisplay(DisplayType::None);
}

void StyleResolver::adjustSVGElementStyle(const SVGElement& svgElement, RenderStyle& style)
{
    // Only the root <svg> element in an SVG document fragment tree honors css position
    auto isPositioningAllowed = svgElement.hasTagName(SVGNames::svgTag) && svgElement.parentNode() && !svgElement.parentNode()->isSVGElement() && !svgElement.correspondingElement();
    if (!isPositioningAllowed)
        style.setPosition(RenderStyle::initialPosition());

    // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
    // not be scaled again.
    if (svgElement.hasTagName(SVGNames::foreignObjectTag))
        style.setEffectiveZoom(RenderStyle::initialZoom());

    // SVG text layout code expects us to be a block-level style element.
    if ((svgElement.hasTagName(SVGNames::foreignObjectTag) || svgElement.hasTagName(SVGNames::textTag)) && style.isDisplayInlineType())
        style.setDisplay(DisplayType::Block);
}

void StyleResolver::adjustRenderStyle(RenderStyle& style, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, const Element* element)
{
    // If the composed tree parent has display:contents, the parent box style will be different from the parent style.
    // We don't have it when resolving computed style for display:none subtree. Use parent style for adjustments in that case.
    if (!parentBoxStyle)
        parentBoxStyle = &parentStyle;

    // Cache our original display.
    style.setOriginalDisplay(style.display());

    if (style.display() == DisplayType::Contents)
        adjustDisplayContentsStyle(style, element);

    if (style.display() != DisplayType::None && style.display() != DisplayType::Contents) {
        if (element) {
            // If we have a <td> that specifies a float property, in quirks mode we just drop the float
            // property.
            // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
            // these tags to retain their display types.
            if (document().inQuirksMode()) {
                if (element->hasTagName(tdTag)) {
                    style.setDisplay(DisplayType::TableCell);
                    style.setFloating(Float::No);
                } else if (is<HTMLTableElement>(*element))
                    style.setDisplay(style.isDisplayInlineType() ? DisplayType::InlineTable : DisplayType::Table);
            }

            if (element->hasTagName(tdTag) || element->hasTagName(thTag)) {
                if (style.whiteSpace() == WhiteSpace::KHTMLNoWrap) {
                    // Figure out if we are really nowrapping or if we should just
                    // use normal instead. If the width of the cell is fixed, then
                    // we don't actually use WhiteSpace::NoWrap.
                    if (style.width().isFixed())
                        style.setWhiteSpace(WhiteSpace::Normal);
                    else
                        style.setWhiteSpace(WhiteSpace::NoWrap);
                }
            }

            // Tables never support the -webkit-* values for text-align and will reset back to the default.
            if (is<HTMLTableElement>(*element) && (style.textAlign() == TextAlignMode::WebKitLeft || style.textAlign() == TextAlignMode::WebKitCenter || style.textAlign() == TextAlignMode::WebKitRight))
                style.setTextAlign(TextAlignMode::Start);

            // Frames and framesets never honor position:relative or position:absolute. This is necessary to
            // fix a crash where a site tries to position these objects. They also never honor display.
            if (element->hasTagName(frameTag) || element->hasTagName(framesetTag)) {
                style.setPosition(PositionType::Static);
                style.setDisplay(DisplayType::Block);
            }

            // Ruby text does not support float or position. This might change with evolution of the specification.
            if (element->hasTagName(rtTag)) {
                style.setPosition(PositionType::Static);
                style.setFloating(Float::No);
            }

            // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
            // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
            // just a single declaration that sets the 'text-align' property to the value 'center'.
            // https://html.spec.whatwg.org/multipage/rendering.html#rendering
            if (element->hasTagName(thTag) && !style.hasExplicitlySetTextAlign() && parentStyle.textAlign() == RenderStyle::initialTextAlign())
                style.setTextAlign(TextAlignMode::Center);

            if (element->hasTagName(legendTag))
                style.setDisplay(DisplayType::Block);
        }

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (element && element->document().documentElement() == element))
            style.setDisplay(equivalentBlockDisplay(style, document()));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == DisplayType::Inline && style.styleType() == PseudoId::None && style.writingMode() != parentStyle.writingMode())
            style.setDisplay(DisplayType::InlineBlock);

        // After performing the display mutation, check table rows. We do not honor position:relative or position:sticky on
        // table rows or cells. This has been established for position:relative in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style.display() == DisplayType::TableHeaderGroup || style.display() == DisplayType::TableRowGroup
            || style.display() == DisplayType::TableFooterGroup || style.display() == DisplayType::TableRow)
            && style.position() == PositionType::Relative)
            style.setPosition(PositionType::Static);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style.display() == DisplayType::TableColumn || style.display() == DisplayType::TableColumnGroup || style.display() == DisplayType::TableFooterGroup
            || style.display() == DisplayType::TableHeaderGroup || style.display() == DisplayType::TableRow || style.display() == DisplayType::TableRowGroup
            || style.display() == DisplayType::TableCell)
            style.setWritingMode(parentStyle.writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style.writingMode() != TopToBottomWritingMode && (style.display() == DisplayType::Box || style.display() == DisplayType::InlineBox))
            style.setWritingMode(TopToBottomWritingMode);

        // https://www.w3.org/TR/css-display/#transformations
        // "A parent with a grid or flex display value blockifies the box’s display type."
        if (parentBoxStyle->isDisplayFlexibleOrGridBox()) {
            style.setFloating(Float::No);
            style.setDisplay(equivalentBlockDisplay(style, document()));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style.position() == PositionType::Static && !parentBoxStyle->isDisplayFlexibleOrGridBox())
        style.setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoZIndex()) {
        if ((element && element->document().documentElement() == element)
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
            || style.position() == PositionType::Sticky
            || style.position() == PositionType::Fixed
            || style.willChangeCreatesStackingContext())
            style.setZIndex(0);
    }

    if (element) {
        // Textarea considers overflow visible as auto.
        if (is<HTMLTextAreaElement>(*element)) {
            style.setOverflowX(style.overflowX() == Overflow::Visible ? Overflow::Auto : style.overflowX());
            style.setOverflowY(style.overflowY() == Overflow::Visible ? Overflow::Auto : style.overflowY());
        }

        // Disallow -webkit-user-modify on :pseudo and ::pseudo elements.
        if (!element->shadowPseudoId().isNull())
            style.setUserModify(UserModify::ReadOnly);

        if (is<HTMLMarqueeElement>(*element)) {
            // For now, <marquee> requires an overflow clip to work properly.
            style.setOverflowX(Overflow::Hidden);
            style.setOverflowY(Overflow::Hidden);

            bool isVertical = style.marqueeDirection() == MarqueeDirection::Up || style.marqueeDirection() == MarqueeDirection::Down;
            // Make horizontal marquees not wrap.
            if (!isVertical) {
                style.setWhiteSpace(WhiteSpace::NoWrap);
                style.setTextAlign(TextAlignMode::Start);
            }
            // Apparently this is the expected legacy behavior.
            if (isVertical && style.height().isAuto())
                style.setHeight(Length(200, Fixed));
        }
    }

    if (doesNotInheritTextDecoration(style, element))
        style.setTextDecorationsInEffect(style.textDecoration());
    else
        style.addToTextDecorationsInEffect(style.textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style.overflowX() == Overflow::Visible && style.overflowY() != Overflow::Visible) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style.setOverflowX(Overflow::Auto);
    } else if (style.overflowY() == Overflow::Visible && style.overflowX() != Overflow::Visible)
        style.setOverflowY(Overflow::Auto);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // Style::createForDocument().
    if ((style.overflowY() == Overflow::PagedX || style.overflowY() == Overflow::PagedY) && !(element && (element->hasTagName(htmlTag) || element->hasTagName(bodyTag))))
        style.setColumnStylesFromPaginationMode(WebCore::paginationModeForRenderStyle(style));

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable
        || style.display() == DisplayType::TableRowGroup || style.display() == DisplayType::TableRow) {
        if (style.overflowX() != Overflow::Visible && style.overflowX() != Overflow::Hidden)
            style.setOverflowX(Overflow::Visible);
        if (style.overflowY() != Overflow::Visible && style.overflowY() != Overflow::Hidden)
            style.setOverflowY(Overflow::Visible);
    }

    // Menulists should have visible overflow
    if (style.appearance() == MenulistPart) {
        style.setOverflowX(Overflow::Visible);
        style.setOverflowY(Overflow::Visible);
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
    if (is<HTMLFormControlElement>(element) && style.computedFontPixelSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!is<HTMLInputElement>(*element) || !downcast<HTMLInputElement>(*element).isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        RenderTheme::singleton().adjustStyle(*this, style, element, m_state.hasUAAppearance(), m_state.borderData(), m_state.backgroundData(), m_state.backgroundColor());

    // If we have first-letter pseudo style, do not share this style.
    if (style.hasPseudoStyle(PseudoId::FirstLetter))
        style.setUnique();

    // FIXME: when dropping the -webkit prefix on transform-style, we should also have opacity < 1 cause flattening.
    if (style.preserves3D() && (style.overflowX() != Overflow::Visible
        || style.overflowY() != Overflow::Visible
        || style.hasClip()
        || style.clipPath()
        || style.hasFilter()
#if ENABLE(FILTERS_LEVEL_2)
        || style.hasBackdropFilter()
#endif
        || style.hasBlendMode()))
        style.setTransformStyle3D(TransformStyle3D::Flat);

    if (is<SVGElement>(element))
        adjustSVGElementStyle(downcast<SVGElement>(*element), style);

    // If the inherited value of justify-items includes the 'legacy' keyword (plus 'left', 'right' or
    // 'center'), 'legacy' computes to the the inherited value. Otherwise, 'auto' computes to 'normal'.
    if (parentBoxStyle->justifyItems().positionType() == ItemPositionType::Legacy && style.justifyItems().position() == ItemPosition::Legacy)
        style.setJustifyItems(parentBoxStyle->justifyItems());
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
    style->setFontDescription(WTFMove(newFontDescription));
}

void StyleResolver::updateFont()
{
    if (!m_state.fontDirty())
        return;

    RenderStyle* style = m_state.style();
#if ENABLE(TEXT_AUTOSIZING)
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

Vector<RefPtr<StyleRule>> StyleResolver::styleRulesForElement(const Element* element, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(element, PseudoId::None, rulesToInclude);
}

Vector<RefPtr<StyleRule>> StyleResolver::pseudoStyleRulesForElement(const Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!element)
        return { };

    m_state = State(*element, nullptr);

    ElementRuleCollector collector(*element, m_ruleSets, m_state.selectorFilter());
    collector.setMode(SelectorChecker::Mode::CollectingRules);
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));
    collector.setMedium(&m_mediaQueryEvaluator);

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        collector.matchUARules();

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            collector.matchUserRules(rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules))
        collector.matchAuthorRules(rulesToInclude & EmptyCSSRules);

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
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationSkip:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyTextUnderlineOffset:
    case CSSPropertyTextDecorationThickness:
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
        || localName == HTMLNames::meterTag;
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
        static const Seconds matchedDeclarationCacheSweepTime { 1_min };
        m_matchedPropertiesCacheSweepTimer.startOneShot(matchedDeclarationCacheSweepTime);
    }

    ASSERT(hash);
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    MatchedPropertiesCacheItem cacheItem(matchResult, style, parentStyle);
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
    // content:attr() value depends on the element it is being applied to.
    if (style->hasAttrContent() || (style->styleType() != PseudoId::None && parentStyle->hasAttrContent()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    if (style->writingMode() != RenderStyle::initialWritingMode() || style->direction() != RenderStyle::initialDirection())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (style->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void extractDirectionAndWritingMode(const RenderStyle& style, const StyleResolver::MatchResult& matchResult, TextDirection& direction, WritingMode& writingMode)
{
    direction = style.direction();
    writingMode = style.writingMode();

    bool hadImportantWritingMode = false;
    bool hadImportantDirection = false;

    for (const auto& matchedProperties : matchResult.matchedProperties()) {
        for (unsigned i = 0, count = matchedProperties.properties->propertyCount(); i < count; ++i) {
            auto property = matchedProperties.properties->propertyAt(i);
            if (!property.value()->isPrimitiveValue())
                continue;
            switch (property.id()) {
            case CSSPropertyWritingMode:
                if (!hadImportantWritingMode || property.isImportant()) {
                    writingMode = downcast<CSSPrimitiveValue>(*property.value());
                    hadImportantWritingMode = property.isImportant();
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
        state.style()->copyNonInheritedFrom(*cacheItem->renderStyle);
        if (state.parentStyle()->inheritedDataShared(cacheItem->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
            InsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(*cacheItem->renderStyle);

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
        cascade.addNormalMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
        cascade.addImportantMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

        ApplyCascadedPropertyState applyState { this, &cascade, &matchResult };

        applyCascadedProperties(CSSPropertyWebkitRubyPosition, CSSPropertyWebkitRubyPosition, applyState);
        adjustStyleForInterCharacterRuby();

#if ENABLE(DARK_MODE_CSS)
        // Supported color schemes can affect resolved colors, so we need to apply that property before any color properties.
        applyCascadedProperties(CSSPropertySupportedColorSchemes, CSSPropertySupportedColorSchemes, applyState);
#endif

        applyCascadedProperties(firstCSSProperty, lastHighPriorityProperty, applyState);

        // If our font got dirtied, update it now.
        updateFont();

        // Now resolve remaining custom properties and the rest, in any order
        for (auto it = cascade.customProperties().begin(); it != cascade.customProperties().end(); ++it)
            applyCascadedCustomProperty(it->key, applyState);
        applyCascadedProperties(firstLowPriorityProperty, lastCSSProperty, applyState);

        state.cacheBorderAndBackground();
    }

    CascadedProperties cascade(direction, writingMode);
    cascade.addNormalMatches(matchResult, 0, matchResult.matchedProperties().size() - 1, applyInheritedOnly);
    cascade.addImportantMatches(matchResult, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    cascade.addImportantMatches(matchResult, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    cascade.addImportantMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    ApplyCascadedPropertyState applyState { this, &cascade, &matchResult };

    applyCascadedProperties(CSSPropertyWebkitRubyPosition, CSSPropertyWebkitRubyPosition, applyState);
    adjustStyleForInterCharacterRuby();

#if ENABLE(DARK_MODE_CSS)
    // Supported color schemes can affect resolved colors, so we need to apply that property before any color properties.
    applyCascadedProperties(CSSPropertySupportedColorSchemes, CSSPropertySupportedColorSchemes, applyState);
#endif

    applyCascadedProperties(firstCSSProperty, lastHighPriorityProperty, applyState);

    // If the effective zoom value changes, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->effectiveZoom() != state.style()->effectiveZoom())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // If our font got dirtied, update it now.
    updateFont();

    // If the font changed, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->fontDescription() != state.style()->fontDescription())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // Now resolve remaining custom properties and the rest, in any order
    for (auto it = cascade.customProperties().begin(); it != cascade.customProperties().end(); ++it)
        applyCascadedCustomProperty(it->key, applyState);
    applyCascadedProperties(firstLowPriorityProperty, lastCSSProperty, applyState);

    // Finally, some properties must be applied in the order they were parsed.
    // There are some CSS properties that affect the same RenderStyle values,
    // so to preserve behavior, we queue them up during cascade and flush here.
    cascade.applyDeferredProperties(*this, applyState);

    ASSERT(!state.fontDirty());

    if (cacheItem || !cacheHash)
        return;
    if (!isCacheableInMatchedPropertiesCache(*state.element(), state.style(), state.parentStyle()))
        return;
    addToMatchedPropertiesCache(state.style(), state.parentStyle(), cacheHash, matchResult);
}

void StyleResolver::applyPropertyToStyle(CSSPropertyID id, CSSValue* value, std::unique_ptr<RenderStyle> style)
{
    m_state = State();
    m_state.setParentStyle(RenderStyle::clonePtr(*style));
    m_state.setStyle(WTFMove(style));
    applyPropertyToCurrentStyle(id, value);
}

void StyleResolver::applyPropertyToCurrentStyle(CSSPropertyID id, CSSValue* value)
{
    ApplyCascadedPropertyState applyState { this, nullptr, nullptr };
    if (value)
        applyProperty(id, value, applyState);
}

inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyFill:
    case CSSPropertyStroke:
    case CSSPropertyStrokeColor:
        return true;
    default:
        break;
    }

    return false;
}

// https://www.w3.org/TR/css-pseudo-4/#marker-pseudo (Editor's Draft, 25 July 2017)
static inline bool isValidMarkerStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyColor:
    case CSSPropertyFontFamily:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontSynthesis:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
    case CSSPropertyFontVariationSettings:
#endif
        return true;
    default:
        break;
    }
    return false;
}

#if ENABLE(VIDEO_TRACK)
static inline bool isValidCueStyleProperty(CSSPropertyID id)
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
    case CSSPropertyPaintOrder:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeColor:
    case CSSPropertyStrokeWidth:
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
bool StyleResolver::useSVGZoomRules() const
{
    return m_state.element() && m_state.element()->isSVGElement();
}

// Scale with/height properties on inline SVG root.
bool StyleResolver::useSVGZoomRulesForLength() const
{
    return is<SVGElement>(m_state.element()) && !(is<SVGSVGElement>(*m_state.element()) && m_state.element()->parentNode());
}

StyleResolver::CascadedProperties* StyleResolver::cascadedPropertiesForRollback(const MatchResult& matchResult)
{
    ASSERT(cascadeLevel() != CascadeLevel::UserAgentLevel);

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state().style(), matchResult, direction, writingMode);

    if (cascadeLevel() == CascadeLevel::AuthorLevel) {
        CascadedProperties* authorRollback = state().authorRollback();
        if (authorRollback)
            return authorRollback;

        auto newAuthorRollback(std::make_unique<CascadedProperties>(direction, writingMode));

        // This special rollback cascade contains UA rules and user rules but no author rules.
        newAuthorRollback->addNormalMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
        newAuthorRollback->addNormalMatches(matchResult, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, false);
        newAuthorRollback->addImportantMatches(matchResult, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, false);
        newAuthorRollback->addImportantMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);

        state().setAuthorRollback(newAuthorRollback);
        return state().authorRollback();
    }

    if (cascadeLevel() == CascadeLevel::UserLevel) {
        CascadedProperties* userRollback = state().userRollback();
        if (userRollback)
            return userRollback;

        auto newUserRollback(std::make_unique<CascadedProperties>(direction, writingMode));

        // This special rollback cascade contains only UA rules.
        newUserRollback->addNormalMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);
        newUserRollback->addImportantMatches(matchResult, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, false);

        state().setUserRollback(newUserRollback);
        return state().userRollback();
    }

    return nullptr;
}

void StyleResolver::applyProperty(CSSPropertyID id, CSSValue* value, ApplyCascadedPropertyState& applyState, SelectorChecker::LinkMatchMask linkMatchMask)
{
    auto* matchResult = applyState.matchResult;
    ASSERT_WITH_MESSAGE(!isShorthandCSSProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    State& state = m_state;

    RefPtr<CSSValue> valueToApply = value;
    if (value->hasVariableReferences()) {
        valueToApply = resolvedVariableValue(id, *value, applyState);
        // If appliedProperties already has this id, then we detected a cycle, and this value should be unset.
        if (!valueToApply || applyState.appliedProperties.get(id)) {
            if (CSSProperty::isInheritedProperty(id))
                valueToApply = CSSValuePool::singleton().createInheritedValue();
            else
                valueToApply = CSSValuePool::singleton().createExplicitInitialValue();
        }
    }

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, state.style()->direction(), state.style()->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), applyState, linkMatchMask);
    }

    CSSValue* valueToCheckForInheritInitial = valueToApply.get();
    CSSCustomPropertyValue* customPropertyValue = nullptr;
    CSSValueID customPropertyValueID = CSSValueInvalid;

    CSSRegisteredCustomProperty* customPropertyRegistered = nullptr;

    if (id == CSSPropertyCustom) {
        customPropertyValue = &downcast<CSSCustomPropertyValue>(*valueToApply);
        ASSERT(customPropertyValue->isResolved());
        if (WTF::holds_alternative<CSSValueID>(customPropertyValue->value()))
            customPropertyValueID = WTF::get<CSSValueID>(customPropertyValue->value());
        auto& name = customPropertyValue->name();
        customPropertyRegistered = document().getCSSRegisteredCustomPropertySet().get(name);
    }

    bool isInherit = state.parentStyle() ? valueToCheckForInheritInitial->isInheritedValue() || customPropertyValueID == CSSValueInherit : false;
    bool isInitial = valueToCheckForInheritInitial->isInitialValue() || customPropertyValueID == CSSValueInitial || (!state.parentStyle() && (valueToCheckForInheritInitial->isInheritedValue() || customPropertyValueID == CSSValueInherit));

    bool isUnset = valueToCheckForInheritInitial->isUnsetValue() || customPropertyValueID == CSSValueUnset;
    bool isRevert = valueToCheckForInheritInitial->isRevertValue() || customPropertyValueID == CSSValueRevert;

    if (isRevert) {
        if (cascadeLevel() == CascadeLevel::UserAgentLevel || !matchResult)
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
                if (customPropertyRegistered && customPropertyRegistered->inherits && rollback->hasCustomProperty(customPropertyValue->name())) {
                    auto property = rollback->customProperty(customPropertyValue->name());
                    if (property.cssValue[linkMatchMask])
                        applyProperty(property.id, property.cssValue[linkMatchMask], applyState, linkMatchMask);
                    return;
                }
            } else if (rollback->hasProperty(id)) {
                auto& property = rollback->property(id);
                if (property.cssValue[linkMatchMask])
                    applyProperty(property.id, property.cssValue[linkMatchMask], applyState, linkMatchMask);
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

    if (isInherit && !CSSProperty::isInheritedProperty(id))
        state.style()->setHasExplicitlyInheritedProperties();

#if ENABLE(CSS_PAINTING_API)
    if (is<CSSPaintImageValue>(*valueToApply)) {
        auto& name = downcast<CSSPaintImageValue>(*valueToApply).name();
        if (auto* paintWorklet = document().paintWorkletGlobalScopeForName(name)) {
            auto locker = holdLock(paintWorklet->paintDefinitionLock());
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    state.style()->addCustomPaintWatchProperty(property);
            }
        }
    }
#endif

    // Use the generated StyleBuilder.
    StyleBuilder::applyProperty(id, *this, *valueToApply, isInitial, isInherit, customPropertyRegistered);
}

RefPtr<CSSValue> StyleResolver::resolvedVariableValue(CSSPropertyID propID, const CSSValue& value, ApplyCascadedPropertyState& state) const
{
    CSSParser parser(document());
    return parser.parseValueWithVariableReferences(propID, value, state);
}

RefPtr<StyleImage> StyleResolver::styleImage(CSSValue& value)
{
    if (is<CSSImageGeneratorValue>(value)) {
        if (is<CSSGradientValue>(value))
            return StyleGeneratedImage::create(downcast<CSSGradientValue>(value).gradientWithStylesResolved(*this));

        if (is<CSSFilterImageValue>(value)) {
            // FilterImage needs to calculate FilterOperations.
            downcast<CSSFilterImageValue>(value).createFilterOperations(this);
        }
        return StyleGeneratedImage::create(downcast<CSSImageGeneratorValue>(value));
    }

    if (is<CSSImageValue>(value) || is<CSSImageSetValue>(value) || is<CSSCursorImageValue>(value))
        return StyleCachedImage::create(value);

    return nullptr;
}

#if ENABLE(TEXT_AUTOSIZING)
void StyleResolver::checkForTextSizeAdjust(RenderStyle* style)
{
    if (style->textSizeAdjust().isAuto())
        return;

    auto newFontDescription = style->fontDescription();
    if (!style->textSizeAdjust().isNone())
        newFontDescription.setComputedSize(newFontDescription.specifiedSize() * style->textSizeAdjust().multiplier());
    else
        newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    style->setFontDescription(WTFMove(newFontDescription));
}
#endif

void StyleResolver::checkForZoomChange(RenderStyle* style, const RenderStyle* parentStyle)
{
    if (!parentStyle)
        return;

    if (style->effectiveZoom() == parentStyle->effectiveZoom() && style->textZoom() == parentStyle->textZoom())
        return;

    const auto& childFont = style->fontDescription();
    auto newFontDescription = childFont;
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(WTFMove(newFontDescription));
}

void StyleResolver::checkForGenericFamilyChange(RenderStyle* style, const RenderStyle* parentStyle)
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
        float fixedScaleFactor = (settings().defaultFixedFontSize() && settings().defaultFontSize())
            ? static_cast<float>(settings().defaultFixedFontSize()) / settings().defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
                childFont.specifiedSize() / fixedScaleFactor :
                childFont.specifiedSize() * fixedScaleFactor;
    }

    auto newFontDescription = childFont;
    setFontSize(newFontDescription, size);
    style->setFontDescription(WTFMove(newFontDescription));
}

void StyleResolver::initializeFontStyle()
{
    FontCascadeDescription fontDescription;
    fontDescription.setRenderingMode(settings().fontRenderingMode());
    fontDescription.setOneFamily(standardFamily);
    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    setFontSize(fontDescription, Style::fontSizeForKeyword(CSSValueMedium, false, document()));
    fontDescription.setShouldAllowUserInstalledFonts(settings().shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No);
    setFontDescription(WTFMove(fontDescription));
}

void StyleResolver::setFontSize(FontCascadeDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), m_state.style(), document()));
}

bool StyleResolver::colorFromPrimitiveValueIsDerivedFromElement(const CSSPrimitiveValue& value)
{
    switch (value.valueID()) {
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
        return value.color();

    auto identifier = value.valueID();
    switch (identifier) {
    case CSSValueWebkitText:
        return document().textColor();
    case CSSValueWebkitLink:
        return (m_state.element()->isLink() && forVisitedLink) ? document().visitedLinkColor() : document().linkColor();
    case CSSValueWebkitActivelink:
        return document().activeLinkColor();
    case CSSValueWebkitFocusRingColor:
        return RenderTheme::focusRingColor(document().styleColorOptions(m_state.style()));
    case CSSValueCurrentcolor:
        // Color is an inherited property so depending on it effectively makes the property inherited.
        // FIXME: Setting the flag as a side effect of calling this function is a bit oblique. Can we do better?
        m_state.style()->setHasExplicitlyInheritedProperties();
        return m_state.style()->color();
    default:
        return StyleColor::colorFromKeyword(identifier, document().styleColorOptions(m_state.style()));
    }
}

void StyleResolver::addViewportDependentMediaQueryResult(const MediaQueryExpression& expression, bool result)
{
    m_viewportDependentMediaQueryResults.append(MediaQueryResult { expression, result });
}

bool StyleResolver::hasMediaQueriesAffectedByViewportChange() const
{
    LOG(MediaQueries, "StyleResolver::hasMediaQueriesAffectedByViewportChange evaluating queries");
    for (auto& result : m_viewportDependentMediaQueryResults) {
        if (m_mediaQueryEvaluator.evaluate(result.expression) != result.result)
            return true;
    }
    return false;
}

void StyleResolver::addAccessibilitySettingsDependentMediaQueryResult(const MediaQueryExpression& expression, bool result)
{
    m_accessibilitySettingsDependentMediaQueryResults.append(MediaQueryResult { expression, result });
}

bool StyleResolver::hasMediaQueriesAffectedByAccessibilitySettingsChange() const
{
    LOG(MediaQueries, "StyleResolver::hasMediaQueriesAffectedByAccessibilitySettingsChange evaluating queries");
    for (auto& result : m_accessibilitySettingsDependentMediaQueryResults) {
        if (m_mediaQueryEvaluator.evaluate(result.expression) != result.result)
            return true;
    }
    return false;
}

void StyleResolver::addAppearanceDependentMediaQueryResult(const MediaQueryExpression& expression, bool result)
{
    m_appearanceDependentMediaQueryResults.append(MediaQueryResult { expression, result });
}

bool StyleResolver::hasMediaQueriesAffectedByAppearanceChange() const
{
    LOG(MediaQueries, "StyleResolver::hasMediaQueriesAffectedByAppearanceChange evaluating queries");
    for (auto& result : m_appearanceDependentMediaQueryResults) {
        if (m_mediaQueryEvaluator.evaluate(result.expression) != result.result)
            return true;
    }
    return false;
}

static FilterOperation::OperationType filterOperationForType(CSSValueID type)
{
    switch (type) {
    case CSSValueUrl:
        return FilterOperation::REFERENCE;
    case CSSValueGrayscale:
        return FilterOperation::GRAYSCALE;
    case CSSValueSepia:
        return FilterOperation::SEPIA;
    case CSSValueSaturate:
        return FilterOperation::SATURATE;
    case CSSValueHueRotate:
        return FilterOperation::HUE_ROTATE;
    case CSSValueInvert:
        return FilterOperation::INVERT;
    case CSSValueAppleInvertLightness:
        return FilterOperation::APPLE_INVERT_LIGHTNESS;
    case CSSValueOpacity:
        return FilterOperation::OPACITY;
    case CSSValueBrightness:
        return FilterOperation::BRIGHTNESS;
    case CSSValueContrast:
        return FilterOperation::CONTRAST;
    case CSSValueBlur:
        return FilterOperation::BLUR;
    case CSSValueDropShadow:
        return FilterOperation::DROP_SHADOW;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FilterOperation::NONE;
}

bool StyleResolver::createFilterOperations(const CSSValue& inValue, FilterOperations& outOperations)
{
    State& state = m_state;
    ASSERT(outOperations.isEmpty());

    if (is<CSSPrimitiveValue>(inValue)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(inValue);
        if (primitiveValue.valueID() == CSSValueNone)
            return true;
    }

    if (!is<CSSValueList>(inValue))
        return false;

    FilterOperations operations;
    for (auto& currentValue : downcast<CSSValueList>(inValue)) {

        if (is<CSSPrimitiveValue>(currentValue)) {
            auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
            if (!primitiveValue.isURI())
                continue;

            String cssUrl = primitiveValue.stringValue();
            URL url = document().completeURL(cssUrl);

            RefPtr<ReferenceFilterOperation> operation = ReferenceFilterOperation::create(cssUrl, url.fragmentIdentifier());
            operations.operations().append(operation);
            continue;
        }

        if (!is<CSSFunctionValue>(currentValue))
            continue;

        auto& filterValue = downcast<CSSFunctionValue>(currentValue.get());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue.name());

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

        switch (operationType) {
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE: {
            double amount = 1;
            if (filterValue.length() == 1) {
                amount = firstValue->doubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            double angle = 0;
            if (filterValue.length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST:
        case FilterOperation::OPACITY: {
            double amount = 1;
            if (filterValue.length() == 1) {
                amount = firstValue->doubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case FilterOperation::APPLE_INVERT_LIGHTNESS: {
            operations.operations().append(InvertLightnessFilterOperation::create());
            break;
        }
        case FilterOperation::BLUR: {
            Length stdDeviation = Length(0, Fixed);
            if (filterValue.length() >= 1)
                stdDeviation = convertToFloatLength(firstValue, state.cssToLengthConversionData());
            if (stdDeviation.isUndefined())
                return false;

            operations.operations().append(BlurFilterOperation::create(stdDeviation));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            if (filterValue.length() != 1)
                return false;

            const auto* cssValue = filterValue.itemWithoutBoundsCheck(0);
            if (!is<CSSShadowValue>(cssValue))
                continue;

            const auto& item = downcast<CSSShadowValue>(*cssValue);
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
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

inline StyleResolver::MatchedProperties::MatchedProperties() = default;

StyleResolver::MatchedProperties::~MatchedProperties() = default;

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

void StyleResolver::CascadedProperties::setPropertyInternal(Property& property, CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, Style::ScopeOrdinal styleScopeOrdinal)
{
    ASSERT(linkMatchType <= SelectorChecker::MatchAll);
    property.id = id;
    property.level = cascadeLevel;
    property.styleScopeOrdinal = styleScopeOrdinal;
    if (linkMatchType == SelectorChecker::MatchAll) {
        property.cssValue[0] = &cssValue;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else
        property.cssValue[linkMatchType] = &cssValue;
}

void StyleResolver::CascadedProperties::set(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, Style::ScopeOrdinal styleScopeOrdinal)
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
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
            customProperties().set(customValue.name(), property);
        } else {
            Property property = customProperties().get(customValue.name());
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
            customProperties().set(customValue.name(), property);
        }
        return;
    }

    if (!m_propertyIsPresent[id])
        memset(property.cssValue, 0, sizeof(property.cssValue));
    m_propertyIsPresent.set(id);
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
}

void StyleResolver::CascadedProperties::setDeferred(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, Style::ScopeOrdinal styleScopeOrdinal)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(id));
    ASSERT(shouldApplyPropertyInParseOrder(id));

    Property property;
    memset(property.cssValue, 0, sizeof(property.cssValue));
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
    m_deferredProperties.append(property);
}

static CascadeLevel cascadeLevelForIndex(const StyleResolver::MatchResult& matchResult, int index)
{
    if (index >= matchResult.ranges.firstUARule && index <= matchResult.ranges.lastUARule)
        return CascadeLevel::UserAgentLevel;
    if (index >= matchResult.ranges.firstUserRule && index <= matchResult.ranges.lastUserRule)
        return CascadeLevel::UserLevel;
    return CascadeLevel::AuthorLevel;
}

void StyleResolver::CascadedProperties::addMatch(const MatchResult& matchResult, unsigned index, bool isImportant, bool inheritedOnly)
{
    auto& matchedProperties = matchResult.matchedProperties()[index];
    auto& styleProperties = *matchedProperties.properties;

    auto propertyWhitelistType = static_cast<PropertyWhitelistType>(matchedProperties.whitelistType);
    auto cascadeLevel = cascadeLevelForIndex(matchResult, index);

    for (unsigned i = 0, count = styleProperties.propertyCount(); i < count; ++i) {
        auto current = styleProperties.propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // We apply the inherited properties only when using the property cache.
            // A match with a value that is explicitely inherited should never have been cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        CSSPropertyID propertyID = current.id();

#if ENABLE(VIDEO_TRACK)
        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(propertyID))
            continue;
#endif
        if (propertyWhitelistType == PropertyWhitelistMarker && !isValidMarkerStyleProperty(propertyID))
            continue;

        if (shouldApplyPropertyInParseOrder(propertyID))
            setDeferred(propertyID, *current.value(), matchedProperties.linkMatchType, cascadeLevel, matchedProperties.styleScopeOrdinal);
        else
            set(propertyID, *current.value(), matchedProperties.linkMatchType, cascadeLevel, matchedProperties.styleScopeOrdinal);
    }
}

void StyleResolver::CascadedProperties::addNormalMatches(const MatchResult& matchResult, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    for (int i = startIndex; i <= endIndex; ++i)
        addMatch(matchResult, i, false, inheritedOnly);
}

static bool hasImportantProperties(const StyleProperties& properties)
{
    for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
        if (properties.propertyAt(i).isImportant())
            return true;
    }
    return false;
}

void StyleResolver::CascadedProperties::addImportantMatches(const MatchResult& matchResult, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    struct IndexAndOrdinal {
        int index;
        Style::ScopeOrdinal ordinal;
    };
    Vector<IndexAndOrdinal> shadowTreeMatches;

    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties()[i];

        if (!hasImportantProperties(*matchedProperties.properties))
            continue;

        if (matchedProperties.styleScopeOrdinal != Style::ScopeOrdinal::Element) {
            shadowTreeMatches.append({ i, matchedProperties.styleScopeOrdinal });
            continue;
        }

        addMatch(matchResult, i, true, inheritedOnly);
    }

    if (shadowTreeMatches.isEmpty())
        return;

    // For !important properties a later shadow tree wins.
    // Match results are sorted in reverse tree context order so this is not needed for normal properties.
    std::stable_sort(shadowTreeMatches.begin(), shadowTreeMatches.end(), [] (const IndexAndOrdinal& a, const IndexAndOrdinal& b) {
        return a.ordinal < b.ordinal;
    });

    for (auto& match : shadowTreeMatches)
        addMatch(matchResult, match.index, true, inheritedOnly);
}

void StyleResolver::CascadedProperties::applyDeferredProperties(StyleResolver& resolver, ApplyCascadedPropertyState& applyState)
{
    for (auto& property : m_deferredProperties)
        property.apply(resolver, applyState);
}

void StyleResolver::CascadedProperties::Property::apply(StyleResolver& resolver, ApplyCascadedPropertyState& applyState)
{
    State& state = resolver.state();
    state.setCascadeLevel(level);
    state.setStyleScopeOrdinal(styleScopeOrdinal);

    if (cssValue[SelectorChecker::MatchDefault]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchDefault], applyState, SelectorChecker::MatchDefault);
    }

    if (state.style()->insideLink() == InsideLink::NotInside)
        return;

    if (cssValue[SelectorChecker::MatchLink]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchLink], applyState, SelectorChecker::MatchLink);
    }

    if (cssValue[SelectorChecker::MatchVisited]) {
        state.setApplyPropertyToRegularStyle(false);
        state.setApplyPropertyToVisitedLinkStyle(true);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchVisited], applyState, SelectorChecker::MatchVisited);
    }

    state.setApplyPropertyToRegularStyle(true);
    state.setApplyPropertyToVisitedLinkStyle(false);
}

void StyleResolver::applyCascadedCustomProperty(const String& name, ApplyCascadedPropertyState& state)
{
    if (state.appliedCustomProperties.contains(name) || !state.cascade->customProperties().contains(name))
        return;

    auto property = state.cascade->customProperties().get(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && this->state().style()->insideLink() == InsideLink::NotInside)
            continue;

        Ref<CSSCustomPropertyValue> valueToApply = CSSCustomPropertyValue::create(downcast<CSSCustomPropertyValue>(*property.cssValue[index]));

        if (state.inProgressPropertiesCustom.contains(name)) {
            // We are in a cycle, so reset the value.
            state.appliedCustomProperties.add(name);
            // Resolve this value so that we reset its dependencies
            if (WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value()))
                resolvedVariableValue(CSSPropertyCustom, valueToApply.get(), state);
            valueToApply = CSSCustomPropertyValue::createWithID(name, CSSValueUnset);
        }

        state.inProgressPropertiesCustom.add(name);

        if (WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            RefPtr<CSSValue> parsedValue = resolvedVariableValue(CSSPropertyCustom, valueToApply.get(), state);

            if (!parsedValue)
                parsedValue = CSSCustomPropertyValue::createWithID(name, CSSValueUnset);

            valueToApply = downcast<CSSCustomPropertyValue>(*parsedValue);
        }

        if (state.inProgressPropertiesCustom.contains(name)) {
            if (index == SelectorChecker::MatchDefault) {
                this->state().setApplyPropertyToRegularStyle(true);
                this->state().setApplyPropertyToVisitedLinkStyle(false);
            }

            if (index == SelectorChecker::MatchLink) {
                this->state().setApplyPropertyToRegularStyle(true);
                this->state().setApplyPropertyToVisitedLinkStyle(false);
            }

            if (index == SelectorChecker::MatchVisited) {
                this->state().setApplyPropertyToRegularStyle(false);
                this->state().setApplyPropertyToVisitedLinkStyle(true);
            }
            applyProperty(CSSPropertyCustom, valueToApply.ptr(), state, index);
        }
        state.inProgressPropertiesCustom.remove(name);
        state.appliedCustomProperties.add(name);
    }
}

void StyleResolver::applyCascadedProperties(int firstProperty, int lastProperty, ApplyCascadedPropertyState& state)
{
    if (LIKELY(state.cascade->customProperties().isEmpty()))
        return applyCascadedPropertiesImpl<CustomPropertyCycleTracking::Disabled>(firstProperty, lastProperty, state);
    return applyCascadedPropertiesImpl<CustomPropertyCycleTracking::Enabled>(firstProperty, lastProperty, state);
}

template<StyleResolver::CustomPropertyCycleTracking TrackCycles>
inline void StyleResolver::applyCascadedPropertiesImpl(int firstProperty, int lastProperty, ApplyCascadedPropertyState& state)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!state.cascade->hasProperty(propertyID))
            continue;
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = state.cascade->property(propertyID);
        ASSERT(!shouldApplyPropertyInParseOrder(propertyID));

        if (TrackCycles == CustomPropertyCycleTracking::Disabled) {
            // If we don't have any custom properties, then there can't be any cycles.
            property.apply(*this, state);
        } else {
            if (UNLIKELY(state.inProgressProperties.get(propertyID))) {
                // We are in a cycle (eg. setting font size using registered custom property value containing em).
                // So this value should be unset.
                state.appliedProperties.set(propertyID);
                // This property is in a cycle, and only the root of the call stack will have firstProperty != lastProperty.
                ASSERT(firstProperty == lastProperty);
                continue;
            }

            state.inProgressProperties.set(propertyID);
            property.apply(*this, state);
            state.appliedProperties.set(propertyID);
            state.inProgressProperties.set(propertyID, false);
        }
    }
}

} // namespace WebCore
