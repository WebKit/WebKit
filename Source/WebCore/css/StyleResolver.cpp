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

#include "CSSDefaultStyleSheets.h"
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
#include "Pair.h"
#include "Quirks.h"
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
#include "SVGURIReference.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SharedStringHash.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolveForDocument.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "UserAgentStyleSheets.h"
#include "ViewportStyleResolver.h"
#include "VisitedLinkState.h"
#include "WebKitFontFamilyNames.h"
#include <bitset>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

using namespace HTMLNames;

inline void StyleResolver::State::clear()
{
    m_element = nullptr;
    m_parentStyle = nullptr;
    m_ownedParentStyle = nullptr;
    m_userAgentAppearanceStyle = nullptr;
}

StyleResolver::StyleResolver(Document& document)
    : m_ruleSets(*this)
    , m_document(document)
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
#endif
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
    AtomString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), WTFMove(rule));
}

StyleResolver::~StyleResolver()
{
    RELEASE_ASSERT(!m_document.isResolvingTreeStyle());
    RELEASE_ASSERT(!m_isDeleted);
    m_isDeleted = true;

#if ENABLE(CSS_DEVICE_ADAPTATION)
    m_viewportStyleResolver->clearDocument();
#endif
}

StyleResolver::State::State(const Element& element, const RenderStyle* parentStyle, const RenderStyle* documentElementStyle, const SelectorFilter* selectorFilter)
    : m_element(&element)
    , m_parentStyle(parentStyle)
    , m_selectorFilter(selectorFilter)
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

inline void StyleResolver::State::setStyle(std::unique_ptr<RenderStyle> style)
{
    m_style = WTFMove(style);
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
        InsideLink linkState = document().visitedLinkState().determineLinkState(element);
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

    applyMatchedProperties(collector.matchResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *state.parentStyle(), parentBoxStyle, &element, state.userAgentAppearanceStyle());

    if (state.style()->hasViewportUnits())
        document().setHasStyleWithViewportUnits();

    state.clear(); // Clear out for the next resolve.

    return { state.takeStyle(), WTFMove(elementStyleRelations) };
}

std::unique_ptr<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleRuleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    RELEASE_ASSERT(!m_isDeleted);

    MatchResult result;
    result.authorDeclarations.append({ &keyframe->properties() });

    ASSERT(!m_state.style());

    State& state = m_state;

    // Create the style
    state.setStyle(RenderStyle::clonePtr(*elementStyle));
    state.setParentStyle(RenderStyle::clonePtr(*elementStyle));

    Style::Builder builder(*this, result, { Style::CascadeLevel::Author });
    builder.applyAllProperties();

    adjustRenderStyle(*state.style(), *state.parentStyle(), nullptr, nullptr, state.userAgentAppearanceStyle());

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
    return m_keyframesRuleMap.find(AtomString(name).impl()) != m_keyframesRuleMap.end();
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

std::unique_ptr<RenderStyle> StyleResolver::pseudoStyleForElement(const Element& element, const PseudoStyleRequest& pseudoStyleRequest, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, const SelectorFilter* selectorFilter)
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
        collector.matchUserRules();
        collector.matchAuthorRules();
    }

    ASSERT(!collector.matchedPseudoElementIds());

    if (collector.matchResult().isEmpty())
        return nullptr;

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    applyMatchedProperties(collector.matchResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *m_state.parentStyle(), parentBoxStyle, nullptr, state.userAgentAppearanceStyle());

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

    auto& result = collector.matchResult();

    Style::Builder builder(*this, result, { Style::CascadeLevel::Author });
    builder.applyAllProperties();

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
    case DisplayType::FlowRoot:
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

#if ENABLE(OVERFLOW_SCROLLING_TOUCH) || ENABLE(POINTER_EVENTS)
static bool isScrollableOverflow(Overflow overflow)
{
    return overflow == Overflow::Scroll || overflow == Overflow::Auto;
}
#endif

