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

#include "BlendingKeyframes.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontSelector.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSViewTransitionRule.h"
#include "CachedResourceLoader.h"
#include "CompositeOperation.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementRuleCollector.h"
#include "FrameSelection.h"
#include "InspectorInstrumentation.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MediaList.h"
#include "NodeRenderStyle.h"
#include "PageRuleCollector.h"
#include "RenderScrollbar.h"
#include "RenderStyleConstants.h"
#include "RenderStyleInlines.h"
#include "RenderStyleSetters.h"
#include "RenderView.h"
#include "ResolvedStyle.h"
#include "RuleSet.h"
#include "RuleSetBuilder.h"
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
#include "UserAgentParts.h"
#include "UserAgentStyle.h"
#include "VisibilityAdjustment.h"
#include "VisitedLinkState.h"
#include "WebAnimationTypes.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {
namespace Style {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Resolver);

class Resolver::State {
public:
    State() = default;
    State(const Element& element, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle = nullptr)
        : m_element(&element)
        , m_parentStyle(parentStyle)
    {
        ASSERT(element.isConnected());

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
    const Element* m_element { };
    std::unique_ptr<RenderStyle> m_style;
    const RenderStyle* m_parentStyle { };
    std::unique_ptr<const RenderStyle> m_ownedParentStyle;
    const RenderStyle* m_rootElementStyle { };

    std::unique_ptr<RenderStyle> m_userAgentAppearanceStyle;
};

Ref<Resolver> Resolver::create(Document& document, ScopeType scopeType)
{
    return adoptRef(*new Resolver(document, scopeType));
}

Resolver::Resolver(Document& document, ScopeType scopeType)
    : m_document(document)
    , m_scopeType(scopeType)
    , m_ruleSets(*this)
    , m_matchedDeclarationsCache(*this)
    , m_matchAuthorAndUserStyles(settings().authorAndUserStylesEnabled())
{
    initialize();
}

void Resolver::initialize()
{
    UserAgentStyle::initDefaultStyleSheet();

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    auto* view = document().view();
    if (view)
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { view->mediaType() };
    else
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { };

    if (auto* documentElement = document().documentElement()) {
        m_rootDefaultStyle = styleForElement(*documentElement, { document().initialContainingBlockStyle() }, RuleMatchingBehavior::MatchOnlyUserAgentRules).style;
        // Turn off assertion against font lookups during style resolver initialization. We may need root style font for media queries.
        document().fontSelector().incrementIsComputingRootStyleFont();
        m_rootDefaultStyle->fontCascade().update(&document().fontSelector());
        m_rootDefaultStyle->fontCascade().primaryFont();
        document().protectedFontSelector()->decrementIsComputingRootStyleFont();
    }

    if (m_rootDefaultStyle && view)
        m_mediaQueryEvaluator = MQ::MediaQueryEvaluator { view->mediaType(), document(), m_rootDefaultStyle.get() };

    m_ruleSets.resetAuthorStyle();
    m_ruleSets.resetUserAgentMediaQueryStyle();
}

Resolver::~Resolver() = default;

Document& Resolver::document()
{
    return *m_document;
}

const Document& Resolver::document() const
{
    return *m_document;
}

const Settings& Resolver::settings() const
{
    return document().settings();
}

void Resolver::addCurrentSVGFontFaceRules()
{
    if (document().svgExtensionsIfExists()) {
        auto& svgFontFaceElements = document().svgExtensionsIfExists()->svgFontFaceElements();
        for (auto& svgFontFaceElement : svgFontFaceElements)
            document().fontSelector().addFontFaceRule(svgFontFaceElement.fontFaceRule(), svgFontFaceElement.isInUserAgentShadowTree());
    }
}

void Resolver::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets)
{
    m_ruleSets.appendAuthorStyleSheets(styleSheets, &m_mediaQueryEvaluator, m_inspectorCSSOMWrappers);

    if (auto renderView = document().renderView())
        renderView->style().fontCascade().update(&document().fontSelector());
}

KeyframesRuleMap& Resolver::userAgentKeyframes()
{
    static NeverDestroyed<KeyframesRuleMap> keyframes;
    return keyframes;
}

