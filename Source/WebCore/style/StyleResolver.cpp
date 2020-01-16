/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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

#include "CSSFontSelector.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "ElementRuleCollector.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "PageRuleCollector.h"
#include "Pair.h"
#include "RenderScrollbar.h"
#include "RenderStyleConstants.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGFontFaceElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SharedStringHash.h"
#include "StyleAdjuster.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolveForDocument.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "UserAgentStyle.h"
#include "ViewportStyleResolver.h"
#include "VisitedLinkState.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {
namespace Style {

using namespace HTMLNames;

Resolver::Resolver(Document& document)
    : m_ruleSets(*this)
    , m_document(document)
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
#endif
    , m_matchAuthorAndUserStyles(m_document.settings().authorAndUserStylesEnabled())
{
    Element* root = m_document.documentElement();

    UserAgentStyle::initDefaultStyle(root);

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

void Resolver::addCurrentSVGFontFaceRules()
{
#if ENABLE(SVG_FONTS)
    if (m_document.svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = m_document.svgExtensions()->svgFontFaceElements();
        for (auto* svgFontFaceElement : svgFontFaceElements)
            m_document.fontSelector().addFontFaceRule(svgFontFaceElement->fontFaceRule(), svgFontFaceElement->isInUserAgentShadowTree());
    }
#endif
}

void Resolver::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets)
{
    m_ruleSets.appendAuthorStyleSheets(styleSheets, &m_mediaQueryEvaluator, m_inspectorCSSOMWrappers);

    if (auto renderView = document().renderView())
        renderView->style().fontCascade().update(&document().fontSelector());

#if ENABLE(CSS_DEVICE_ADAPTATION)
    viewportStyleResolver()->resolve();
#endif
}

// This is a simplified style setting function for keyframe styles
void Resolver::addKeyframeStyle(Ref<StyleRuleKeyframes>&& rule)
{
    AtomString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), WTFMove(rule));
}

Resolver::~Resolver()
{
    RELEASE_ASSERT(!m_document.isResolvingTreeStyle());
    RELEASE_ASSERT(!m_isDeleted);
    m_isDeleted = true;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    m_viewportStyleResolver->clearDocument();
#endif
}

Resolver::State::State(const Element& element, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle)
    : m_element(&element)
    , m_parentStyle(parentStyle)
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
}

inline void Resolver::State::setStyle(std::unique_ptr<RenderStyle> style)
{
    m_style = WTFMove(style);
}

inline void Resolver::State::setParentStyle(std::unique_ptr<RenderStyle> parentStyle)
{
    m_ownedParentStyle = WTFMove(parentStyle);
    m_parentStyle = m_ownedParentStyle.get();
}

static inline bool isAtShadowBoundary(const Element& element)
{
    auto* parentNode = element.parentNode();
    return parentNode && parentNode->isShadowRoot();
}

BuilderContext Resolver::builderContext(const State& state)
{
    return {
        m_document,
        *state.parentStyle(),
        state.rootElementStyle(),
        state.element()
    };
}

ElementStyle Resolver::styleForElement(const Element& element, const RenderStyle* parentStyle, const RenderStyle* parentBoxStyle, RuleMatchingBehavior matchingBehavior, const SelectorFilter* selectorFilter)
{
    RELEASE_ASSERT(!m_isDeleted);

    auto state = State(element, parentStyle, m_overrideDocumentElementStyle);

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtr());
        state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement(&element));
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    auto& style = *state.style();

    if (element.isLink()) {
        style.setIsLink(true);
        InsideLink linkState = document().visitedLinkState().determineLinkState(element);
        if (linkState != InsideLink::NotInside) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassVisited);
            if (forceVisited)
                linkState = InsideLink::InsideVisited;
        }
        style.setInsideLink(linkState);
    }

    UserAgentStyle::ensureDefaultStyleSheetsForElement(element);

    ElementRuleCollector collector(element, m_ruleSets, selectorFilter);
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

    auto elementStyleRelations = commitRelationsToRenderStyle(style, element, collector.styleRelations());

    applyMatchedProperties(state, collector.matchResult());

    Adjuster adjuster(document(), *state.parentStyle(), parentBoxStyle, &element);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    return { state.takeStyle(), WTFMove(elementStyleRelations) };
}