static bool hasEffectiveDisplayNoneForDisplayContents(const Element& element)
{
    // https://drafts.csswg.org/css-display-3/#unbox-html
    static NeverDestroyed<HashSet<AtomString>> tagNames = [] {
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
        HashSet<AtomString> set;
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

#if ENABLE(POINTER_EVENTS)
static OptionSet<TouchAction> computeEffectiveTouchActions(const RenderStyle& style, OptionSet<TouchAction> effectiveTouchActions)
{
    // https://w3c.github.io/pointerevents/#determining-supported-touch-behavior
    // "A touch behavior is supported if it conforms to the touch-action property of each element between
    // the hit tested element and its nearest ancestor with the default touch behavior (including both the
    // hit tested element and the element with the default touch behavior)."

    bool hasDefaultTouchBehavior = isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY());
    if (hasDefaultTouchBehavior)
        effectiveTouchActions = RenderStyle::initialTouchActions();

    auto touchActions = style.touchActions();
    if (touchActions == RenderStyle::initialTouchActions())
        return effectiveTouchActions;

    if (effectiveTouchActions.contains(TouchAction::None))
        return { TouchAction::None };

    if (effectiveTouchActions.containsAny({ TouchAction::Auto, TouchAction::Manipulation }))
        return touchActions;

    if (touchActions.containsAny({ TouchAction::Auto, TouchAction::Manipulation }))
        return effectiveTouchActions;

    auto sharedTouchActions = effectiveTouchActions & touchActions;
    if (sharedTouchActions.isEmpty())
        return { TouchAction::None };

    return sharedTouchActions;
}
#endif

#if ENABLE(TEXT_AUTOSIZING)
static bool hasTextChild(const Element& element)
{
    for (auto* child = element.firstChild(); child; child = child->nextSibling()) {
        if (is<Text>(child))
            return true;
    }
    return false;
}

bool StyleResolver::adjustRenderStyleForTextAutosizing(RenderStyle& style, const Element& element)
{
    if (!settings().textAutosizingEnabled() || !settings().textAutosizingUsesIdempotentMode())
        return false;

    AutosizeStatus::updateStatus(style);
    if (style.textSizeAdjust().isNone())
        return false;

    float initialScale = document().page() ? document().page()->initialScale() : 1;
    auto adjustLineHeightIfNeeded = [&](auto computedFontSize) {
        auto lineHeight = style.specifiedLineHeight();
        constexpr static unsigned eligibleFontSize = 12;
        if (computedFontSize * initialScale >= eligibleFontSize)
            return;

        constexpr static float boostFactor = 1.25;
        auto minimumLineHeight = boostFactor * computedFontSize;
        if (!lineHeight.isFixed() || lineHeight.value() >= minimumLineHeight)
            return;

        if (AutosizeStatus::probablyContainsASmallFixedNumberOfLines(style))
            return;

        style.setLineHeight({ minimumLineHeight, Fixed });
    };

    auto fontDescription = style.fontDescription();
    auto initialComputedFontSize = fontDescription.computedSize();
    auto specifiedFontSize = fontDescription.specifiedSize();
    bool isCandidate = style.isIdempotentTextAutosizingCandidate();
    if (!isCandidate && WTF::areEssentiallyEqual(initialComputedFontSize, specifiedFontSize))
        return false;

    auto adjustedFontSize = AutosizeStatus::idempotentTextSize(fontDescription.specifiedSize(), initialScale);
    if (isCandidate && WTF::areEssentiallyEqual(initialComputedFontSize, adjustedFontSize))
        return false;

    if (!hasTextChild(element))
        return false;

    fontDescription.setComputedSize(isCandidate ? adjustedFontSize : specifiedFontSize);
    style.setFontDescription(WTFMove(fontDescription));
    style.fontCascade().update(&document().fontSelector());

    // FIXME: We should restore computed line height to its original value in the case where the element is not
    // an idempotent text autosizing candidate; otherwise, if an element that is a text autosizing candidate contains
    // children which are not autosized, the non-autosized content will end up with a boosted line height.
    if (isCandidate)
        adjustLineHeightIfNeeded(adjustedFontSize);

    return true;
}
#endif

void StyleResolver::adjustRenderStyle(RenderStyle& style, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, const Element* element, const RenderStyle* userAgentAppearanceStyle)
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

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
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
        RenderTheme::singleton().adjustStyle(*this, style, element, userAgentAppearanceStyle);

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

#if ENABLE(POINTER_EVENTS)
    style.setEffectiveTouchActions(computeEffectiveTouchActions(style, parentStyle.effectiveTouchActions()));
#endif

    if (element) {
#if ENABLE(TEXT_AUTOSIZING)
        adjustRenderStyleForTextAutosizing(style, *element);
#endif
        adjustRenderStyleForSiteSpecificQuirks(style, *element);
    }
}

