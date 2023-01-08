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
#include "CompositeOperation.h"
#include "ElementRuleCollector.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "Logging.h"
#include "MediaList.h"
#include "NodeRenderStyle.h"
#include "PageRuleCollector.h"
#include "Pair.h"
#include "RenderScrollbar.h"
#include "RenderStyleConstants.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGFontFaceElement.h"
#include "Settings.h"
#include "ShadowPseudoIds.h"
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
#include "VisitedLinkState.h"
#include "WebAnimationTypes.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {
namespace Style {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(Resolver);

class Resolver::State {
public:
    State() { }
    State(const Element& element, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle = nullptr)
        : m_element(&element)
        , m_parentStyle(parentStyle)
    {
        bool resetStyleInheritance = hasShadowRootParent(element) && downcast<ShadowRoot>(element.parentNode())->resetStyleInheritance();
        if (resetStyleInheritance)
            m_parentStyle = nullptr;

        auto& document = element.document();
        auto* documentElement = document.documentElement();
        if (!documentElement || documentElement == &element)
            m_rootElementStyle = document.initialContainingBlockStyle();
        else
            m_rootElementStyle = documentElementStyle ? documentElementStyle : documentElement->renderStyle();
    }

    const Element* element() const { return m_element; }

    void setStyle(std::unique_ptr<RenderStyle> style) { m_style = WTFMove(style); }
    RenderStyle* style() const { return m_style.get(); }
    std::unique_ptr<RenderStyle> takeStyle() { return WTFMove(m_style); }