std::unique_ptr<RenderStyle> Resolver::styleForKeyframe(const Element& element, const RenderStyle* elementStyle, const StyleRuleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    RELEASE_ASSERT(!m_isDeleted);

    MatchResult result;
    result.authorDeclarations.append({ &keyframe->properties() });

    auto state = State(element, nullptr);

    state.setStyle(RenderStyle::clonePtr(*elementStyle));
    state.setParentStyle(RenderStyle::clonePtr(*elementStyle));

    Builder builder(*state.style(), builderContext(state), result, { CascadeLevel::Author });
    builder.applyAllProperties();

    Adjuster adjuster(document(), *state.parentStyle(), nullptr, nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

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

bool Resolver::isAnimationNameValid(const String& name)
{
    return m_keyframesRuleMap.find(AtomString(name).impl()) != m_keyframesRuleMap.end();
}

void Resolver::keyframeStylesForAnimation(const Element& element, const RenderStyle* elementStyle, KeyframeList& list)
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
        // Add this keyframe style to all the indicated key times
        for (auto key : keyframe->keys()) {
            KeyframeValue keyframeValue(0, nullptr);
            keyframeValue.setStyle(styleForKeyframe(element, elementStyle, keyframe.ptr(), keyframeValue));
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
        keyframeValue.setStyle(styleForKeyframe(element, elementStyle, zeroPercentKeyframe, keyframeValue));
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
        keyframeValue.setStyle(styleForKeyframe(element, elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(WTFMove(keyframeValue));
    }
}

std::unique_ptr<RenderStyle> Resolver::pseudoStyleForElement(const Element& element, const PseudoElementRequest& pseudoElementRequest, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, const SelectorFilter* selectorFilter)
{
    auto state = State(element, &parentStyle, m_overrideDocumentElementStyle);

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtr());
        state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement(&element));
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    ElementRuleCollector collector(element, m_ruleSets, selectorFilter);
    collector.setPseudoElementRequest(pseudoElementRequest);
    collector.setMedium(&m_mediaQueryEvaluator);
    collector.matchUARules();

    if (m_matchAuthorAndUserStyles) {
        collector.matchUserRules();
        collector.matchAuthorRules();
    }

    ASSERT(!collector.matchedPseudoElementIds());

    if (collector.matchResult().isEmpty())
        return nullptr;

    state.style()->setStyleType(pseudoElementRequest.pseudoId);

    applyMatchedProperties(state, collector.matchResult());

    Adjuster adjuster(document(), *state.parentStyle(), parentBoxStyle, nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    return state.takeStyle();
}

std::unique_ptr<RenderStyle> Resolver::styleForPage(int pageIndex)
{
    RELEASE_ASSERT(!m_isDeleted);

    auto* documentElement = m_document.documentElement();
    if (!documentElement)
        return RenderStyle::createPtr();

    auto state = State(*documentElement, m_document.renderStyle());

    state.setStyle(RenderStyle::createPtr());
    state.style()->inheritFrom(*state.rootElementStyle());

    PageRuleCollector collector(state, m_ruleSets);
    collector.matchAllPageRules(pageIndex);

    auto& result = collector.matchResult();

    Builder builder(*state.style(), builderContext(state), result, { CascadeLevel::Author });
    builder.applyAllProperties();

    // Now return the style.
    return state.takeStyle();
}

std::unique_ptr<RenderStyle> Resolver::defaultStyleForElement(const Element* element)
{
    auto style = RenderStyle::createPtr();

    FontCascadeDescription fontDescription;
    fontDescription.setRenderingMode(settings().fontRenderingMode());
    fontDescription.setOneFamily(standardFamily);
    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);

    auto size = fontSizeForKeyword(CSSValueMedium, false, document());
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), is<SVGElement>(element), style.get(), document()));

    fontDescription.setShouldAllowUserInstalledFonts(settings().shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No);
    style->setFontDescription(WTFMove(fontDescription));

    style->fontCascade().update(&document().fontSelector());

    return style;
}