void StyleResolver::adjustRenderStyleForSiteSpecificQuirks(RenderStyle& style, const Element& element)
{
    if (document().quirks().needsGMailOverflowScrollQuirk()) {
        // This turns sidebar scrollable without mouse move event.
        static NeverDestroyed<AtomString> roleValue("navigation", AtomString::ConstructFromLiteral);
        if (style.overflowY() == Overflow::Hidden && element.attributeWithoutSynchronization(roleAttr) == roleValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (document().quirks().needsYouTubeOverflowScrollQuirk()) {
        // This turns sidebar scrollable without hover.
        static NeverDestroyed<AtomString> idValue("guide-inner-content", AtomString::ConstructFromLiteral);
        if (style.overflowY() == Overflow::Hidden && element.idForStyleResolution() == idValue)
            style.setOverflowY(Overflow::Auto);
    }
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

void StyleResolver::invalidateMatchedDeclarationsCache()
{
    m_matchedDeclarationsCache.invalidate();
}

void StyleResolver::clearCachedDeclarationsAffectedByViewportUnits()
{
    m_matchedDeclarationsCache.clearEntriesAffectedByViewportUnits();
}

void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, const Element& element, UseMatchedDeclarationsCache useMatchedDeclarationsCache)
{
    State& state = m_state;
    unsigned cacheHash = useMatchedDeclarationsCache == UseMatchedDeclarationsCache::Yes ? Style::MatchedDeclarationsCache::computeHash(matchResult) : 0;
    auto includedProperties = Style::PropertyCascade::IncludedProperties::All;

    auto& style = *state.style();
    auto& parentStyle = *state.parentStyle();

    auto* cacheEntry = m_matchedDeclarationsCache.find(cacheHash, matchResult);
    if (cacheEntry && Style::MatchedDeclarationsCache::isCacheable(element, style, parentStyle)) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the 
        // element context. This is fast and saves memory by reusing the style data structures.
        style.copyNonInheritedFrom(*cacheEntry->renderStyle);
        if (parentStyle.inheritedDataShared(cacheEntry->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
            InsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            style.inheritFrom(*cacheEntry->renderStyle);

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            style.setInsideLink(linkStatus);
            return;
        }
        includedProperties = Style::PropertyCascade::IncludedProperties::InheritedOnly;
    }

    if (elementTypeHasAppearanceFromUAStyle(element)) {
        // Find out if there's a -webkit-appearance property in effect from the UA sheet.
        // If so, we cache the border and background styles so that RenderTheme::adjustStyle()
        // can look at them later to figure out if this is a styled form control or not.
        auto userAgentStyle = RenderStyle::clonePtr(style);
        Style::Builder builder(*userAgentStyle, *this, matchResult, { Style::CascadeLevel::UserAgent });
        builder.applyAllProperties();

        state.setUserAgentAppearanceStyle(WTFMove(userAgentStyle));
    }

    Style::Builder builder(*this, matchResult, Style::allCascadeLevels(), includedProperties);

    // High priority properties may affect resolution of other properties (they are mostly font related).
    builder.applyHighPriorityProperties();

    // If the effective zoom value changes, we can't use the matched properties cache. Start over.
    if (cacheEntry && cacheEntry->renderStyle->effectiveZoom() != style.effectiveZoom())
        return applyMatchedProperties(matchResult, element, UseMatchedDeclarationsCache::No);

    // If the font changed, we can't use the matched properties cache. Start over.
    if (cacheEntry && cacheEntry->renderStyle->fontDescription() != style.fontDescription())
        return applyMatchedProperties(matchResult, element, UseMatchedDeclarationsCache::No);

    builder.applyLowPriorityProperties();

    for (auto& contentAttribute : builder.state().registeredContentAttributes())
        ruleSets().mutableFeatures().registerContentAttribute(contentAttribute);

    if (cacheEntry || !cacheHash)
        return;

    if (Style::MatchedDeclarationsCache::isCacheable(element, style, parentStyle))
        m_matchedDeclarationsCache.add(style, parentStyle, cacheHash, matchResult);
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
    if (!value)
        return;

    MatchResult matchResult;
    Style::Builder builder(*this, matchResult, { });
    builder.applyPropertyValue(id, *value);
}

void StyleResolver::initializeFontStyle()
{
    FontCascadeDescription fontDescription;
    fontDescription.setRenderingMode(settings().fontRenderingMode());
    fontDescription.setOneFamily(standardFamily);
    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);

    auto size = Style::fontSizeForKeyword(CSSValueMedium, false, document());
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), is<SVGElement>(m_state.element()), m_state.style(), document()));

    fontDescription.setShouldAllowUserInstalledFonts(settings().shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No);
    style()->setFontDescription(WTFMove(fontDescription));
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

} // namespace WebCore