    void setParentStyle(std::unique_ptr<RenderStyle> parentStyle)
    {
        m_ownedParentStyle = WTFMove(parentStyle);
        m_parentStyle = m_ownedParentStyle.get();
    }
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

Ref<Resolver> Resolver::create(Document& document)
{
    return adoptRef(*new Resolver(document));
}

Resolver::Resolver(Document& document)
    : m_ruleSets(*this)
    , m_document(document)
    , m_matchAuthorAndUserStyles(m_document.settings().authorAndUserStylesEnabled())
{
    UserAgentStyle::initDefaultStyleSheet();

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = m_document.view();
    if (view)
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { view->mediaType() };
    else
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { };

    if (auto* documentElement = m_document.documentElement()) {
        m_rootDefaultStyle = styleForElement(*documentElement, { m_document.initialContainingBlockStyle() }, RuleMatchingBehavior::MatchOnlyUserAgentRules).style;
        // Turn off assertion against font lookups during style resolver initialization. We may need root style font for media queries.
        m_document.fontSelector().incrementIsComputingRootStyleFont();
        m_rootDefaultStyle->fontCascade().update(&m_document.fontSelector());
        m_rootDefaultStyle->fontCascade().primaryFont();
        m_document.fontSelector().decrementIsComputingRootStyleFont();
    }

    if (m_rootDefaultStyle && view)
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { view->mediaType(), m_document, m_rootDefaultStyle.get() };

    m_ruleSets.resetAuthorStyle();
    m_ruleSets.resetUserAgentMediaQueryStyle();
}

void Resolver::addCurrentSVGFontFaceRules()
{
    if (m_document.svgExtensions()) {
        auto& svgFontFaceElements = m_document.svgExtensions()->svgFontFaceElements();
        for (auto& svgFontFaceElement : svgFontFaceElements)
            m_document.fontSelector().addFontFaceRule(svgFontFaceElement.fontFaceRule(), svgFontFaceElement.isInUserAgentShadowTree());
    }
}

void Resolver::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets)
{
    m_ruleSets.appendAuthorStyleSheets(styleSheets, &m_mediaQueryEvaluator, m_inspectorCSSOMWrappers);

    if (auto renderView = document().renderView())
        renderView->style().fontCascade().update(&document().fontSelector());
}

// This is a simplified style setting function for keyframe styles
void Resolver::addKeyframeStyle(Ref<StyleRuleKeyframes>&& rule)
{
    auto& animationName = rule->name();
    m_keyframesRuleMap.set(animationName, WTFMove(rule));
    m_document.keyframesRuleDidChange(animationName);
}

Resolver::~Resolver()
{
    RELEASE_ASSERT(!m_document.isResolvingTreeStyle());
}

static inline bool isAtShadowBoundary(const Element& element)
{
    return is<ShadowRoot>(element.parentNode());
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

ResolvedStyle Resolver::styleForElement(const Element& element, const ResolutionContext& context, RuleMatchingBehavior matchingBehavior)
{
    auto state = State(element, context.parentStyle, context.documentElementStyle);

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

    ElementRuleCollector collector(element, m_ruleSets, context.selectorMatchingState);
    collector.setMedium(m_mediaQueryEvaluator);

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

    Adjuster adjuster(document(), *state.parentStyle(), context.parentBoxStyle, &element);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    if (state.style()->usesViewportUnits())
        document().setHasStyleWithViewportUnits();

    return { state.takeStyle(), WTFMove(elementStyleRelations), collector.releaseMatchResult() };
}

std::unique_ptr<RenderStyle> Resolver::styleForKeyframe(const Element& element, const RenderStyle& elementStyle, const ResolutionContext& context, const StyleRuleKeyframe& keyframe, KeyframeValue& keyframeValue)
{
    // Add all the animating properties to the keyframe.
    bool hasRevert = false;
    for (auto propertyReference : keyframe.properties()) {
        auto unresolvedProperty = propertyReference.id();
        // The animation-composition and animation-timing-function within keyframes are special
        // because they are not animated; they just describe the composite operation and timing
        // function between this keyframe and the next.
        if (CSSProperty::isDirectionAwareProperty(unresolvedProperty))
            keyframeValue.setContainsDirectionAwareProperty(true);
        if (auto* value = propertyReference.value()) {
            auto resolvedProperty = CSSProperty::resolveDirectionAwareProperty(unresolvedProperty, elementStyle.direction(), elementStyle.writingMode());
            if (resolvedProperty != CSSPropertyAnimationTimingFunction && resolvedProperty != CSSPropertyAnimationComposition) {
                if (value->isCustomPropertyValue())
                    keyframeValue.addProperty(downcast<CSSCustomPropertyValue>(*value).name());
                else
                    keyframeValue.addProperty(resolvedProperty);
            }
            if (value->isRevertValue())
                hasRevert = true;
        }
    }

    auto state = State(element, nullptr, context.documentElementStyle);

    state.setStyle(RenderStyle::clonePtr(elementStyle));
    state.setParentStyle(RenderStyle::clonePtr(context.parentStyle ? *context.parentStyle : elementStyle));

    ElementRuleCollector collector(element, m_ruleSets, context.selectorMatchingState);
    collector.setPseudoElementRequest({ elementStyle.styleType() });
    if (hasRevert) {
        // In the animation origin, 'revert' rolls back the cascaded value to the user level.
        // Therefore, we need to collect UA and user rules.
        collector.setMedium(m_mediaQueryEvaluator);
        collector.matchUARules();
        collector.matchUserRules();
    }
    collector.addAuthorKeyframeRules(keyframe);
    Builder builder(*state.style(), builderContext(state), collector.matchResult(), CascadeLevel::Author);
    builder.state().setIsBuildingKeyframeStyle();
    builder.applyAllProperties();

    Adjuster adjuster(document(), *state.parentStyle(), nullptr, nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    return state.takeStyle();
}

bool Resolver::isAnimationNameValid(const String& name)
{
    return m_keyframesRuleMap.find(AtomString(name)) != m_keyframesRuleMap.end();
}

Vector<Ref<StyleRuleKeyframe>> Resolver::keyframeRulesForName(const AtomString& animationName) const
{
    if (animationName.isEmpty())
        return { };

    m_keyframesRuleMap.checkConsistency();

    auto it = m_keyframesRuleMap.find(animationName);
    if (it == m_keyframesRuleMap.end())
        return { };

    auto compositeOperationForKeyframe = [](Ref<StyleRuleKeyframe> keyframe) -> CompositeOperation {
        if (auto compositeOperationCSSValue = keyframe->properties().getPropertyCSSValue(CSSPropertyAnimationComposition)) {
            if (auto compositeOperation = toCompositeOperation(*compositeOperationCSSValue))
                return *compositeOperation;
        }
        return Animation::initialCompositeOperation();
    };

    auto timingFunctionForKeyframe = [](Ref<StyleRuleKeyframe> keyframe) -> RefPtr<const TimingFunction> {
        if (auto timingFunctionCSSValue = keyframe->properties().getPropertyCSSValue(CSSPropertyAnimationTimingFunction)) {
            if (auto timingFunction = TimingFunction::createFromCSSValue(*timingFunctionCSSValue))
                return timingFunction;
        }
        return &CubicBezierTimingFunction::defaultTimingFunction();
    };

    HashSet<RefPtr<const TimingFunction>> timingFunctions;
    auto uniqueTimingFunctionForKeyframe = [&](Ref<StyleRuleKeyframe> keyframe) -> RefPtr<const TimingFunction> {
        auto timingFunction = timingFunctionForKeyframe(keyframe);
        for (auto existingTimingFunction : timingFunctions) {
            if (arePointingToEqualData(timingFunction, existingTimingFunction))
                return existingTimingFunction;
        }
        timingFunctions.add(timingFunction);
        return timingFunction;
    };

    auto* keyframesRule = it->value.get();
    auto* keyframes = &keyframesRule->keyframes();

    using KeyframeUniqueKey = std::tuple<double, RefPtr<const TimingFunction>, CompositeOperation>;
    auto hasDuplicateKeys = [&]() -> bool {
        HashSet<KeyframeUniqueKey> uniqueKeyframeKeys;
        for (auto& keyframe : *keyframes) {
            auto compositeOperation = compositeOperationForKeyframe(keyframe);
            auto timingFunction = uniqueTimingFunctionForKeyframe(keyframe);
            for (auto key : keyframe->keys()) {
                if (!uniqueKeyframeKeys.add({ key, timingFunction, compositeOperation }))
                    return true;
            }
        }
        return false;
    }();

    if (!hasDuplicateKeys)
        return *keyframes;

    // FIXME: If HashMaps could have Ref<> as value types, we wouldn't need
    // to copy the HashMap into a Vector.
    // Merge keyframes with a similar offset and timing function.
    Vector<Ref<StyleRuleKeyframe>> deduplicatedKeyframes;
    HashMap<KeyframeUniqueKey, RefPtr<StyleRuleKeyframe>> keyframesMap;
    for (auto& originalKeyframe : *keyframes) {
        auto compositeOperation = compositeOperationForKeyframe(originalKeyframe);
        auto timingFunction = uniqueTimingFunctionForKeyframe(originalKeyframe);
        for (auto key : originalKeyframe->keys()) {
            KeyframeUniqueKey uniqueKey { key, timingFunction, compositeOperation };
            if (auto keyframe = keyframesMap.get(uniqueKey))
                keyframe->mutableProperties().mergeAndOverrideOnConflict(originalKeyframe->properties());
            else {
                auto styleRuleKeyframe = StyleRuleKeyframe::create(MutableStyleProperties::create());
                styleRuleKeyframe.ptr()->setKey(key);
                styleRuleKeyframe.ptr()->mutableProperties().mergeAndOverrideOnConflict(originalKeyframe->properties());
                keyframesMap.set(uniqueKey, styleRuleKeyframe.ptr());
                deduplicatedKeyframes.append(styleRuleKeyframe);
            }
        }
    }

    return deduplicatedKeyframes;
}

void Resolver::keyframeStylesForAnimation(const Element& element, const RenderStyle& elementStyle, const ResolutionContext& context, KeyframeList& list, bool& containsCSSVariableReferences)
{
    list.clear();

    auto keyframeRules = keyframeRulesForName(list.animationName());
    if (keyframeRules.isEmpty())
        return;

    containsCSSVariableReferences = false;
    // Construct and populate the style for each keyframe.
    for (auto& keyframeRule : keyframeRules) {
        // Add this keyframe style to all the indicated key times
        for (auto key : keyframeRule->keys()) {
            KeyframeValue keyframeValue(0, nullptr);
            keyframeValue.setStyle(styleForKeyframe(element, elementStyle, context, keyframeRule.get(), keyframeValue));
            keyframeValue.setKey(key);
            if (!containsCSSVariableReferences)
                containsCSSVariableReferences = keyframeRule->containsCSSVariableReferences();
            if (auto timingFunctionCSSValue = keyframeRule->properties().getPropertyCSSValue(CSSPropertyAnimationTimingFunction))
                keyframeValue.setTimingFunction(TimingFunction::createFromCSSValue(*timingFunctionCSSValue.get()));
            if (auto compositeOperationCSSValue = keyframeRule->properties().getPropertyCSSValue(CSSPropertyAnimationComposition)) {
                if (auto compositeOperation = toCompositeOperation(*compositeOperationCSSValue))
                    keyframeValue.setCompositeOperation(*compositeOperation);
            }
            list.insert(WTFMove(keyframeValue));
        }
    }
}

std::optional<ResolvedStyle> Resolver::styleForPseudoElement(const Element& element, const PseudoElementRequest& pseudoElementRequest, const ResolutionContext& context)
{
    auto state = State(element, context.parentStyle, context.documentElementStyle);

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtr());
        state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement(&element));
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    ElementRuleCollector collector(element, m_ruleSets, context.selectorMatchingState);
    collector.setPseudoElementRequest(pseudoElementRequest);
    collector.setMedium(m_mediaQueryEvaluator);
    collector.matchUARules();

    if (m_matchAuthorAndUserStyles) {
        collector.matchUserRules();
        collector.matchAuthorRules();
    }

    ASSERT(!collector.matchedPseudoElementIds());

    if (collector.matchResult().isEmpty())
        return { };

    state.style()->setStyleType(pseudoElementRequest.pseudoId);

    applyMatchedProperties(state, collector.matchResult());

    Adjuster adjuster(document(), *state.parentStyle(), context.parentBoxStyle, nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    if (state.style()->usesViewportUnits())
        document().setHasStyleWithViewportUnits();

    return ResolvedStyle { state.takeStyle(), nullptr, collector.releaseMatchResult() };
}

std::unique_ptr<RenderStyle> Resolver::styleForPage(int pageIndex)
{
    auto* documentElement = m_document.documentElement();
    if (!documentElement || !documentElement->renderStyle())
        return RenderStyle::createPtr();

    auto state = State(*documentElement, m_document.initialContainingBlockStyle());

    state.setStyle(RenderStyle::createPtr());
    state.style()->inheritFrom(*state.rootElementStyle());

    PageRuleCollector collector(m_ruleSets, documentElement->renderStyle()->direction());
    collector.matchAllPageRules(pageIndex);

    auto& result = collector.matchResult();

    Builder builder(*state.style(), builderContext(state), result, CascadeLevel::Author);
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
    collector.setMedium(m_mediaQueryEvaluator);
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
        || localName == HTMLNames::meterTag
        || (element.isInUserAgentShadowTree() && element.shadowPseudoId() == ShadowPseudoIds::webkitListButton());
}

void Resolver::invalidateMatchedDeclarationsCache()
{
    m_matchedDeclarationsCache.invalidate();
}

void Resolver::clearCachedDeclarationsAffectedByViewportUnits()
{
    m_matchedDeclarationsCache.clearEntriesAffectedByViewportUnits();
}

void Resolver::applyMatchedProperties(State& state, const MatchResult& matchResult)
{
    unsigned cacheHash = MatchedDeclarationsCache::computeHash(matchResult);
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

            if (cacheEntry->userAgentAppearanceStyle && elementTypeHasAppearanceFromUAStyle(element))
                state.setUserAgentAppearanceStyle(RenderStyle::clonePtr(*cacheEntry->userAgentAppearanceStyle));

            return;
        }

        includedProperties = PropertyCascade::IncludedProperties::InheritedOnly;
    }