Vector<RefPtr<const StyleRule>> Resolver::styleRulesForElement(const Element* element, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(element, PseudoId::None, rulesToInclude);
}

Vector<RefPtr<const StyleRule>> Resolver::pseudoStyleRulesForElement(const Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!element)
        return { };

    auto state = State(*element, nullptr);

    ElementRuleCollector collector(*element, m_ruleSets, nullptr);
    collector.setMode(SelectorChecker::Mode::CollectingRules);
    collector.setPseudoElementRequest({ pseudoId });
    collector.setMedium(&m_mediaQueryEvaluator);
    collector.setIncludeEmptyRules(rulesToInclude & EmptyCSSRules);

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        collector.matchUARules();

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            collector.matchUserRules();
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules))
        collector.matchAuthorRules();

    return collector.matchedRuleList();
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

void Resolver::invalidateMatchedDeclarationsCache()
{
    m_matchedDeclarationsCache.invalidate();
}

void Resolver::clearCachedDeclarationsAffectedByViewportUnits()
{
    m_matchedDeclarationsCache.clearEntriesAffectedByViewportUnits();
}

void Resolver::applyMatchedProperties(State& state, const MatchResult& matchResult, UseMatchedDeclarationsCache useMatchedDeclarationsCache)
{
    unsigned cacheHash = useMatchedDeclarationsCache == UseMatchedDeclarationsCache::Yes ? MatchedDeclarationsCache::computeHash(matchResult) : 0;
    auto includedProperties = PropertyCascade::IncludedProperties::All;

    auto& style = *state.style();
    auto& parentStyle = *state.parentStyle();
    auto& element = *state.element();

    auto* cacheEntry = m_matchedDeclarationsCache.find(cacheHash, matchResult);
    if (cacheEntry && MatchedDeclarationsCache::isCacheable(element, style, parentStyle)) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the 
        // element context. This is fast and saves memory by reusing the style data structures.
        style.copyNonInheritedFrom(*cacheEntry->renderStyle);

        if (parentStyle.inheritedEqual(*cacheEntry->parentRenderStyle) && !isAtShadowBoundary(element)) {
            InsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            style.inheritFrom(*cacheEntry->renderStyle);

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            style.setInsideLink(linkStatus);
            return;
        }
        
        includedProperties = PropertyCascade::IncludedProperties::InheritedOnly;
    }

    if (elementTypeHasAppearanceFromUAStyle(element)) {
        // Find out if there's a -webkit-appearance property in effect from the UA sheet.
        // If so, we cache the border and background styles so that RenderTheme::adjustStyle()
        // can look at them later to figure out if this is a styled form control or not.
        auto userAgentStyle = RenderStyle::clonePtr(style);
        Builder builder(*userAgentStyle, builderContext(state), matchResult, { CascadeLevel::UserAgent });
        builder.applyAllProperties();

        state.setUserAgentAppearanceStyle(WTFMove(userAgentStyle));
    }

    Builder builder(*state.style(), builderContext(state), matchResult, allCascadeLevels(), includedProperties);

    // High priority properties may affect resolution of other properties (they are mostly font related).
    builder.applyHighPriorityProperties();

    if (cacheEntry && !cacheEntry->isUsableAfterHighPriorityProperties(style)) {
        // We need to resolve all properties without caching.
        applyMatchedProperties(state, matchResult, UseMatchedDeclarationsCache::No);
        return;
    }

    builder.applyLowPriorityProperties();

    for (auto& contentAttribute : builder.state().registeredContentAttributes())
        ruleSets().mutableFeatures().registerContentAttribute(contentAttribute);

    if (cacheEntry || !cacheHash)
        return;

    if (MatchedDeclarationsCache::isCacheable(element, style, parentStyle))
        m_matchedDeclarationsCache.add(style, parentStyle, cacheHash, matchResult);
}

bool Resolver::hasViewportDependentMediaQueries() const
{
    return m_ruleSets.hasViewportDependentMediaQueries();
}

Optional<DynamicMediaQueryEvaluationChanges> Resolver::evaluateDynamicMediaQueries()
{
    return m_ruleSets.evaluteDynamicMediaQueryRules(m_mediaQueryEvaluator);
}

} // namespace Style
} // namespace WebCore