void Resolver::addUserAgentKeyframeStyle(Ref<StyleRuleKeyframes>&& rule)
{
    const auto& animationName = rule->name();
    userAgentKeyframes().set(animationName, WTFMove(rule));
}

// This is a simplified style setting function for keyframe styles
void Resolver::addKeyframeStyle(Ref<StyleRuleKeyframes>&& rule)
{
    const auto& animationName = rule->name();
    m_keyframesRuleMap.set(animationName, WTFMove(rule));
    document().keyframesRuleDidChange(animationName);
}

auto Resolver::initializeStateAndStyle(const Element& element, const ResolutionContext& context) -> State
{
    auto state = State { element, context.parentStyle, context.documentElementStyle };

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtrWithRegisteredInitialValues(document().customPropertyRegistry()));
        if (&element == document().documentElement() && !context.isSVGUseTreeRoot) {
            // Initial values for custom properties are inserted to the document element style. Don't overwrite them.
            state.style()->inheritIgnoringCustomPropertiesFrom(*state.parentStyle());
        } else
            state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement(&element));
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    if (element.isLink()) {
        auto& style = *state.style();
        style.setIsLink(true);
        InsideLink linkState = document().visitedLinkState().determineLinkState(element);
        if (linkState != InsideLink::NotInside) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClass::Visited);
            if (forceVisited)
                linkState = InsideLink::InsideVisited;
        }
        style.setInsideLink(linkState);
    }

    return state;
}

BuilderContext Resolver::builderContext(const State& state)
{
    return {
        document(),
        *state.parentStyle(),
        state.rootElementStyle(),
        state.element()
    };
}

ResolvedStyle Resolver::styleForElement(Element& element, const ResolutionContext& context, RuleMatchingBehavior matchingBehavior)
{
    auto state = initializeStateAndStyle(element, context);
    auto& style = *state.style();

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
    adjuster.adjust(style, state.userAgentAppearanceStyle());

    if (style.usesViewportUnits())
        document().setHasStyleWithViewportUnits();

    return { state.takeStyle(), WTFMove(elementStyleRelations), collector.releaseMatchResult() };
}

ResolvedStyle Resolver::styleForElementWithCachedMatchResult(Element& element, const ResolutionContext& context, const MatchResult& matchResult, const RenderStyle& existingRenderStyle)
{
    auto state = initializeStateAndStyle(element, context);
    auto& style = *state.style();

    style.copyPseudoElementBitsFrom(existingRenderStyle);
    copyRelations(style, existingRenderStyle);

    applyMatchedProperties(state, matchResult);

    Adjuster adjuster(document(), *state.parentStyle(), context.parentBoxStyle, &element);
    adjuster.adjust(style, state.userAgentAppearanceStyle());

    if (style.usesViewportUnits())
        document().setHasStyleWithViewportUnits();

    return { state.takeStyle(), { }, makeUnique<MatchResult>(matchResult) };
}