    if (elementTypeHasAppearanceFromUAStyle(element)) {
        // Find out if there's a -webkit-appearance property in effect from the UA sheet.
        // If so, we cache the border and background styles so that RenderTheme::adjustStyle()
        // can look at them later to figure out if this is a styled form control or not.
        auto userAgentStyle = RenderStyle::clonePtr(style);
        Builder builder(*userAgentStyle, builderContext(state), matchResult, CascadeLevel::UserAgent);
        builder.applyAllProperties();

        state.setUserAgentAppearanceStyle(WTFMove(userAgentStyle));
    }

    Builder builder(*state.style(), builderContext(state), matchResult, CascadeLevel::Author, includedProperties);

    // Top priority properties may affect resolution of high priority ones.
    builder.applyTopPriorityProperties();

    // High priority properties may affect resolution of other properties (they are mostly font related).
    builder.applyHighPriorityProperties();

    if (cacheEntry && !cacheEntry->isUsableAfterHighPriorityProperties(style)) {
        // High-priority properties may affect resolution of other properties. Kick out the existing cache entry and try again.
        m_matchedDeclarationsCache.remove(cacheHash);
        applyMatchedProperties(state, matchResult);
        return;
    }

    builder.applyNonHighPriorityProperties();

    for (auto& contentAttribute : builder.state().registeredContentAttributes())
        ruleSets().mutableFeatures().registerContentAttribute(contentAttribute);

    if (cacheEntry || !cacheHash)
        return;

    if (MatchedDeclarationsCache::isCacheable(element, style, parentStyle))
        m_matchedDeclarationsCache.add(style, parentStyle, state.userAgentAppearanceStyle(), cacheHash, matchResult);
}

bool Resolver::hasViewportDependentMediaQueries() const
{
    return m_ruleSets.hasViewportDependentMediaQueries();
}

std::optional<DynamicMediaQueryEvaluationChanges> Resolver::evaluateDynamicMediaQueries()
{
    return m_ruleSets.evaluateDynamicMediaQueryRules(m_mediaQueryEvaluator);
}

} // namespace Style
} // namespace WebCore