std::unique_ptr<RenderStyle> Resolver::styleForKeyframe(Element& element, const RenderStyle& elementStyle, const ResolutionContext& context, const StyleRuleKeyframe& keyframe, BlendingKeyframe& blendingKeyframe)
{
    // Add all the animating properties to the keyframe.
    bool hasRevert = false;
    for (auto propertyReference : keyframe.properties()) {
        auto unresolvedProperty = propertyReference.id();
        // The animation-composition and animation-timing-function within keyframes are special
        // because they are not animated; they just describe the composite operation and timing
        // function between this keyframe and the next.
        if (CSSProperty::isDirectionAwareProperty(unresolvedProperty))
            blendingKeyframe.setContainsDirectionAwareProperty(true);
        if (auto* value = propertyReference.value()) {
            auto resolvedProperty = CSSProperty::resolveDirectionAwareProperty(unresolvedProperty, elementStyle.writingMode());
            if (resolvedProperty != CSSPropertyAnimationTimingFunction && resolvedProperty != CSSPropertyAnimationComposition) {
                if (auto customValue = dynamicDowncast<CSSCustomPropertyValue>(*value))
                    blendingKeyframe.addProperty(customValue->name());
                else
                    blendingKeyframe.addProperty(resolvedProperty);
            }
            if (isValueID(*value, CSSValueRevert))
                hasRevert = true;
        }
    }

    auto state = State(element, nullptr, context.documentElementStyle);

    state.setStyle(RenderStyle::clonePtr(elementStyle));
    state.setParentStyle(RenderStyle::clonePtr(context.parentStyle ? *context.parentStyle : elementStyle));

    ElementRuleCollector collector(element, m_ruleSets, context.selectorMatchingState);

    if (elementStyle.pseudoElementType() != PseudoId::None)
        collector.setPseudoElementRequest(Style::PseudoElementIdentifier { elementStyle.pseudoElementType(), elementStyle.pseudoElementNameArgument() });

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

    Adjuster adjuster(document(), *state.parentStyle(), nullptr, elementStyle.pseudoElementType() == PseudoId::None ? &element : nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    return state.takeStyle();
}

bool Resolver::isAnimationNameValid(const String& name)
{
    return m_keyframesRuleMap.find(AtomString(name)) != m_keyframesRuleMap.end()
        || userAgentKeyframes().find(AtomString(name)) != userAgentKeyframes().end();
}

Vector<Ref<StyleRuleKeyframe>> Resolver::keyframeRulesForName(const AtomString& animationName) const
{
    if (animationName.isEmpty())
        return { };

    m_keyframesRuleMap.checkConsistency();

    // Check author map first then check user-agent map.
    auto it = m_keyframesRuleMap.find(animationName);
    if (it == m_keyframesRuleMap.end()) {
        it = userAgentKeyframes().find(animationName);
        if (it == userAgentKeyframes().end())
            return { };
    }

    auto compositeOperationForKeyframe = [](Ref<StyleRuleKeyframe> keyframe) -> CompositeOperation {
        if (auto compositeOperationCSSValue = keyframe->properties().getPropertyCSSValue(CSSPropertyAnimationComposition)) {
            if (auto compositeOperation = toCompositeOperation(*compositeOperationCSSValue))
                return *compositeOperation;
        }
        return Animation::initialCompositeOperation();
    };

    auto timingFunctionForKeyframe = [](Ref<StyleRuleKeyframe> keyframe) -> RefPtr<const TimingFunction> {
        if (auto timingFunctionCSSValue = keyframe->properties().getPropertyCSSValue(CSSPropertyAnimationTimingFunction)) {
            if (auto timingFunction = createTimingFunction(*timingFunctionCSSValue))
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
    // to copy the UncheckedKeyHashMap into a Vector.
    // Merge keyframes with a similar offset and timing function.
    Vector<Ref<StyleRuleKeyframe>> deduplicatedKeyframes;
    UncheckedKeyHashMap<KeyframeUniqueKey, RefPtr<StyleRuleKeyframe>> keyframesMap;
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

void Resolver::keyframeStylesForAnimation(Element& element, const RenderStyle& elementStyle, const ResolutionContext& context, BlendingKeyframes& list)
{
    list.clear();

    auto keyframeRules = keyframeRulesForName(list.animationName());
    if (keyframeRules.isEmpty())
        return;

    // Construct and populate the style for each keyframe.
    for (auto& keyframeRule : keyframeRules) {
        // Add this keyframe style to all the indicated key times
        for (auto key : keyframeRule->keys()) {
            BlendingKeyframe blendingKeyframe(0, nullptr);
            blendingKeyframe.setStyle(styleForKeyframe(element, elementStyle, context, keyframeRule.get(), blendingKeyframe));
            blendingKeyframe.setOffset(key);
            if (auto timingFunctionCSSValue = keyframeRule->properties().getPropertyCSSValue(CSSPropertyAnimationTimingFunction))
                blendingKeyframe.setTimingFunction(createTimingFunction(*timingFunctionCSSValue));
            if (auto compositeOperationCSSValue = keyframeRule->properties().getPropertyCSSValue(CSSPropertyAnimationComposition)) {
                if (auto compositeOperation = toCompositeOperation(*compositeOperationCSSValue))
                    blendingKeyframe.setCompositeOperation(*compositeOperation);
            }
            list.insert(WTFMove(blendingKeyframe));
            list.updatePropertiesMetadata(keyframeRule->properties());
        }
    }
}

std::optional<ResolvedStyle> Resolver::styleForPseudoElement(Element& element, const PseudoElementRequest& pseudoElementRequest, const ResolutionContext& context)
{
    auto state = State(element, context.parentStyle, context.documentElementStyle);

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::createPtrWithRegisteredInitialValues(document().customPropertyRegistry()));
        state.style()->inheritFrom(*state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement(&element));
        state.setParentStyle(RenderStyle::clonePtr(*state.style()));
    }

    ElementRuleCollector collector(element, m_ruleSets, context.selectorMatchingState);
    if (pseudoElementRequest.pseudoId() != PseudoId::None)
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

    state.style()->setPseudoElementType(pseudoElementRequest.pseudoId());
    if (!pseudoElementRequest.nameArgument().isNull())
        state.style()->setPseudoElementNameArgument(pseudoElementRequest.nameArgument());

    applyMatchedProperties(state, collector.matchResult());

    Adjuster adjuster(document(), *state.parentStyle(), context.parentBoxStyle, nullptr);
    adjuster.adjust(*state.style(), state.userAgentAppearanceStyle());

    Adjuster::adjustVisibilityForPseudoElement(*state.style(), element);

    if (state.style()->usesViewportUnits())
        document().setHasStyleWithViewportUnits();

    return ResolvedStyle { state.takeStyle(), nullptr, collector.releaseMatchResult() };
}

std::unique_ptr<RenderStyle> Resolver::styleForPage(int pageIndex)
{
    auto* documentElement = document().documentElement();
    if (!documentElement || !documentElement->renderStyle())
        return RenderStyle::createPtr();

    auto state = State(*documentElement, document().initialContainingBlockStyle());

    state.setStyle(RenderStyle::createPtr());
    state.style()->inheritFrom(*state.rootElementStyle());

    PageRuleCollector collector(m_ruleSets, documentElement->renderStyle()->writingMode());
    collector.matchAllPageRules(pageIndex);

    auto& result = collector.matchResult();

    Builder builder(*state.style(), builderContext(state), result, CascadeLevel::Author);
    builder.applyAllProperties();

    // Now return the style.
    return state.takeStyle();
}

std::unique_ptr<RenderStyle> Resolver::defaultStyleForElement(const Element* element)
{
    auto style = RenderStyle::createPtrWithRegisteredInitialValues(document().customPropertyRegistry());

    FontCascadeDescription fontDescription;
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
    return pseudoStyleRulesForElement(element, { }, rulesToInclude);
}

Vector<RefPtr<const StyleRule>> Resolver::pseudoStyleRulesForElement(const Element* element, const std::optional<Style::PseudoElementRequest>& pseudoElementIdentifier, unsigned rulesToInclude)
{
    if (!element)
        return { };

    auto state = State(*element, nullptr);

    ElementRuleCollector collector(*element, m_ruleSets, nullptr);
    collector.setMode(SelectorChecker::Mode::CollectingRules);
    if (pseudoElementIdentifier)
        collector.setPseudoElementRequest(*pseudoElementIdentifier);
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
        || (element.isInUserAgentShadowTree() && element.userAgentPart() == UserAgentParts::webkitListButton());
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
    auto& style = *state.style();
    auto& parentStyle = *state.parentStyle();
    auto& element = *state.element();

    unsigned cacheHash = MatchedDeclarationsCache::computeHash(matchResult, parentStyle.inheritedCustomProperties());
    auto includedProperties = PropertyCascade::normalProperties();

    auto* cacheEntry = m_matchedDeclarationsCache.find(cacheHash, matchResult, parentStyle.inheritedCustomProperties());

    auto hasUsableEntry = cacheEntry && MatchedDeclarationsCache::isCacheable(element, style, parentStyle);
    if (hasUsableEntry) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        style.copyNonInheritedFrom(*cacheEntry->renderStyle);

        bool hasExplicitlyInherited = cacheEntry->renderStyle->hasExplicitlyInheritedProperties();
        bool inheritedStyleEqual = parentStyle.inheritedEqual(*cacheEntry->parentRenderStyle);

        if (inheritedStyleEqual) {
            InsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            style.inheritFrom(*cacheEntry->renderStyle);

            // Link status is treated like an inherited property. We need to explicitly restore it.
            style.setInsideLink(linkStatus);

            if (!hasExplicitlyInherited && matchResult.nonCacheablePropertyIds.isEmpty()) {
                if (cacheEntry->userAgentAppearanceStyle && elementTypeHasAppearanceFromUAStyle(element))
                    state.setUserAgentAppearanceStyle(RenderStyle::clonePtr(*cacheEntry->userAgentAppearanceStyle));

                return;
            }
        }

        includedProperties = { };

        if (!inheritedStyleEqual) {
            includedProperties.add(PropertyCascade::PropertyType::Inherited);
            // FIXME: See colorFromPrimitiveValueWithResolvedCurrentColor().
            bool mayContainResolvedCurrentcolor = style.disallowsFastPathInheritance() && hasExplicitlyInherited;
            if (mayContainResolvedCurrentcolor && parentStyle.color() != cacheEntry->parentRenderStyle->color())
                includedProperties.add(PropertyCascade::PropertyType::NonInherited);
        }
        if (hasExplicitlyInherited)
            includedProperties.add(PropertyCascade::PropertyType::ExplicitlyInherited);
        if (!matchResult.nonCacheablePropertyIds.isEmpty())
            includedProperties.add(PropertyCascade::PropertyType::NonCacheable);
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

bool Resolver::hasSelectorForAttribute(const Element& element, const AtomString& attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    if (element.isHTMLElement() && element.document().isHTMLDocument())
        return m_ruleSets.features().attributeLowercaseLocalNamesInRules.contains(attributeName);
    return m_ruleSets.features().attributeLocalNamesInRules.contains(attributeName);
}

bool Resolver::hasSelectorForId(const AtomString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_ruleSets.features().idsInRules.contains(idValue);
}

bool Resolver::hasViewportDependentMediaQueries() const
{
    return m_ruleSets.hasViewportDependentMediaQueries();
}

std::optional<DynamicMediaQueryEvaluationChanges> Resolver::evaluateDynamicMediaQueries()
{
    return m_ruleSets.evaluateDynamicMediaQueryRules(m_mediaQueryEvaluator);
}


static CSSSelectorList viewTransitionSelector(CSSSelector::PseudoElement element, const AtomString& name)
{
    MutableCSSSelectorList selectorList;

    auto rootSelector = makeUnique<MutableCSSSelector>();
    rootSelector->setMatch(CSSSelector::Match::PseudoClass);
    rootSelector->setPseudoClass(CSSSelector::PseudoClass::Root);
    selectorList.append(WTFMove(rootSelector));

    auto groupSelector = makeUnique<MutableCSSSelector>();
    groupSelector->setMatch(CSSSelector::Match::PseudoElement);
    groupSelector->setPseudoElement(element);

    AtomString selectorName;
    switch (element) {
    case CSSSelector::PseudoElement::ViewTransitionGroup:
        selectorName = "view-transition-group"_s;
        break;
    case CSSSelector::PseudoElement::ViewTransitionImagePair:
        selectorName = "view-transition-image-pair"_s;
        break;
    case CSSSelector::PseudoElement::ViewTransitionNew:
        selectorName = "view-transition-new"_s;
        break;
    case CSSSelector::PseudoElement::ViewTransitionOld:
        selectorName = "view-transition-old"_s;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    groupSelector->setValue(selectorName);
    groupSelector->setArgumentList({ { name } });

    selectorList.first()->appendTagHistory(CSSSelector::Relation::Subselector, WTFMove(groupSelector));

    return CSSSelectorList(WTFMove(selectorList));
}

RefPtr<StyleRuleViewTransition> Resolver::viewTransitionRule() const
{
    return m_ruleSets.viewTransitionRule();
}

void Resolver::setViewTransitionStyles(CSSSelector::PseudoElement element, const AtomString& name, Ref<MutableStyleProperties> properties)
{
    if (!m_document)
        return;

    auto styleRule = StyleRule::create(WTFMove(properties), true, viewTransitionSelector(element, name));

    auto* viewTransitionsStyle = m_ruleSets.dynamicViewTransitionsStyle();
    RuleSetBuilder builder(*viewTransitionsStyle, mediaQueryEvaluator(), this);
    builder.addStyleRule(styleRule);
}

} // namespace Style
} // namespace WebCore
