/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012-2020 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2020, 2022 Igalia S.L.
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
#include "StyleAdjuster.h"

#include "CSSFontSelector.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementAncestorIterator.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLBodyElement.h"
#include "HTMLDialogElement.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLMarqueeElement.h"
#include "HTMLNames.h"
#include "HTMLSlotElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "LocalDOMWindow.h"
#include "LocalFrameView.h"
#include "MathMLElement.h"
#include "NodeName.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleSelfAlignmentData.h"
#include "StyleUpdate.h"
#include "Styleable.h"
#include "Text.h"
#include "TouchAction.h"
#include "TypedElementDescendantIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibilityAdjustment.h"
#include "WebAnimationTypes.h"
#include <wtf/RobinHoodHashSet.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if ENABLE(FULLSCREEN_API)
#include "FullscreenManager.h"
#endif

namespace WebCore {
namespace Style {

using namespace HTMLNames;

Adjuster::Adjuster(const Document& document, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, Element* element)
    : m_document(document)
    , m_parentStyle(parentStyle)
    , m_parentBoxStyle(parentBoxStyle ? *parentBoxStyle : m_parentStyle)
    , m_element(element)
{
}

#if PLATFORM(COCOA)
static void addIntrinsicMargins(RenderStyle& style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = clampToInteger(2 * style.usedZoom());

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "hasQuirk" to decide the margin wasn't set is kind of lame.
    if (style.width().isIntrinsicOrAuto()) {
        if (style.marginLeft().hasQuirk())
            style.setMarginLeft(WebCore::Length(intrinsicMargin, LengthType::Fixed));
        if (style.marginRight().hasQuirk())
            style.setMarginRight(WebCore::Length(intrinsicMargin, LengthType::Fixed));
    }

    if (style.height().isAuto()) {
        if (style.marginTop().hasQuirk())
            style.setMarginTop(WebCore::Length(intrinsicMargin, LengthType::Fixed));
        if (style.marginBottom().hasQuirk())
            style.setMarginBottom(WebCore::Length(intrinsicMargin, LengthType::Fixed));
    }
}
#endif

// https://www.w3.org/TR/css-display-3/#transformations
static DisplayType equivalentBlockDisplay(const RenderStyle& style)
{
    switch (auto display = style.display()) {
    case DisplayType::Block:
    case DisplayType::Table:
    case DisplayType::Box:
    case DisplayType::Flex:
    case DisplayType::Grid:
    case DisplayType::FlowRoot:
    case DisplayType::ListItem:
    case DisplayType::RubyBlock:
        return display;
    case DisplayType::InlineTable:
        return DisplayType::Table;
    case DisplayType::InlineBox:
        return DisplayType::Box;
    case DisplayType::InlineFlex:
        return DisplayType::Flex;
    case DisplayType::InlineGrid:
        return DisplayType::Grid;
    case DisplayType::Ruby:
        return DisplayType::RubyBlock;

    case DisplayType::Inline:
    case DisplayType::InlineBlock:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
    case DisplayType::RubyBase:
    case DisplayType::RubyAnnotation:
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

// https://www.w3.org/TR/css-display-3/#transformations
static DisplayType equivalentInlineDisplay(const RenderStyle& style)
{
    switch (auto display = style.display()) {
    case DisplayType::Block:
        return DisplayType::InlineBlock;
    case DisplayType::Table:
        return DisplayType::InlineTable;
    case DisplayType::Box:
        return DisplayType::InlineBox;
    case DisplayType::Flex:
        return DisplayType::InlineFlex;
    case DisplayType::Grid:
        return DisplayType::InlineGrid;
    case DisplayType::RubyBlock:
        return DisplayType::Ruby;

    case DisplayType::Inline:
    case DisplayType::InlineBlock:
    case DisplayType::InlineTable:
    case DisplayType::InlineBox:
    case DisplayType::InlineFlex:
    case DisplayType::InlineGrid:
    case DisplayType::Ruby:
    case DisplayType::RubyBase:
    case DisplayType::RubyAnnotation:
        return display;

    case DisplayType::FlowRoot:
    case DisplayType::ListItem:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
        return DisplayType::Inline;

    case DisplayType::Contents:
        ASSERT_NOT_REACHED();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED();
        return DisplayType::None;
    }
    ASSERT_NOT_REACHED();
    return DisplayType::Inline;
}

static bool shouldInheritTextDecorationsInEffect(const RenderStyle& style, const Element* element)
{
    if (style.isFloating() || style.hasOutOfFlowPosition())
        return false;

    // Media elements have a special rendering where the media controls do not use a proper containing
    // block model which means we need to manually stop text-decorations to apply to text inside media controls.
    auto isAtMediaUAShadowBoundary = [&] {
#if ENABLE(VIDEO)
        if (!element)
            return false;
        auto* parentNode = element->parentNode();
        return parentNode && parentNode->isUserAgentShadowRoot() && parentNode->parentOrShadowHostElement()->isMediaElement();
#else
        return false;
#endif
    }();

    // Outermost <svg> roots are considered to be atomic inline-level.
    if (RefPtr svgElement = dynamicDowncast<SVGElement>(element); svgElement && svgElement->isOutermostSVGSVGElement())
        return false;

    // There is no other good way to prevent decorations from affecting user agent shadow trees.
    if (isAtMediaUAShadowBoundary)
        return false;

    switch (style.display()) {
    case DisplayType::InlineTable:
    case DisplayType::InlineBlock:
    case DisplayType::InlineGrid:
    case DisplayType::InlineFlex:
    case DisplayType::InlineBox:
        return false;
    default:
        break;
    };

    return true;
}

static bool isScrollableOverflow(Overflow overflow)
{
    return overflow == Overflow::Scroll || overflow == Overflow::Auto;
}

static OptionSet<TouchAction> computeUsedTouchActions(const RenderStyle& style, OptionSet<TouchAction> usedTouchActions)
{
    // https://w3c.github.io/pointerevents/#determining-supported-touch-behavior
    // "A touch behavior is supported if it conforms to the touch-action property of each element between
    // the hit tested element and its nearest ancestor with the default touch behavior (including both the
    // hit tested element and the element with the default touch behavior)."

    bool hasDefaultTouchBehavior = isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY());
    if (hasDefaultTouchBehavior)
        usedTouchActions = RenderStyle::initialTouchActions();

    auto touchActions = style.touchActions();
    if (touchActions == RenderStyle::initialTouchActions())
        return usedTouchActions;

    if (usedTouchActions.contains(TouchAction::None))
        return { TouchAction::None };

    if (usedTouchActions.containsAny({ TouchAction::Auto, TouchAction::Manipulation }))
        return touchActions;

    if (touchActions.containsAny({ TouchAction::Auto, TouchAction::Manipulation }))
        return usedTouchActions;

    auto sharedTouchActions = usedTouchActions & touchActions;
    if (sharedTouchActions.isEmpty())
        return { TouchAction::None };

    return sharedTouchActions;
}

bool Adjuster::adjustEventListenerRegionTypesForRootStyle(RenderStyle& rootStyle, const Document& document)
{
    auto regionTypes = computeEventListenerRegionTypes(document, rootStyle, document, { });
    if (RefPtr window = document.domWindow())
        regionTypes.add(computeEventListenerRegionTypes(document, rootStyle, *window, { }));

    bool changed = regionTypes != rootStyle.eventListenerRegionTypes();
    rootStyle.setEventListenerRegionTypes(regionTypes);
    return changed;
}

OptionSet<EventListenerRegionType> Adjuster::computeEventListenerRegionTypes(const Document& document, const RenderStyle& style, const EventTarget& eventTarget, OptionSet<EventListenerRegionType> parentTypes)
{
    auto types = parentTypes;

#if ENABLE(WHEEL_EVENT_REGIONS)
    auto findListeners = [&](auto& eventName, auto type, auto nonPassiveType) {
        auto* eventListenerVector = eventTarget.eventTargetData()->eventListenerMap.find(eventName);
        if (!eventListenerVector)
            return;

        types.add(type);

        auto isPassiveOnly = [&] {
            for (auto& listener : *eventListenerVector) {
                if (!listener->isPassive())
                    return false;
            }
            return true;
        }();

        if (!isPassiveOnly)
            types.add(nonPassiveType);
    };

    if (eventTarget.hasEventListeners()) {
        findListeners(eventNames().wheelEvent, EventListenerRegionType::Wheel, EventListenerRegionType::NonPassiveWheel);
        findListeners(eventNames().mousewheelEvent, EventListenerRegionType::Wheel, EventListenerRegionType::NonPassiveWheel);
    }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (document.page() && document.page()->shouldBuildInteractionRegions()) {
        if (const auto* node = dynamicDowncast<Node>(eventTarget)) {
            if (node->willRespondToMouseClickEventsWithEditability(node->computeEditabilityForMouseClickEvents(&style)))
                types.add(EventListenerRegionType::MouseClick);
        }
    }
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(style);
#endif

#if !ENABLE(WHEEL_EVENT_REGIONS) && !ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    UNUSED_PARAM(eventTarget);
#endif

    return types;
}

static bool isOverflowClipOrVisible(Overflow overflow)
{
    return overflow == Overflow::Clip || overflow == Overflow::Visible;
}

static bool shouldInlinifyForRuby(const RenderStyle& style, const RenderStyle& parentBoxStyle)
{
    auto parentDisplay = parentBoxStyle.display();
    auto hasRubyParent = parentDisplay == DisplayType::Ruby
        || parentDisplay == DisplayType::RubyBlock
        || parentDisplay == DisplayType::RubyAnnotation
        || parentDisplay == DisplayType::RubyBase;

    return hasRubyParent && !style.hasOutOfFlowPosition() && !style.isFloating();
}

static bool hasUnsupportedRubyDisplay(DisplayType display, const Element* element)
{
    // Only allow ruby elements to have ruby display types for now.
    switch (display) {
    case DisplayType::Ruby:
    case DisplayType::RubyBlock:
        // Test for localName so this also allows WebVTT ruby elements.
        return !element || !element->hasLocalName(rubyTag->localName());
    case DisplayType::RubyAnnotation:
        return !element || !element->hasLocalName(rtTag->localName());
    case DisplayType::RubyBase:
        ASSERT_NOT_REACHED();
        return false;
    default:
        return false;
    }
}

// https://drafts.csswg.org/css-ruby-1/#bidi
static UnicodeBidi forceBidiIsolationForRuby(UnicodeBidi unicodeBidi)
{
    switch (unicodeBidi) {
    case UnicodeBidi::Normal:
    case UnicodeBidi::Embed:
    case UnicodeBidi::Isolate:
        return UnicodeBidi::Isolate;
    case UnicodeBidi::Override:
    case UnicodeBidi::IsolateOverride:
        return UnicodeBidi::IsolateOverride;
    case UnicodeBidi::Plaintext:
        return UnicodeBidi::Plaintext;
    }
    ASSERT_NOT_REACHED();
    return UnicodeBidi::Isolate;
}

void Adjuster::adjust(RenderStyle& style, const RenderStyle* userAgentAppearanceStyle) const
{
    if (style.display() == DisplayType::Contents)
        adjustDisplayContentsStyle(style);

    if (m_element && (m_element->hasTagName(frameTag) || m_element->hasTagName(framesetTag))) {
        // Framesets ignore display and position properties.
        style.setPosition(PositionType::Static);
        style.setEffectiveDisplay(DisplayType::Block);
    }

    if (style.display() != DisplayType::None && style.display() != DisplayType::Contents) {
        if (RefPtr element = m_element) {
            // Tables never support the -webkit-* values for text-align and will reset back to the default.
            if (is<HTMLTableElement>(*element) && (style.textAlign() == TextAlignMode::WebKitLeft || style.textAlign() == TextAlignMode::WebKitCenter || style.textAlign() == TextAlignMode::WebKitRight))
                style.setTextAlign(TextAlignMode::Start);

            // Ruby text does not support float or position. This might change with evolution of the specification.
            if (element->hasTagName(rtTag)) {
                style.setPosition(PositionType::Static);
                style.setFloating(Float::None);
            }

            if (element->hasTagName(legendTag))
                style.setEffectiveDisplay(equivalentBlockDisplay(style));
        }

        if (hasUnsupportedRubyDisplay(style.display(), m_element.get()))
            style.setEffectiveDisplay(style.display() == DisplayType::RubyBlock ? DisplayType::Block : DisplayType::Inline);

        // Top layer elements are always position: absolute; unless the position is set to fixed.
        // https://fullscreen.spec.whatwg.org/#new-stacking-layer
        if (m_element != m_document->documentElement() && style.position() != PositionType::Absolute && style.position() != PositionType::Fixed && isInTopLayerOrBackdrop(style, m_element.get()))
            style.setPosition(PositionType::Absolute);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (m_element && m_document->documentElement() == m_element.get()))
            style.setEffectiveDisplay(equivalentBlockDisplay(style));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == DisplayType::Inline && style.pseudoElementType() == PseudoId::None && style.writingMode().computedWritingMode() != m_parentStyle.writingMode().computedWritingMode())
            style.setEffectiveDisplay(DisplayType::InlineBlock);

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
            style.setWritingMode(m_parentStyle.writingMode().computedWritingMode());

        if (style.isDisplayDeprecatedFlexibleBox()) {
            // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
            // of block-flow to anything other than StyleWritingMode::HorizontalTb.
            // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
            style.setWritingMode(StyleWritingMode::HorizontalTb);
        }

        if (m_parentBoxStyle.isDisplayDeprecatedFlexibleBox())
            style.setFloating(Float::None);

        // https://www.w3.org/TR/css-display/#transformations
        // "A parent with a grid or flex display value blockifies the box’s display type."
        if (m_parentBoxStyle.isDisplayFlexibleOrGridBox()) {
            style.setFloating(Float::None);
            style.setEffectiveDisplay(equivalentBlockDisplay(style));
        }

        // https://www.w3.org/TR/css-ruby-1/#anon-gen-inlinize
        if (shouldInlinifyForRuby(style, m_parentBoxStyle))
            style.setEffectiveDisplay(equivalentInlineDisplay(style));
        // https://drafts.csswg.org/css-ruby-1/#bidi
        if (style.isRubyContainerOrInternalRubyBox())
            style.setUnicodeBidi(forceBidiIsolationForRuby(style.unicodeBidi()));
    }

    auto hasAutoZIndex = [](const RenderStyle& style, const RenderStyle& parentBoxStyle, const Element* element) {
        if (style.hasAutoSpecifiedZIndex())
            return true;

        // SVG2: Contrary to the rules in CSS 2.1, the z-index property applies to all SVG elements regardless
        // of the value of the position property, with one exception: as for boxes in CSS 2.1, outer ‘svg’ elements
        // must be positioned for z-index to apply to them.
        if (element && element->document().settings().layerBasedSVGEngineEnabled()) {
            if (RefPtr svgElement = dynamicDowncast<SVGElement>(*element); svgElement && svgElement->isOutermostSVGSVGElement())
                return element->renderer() && element->renderer()->style().position() == PositionType::Static;

            return false;
        }

        // Make sure our z-index value is only applied if the object is positioned.
        return style.position() == PositionType::Static && !parentBoxStyle.isDisplayFlexibleOrGridBox();
    };

    if (hasAutoZIndex(style, m_parentBoxStyle, m_element.get()))
        style.setHasAutoUsedZIndex();
    else
        style.setUsedZIndex(style.specifiedZIndex());

    // For SVG compatibility purposes we have to consider the 'animatedLocalTransform' besides the RenderStyle to query
    // if an element has a transform. SVG transforms are not stored on the RenderStyle, and thus we need a special case here.
    // Same for the additional translation component present in RenderSVGTransformableContainer (that stems from <use> x/y
    // properties, that are transferred to the internal RenderSVGTransformableContainer), or for the viewBox-induced transformation
    // in RenderSVGViewportContainer. They all need to return true for 'hasTransformRelatedProperty'.
    auto hasTransformRelatedProperty = [](const RenderStyle& style, const Element* element, const RenderStyle& parentStyle) {
        if (element && element->document().settings().css3DTransformBackfaceVisibilityInteroperabilityEnabled() && style.backfaceVisibility() == BackfaceVisibility::Hidden && parentStyle.preserves3D())
            return true;

        if (style.hasTransformRelatedProperty())
            return true;

        if (element && element->document().settings().layerBasedSVGEngineEnabled()) {
            if (auto* graphicsElement = dynamicDowncast<SVGGraphicsElement>(element); graphicsElement && graphicsElement->hasTransformRelatedAttributes())
                return true;
        }

        return false;
    };

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoUsedZIndex()) {
        if ((m_element && m_document->documentElement() == m_element.get())
            || style.hasOpacity()
            || hasTransformRelatedProperty(style, m_element.get(), m_parentStyle)
            || style.hasMask()
            || style.clipPath()
            || style.boxReflect()
            || style.hasFilter()
            || style.hasBackdropFilter()
            || style.hasBlendMode()
            || style.hasIsolation()
            || style.position() == PositionType::Sticky
            || style.position() == PositionType::Fixed
            || style.willChangeCreatesStackingContext()
            || isInTopLayerOrBackdrop(style, m_element.get()))
            style.setUsedZIndex(0);
    }

    if (RefPtr element = m_element) {
        // Textarea considers overflow visible as auto.
        if (is<HTMLTextAreaElement>(*element)) {
            style.setOverflowX(style.overflowX() == Overflow::Visible ? Overflow::Auto : style.overflowX());
            style.setOverflowY(style.overflowY() == Overflow::Visible ? Overflow::Auto : style.overflowY());
        }

        if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element); input && input->isPasswordField())
            style.setTextSecurity(style.inputSecurity() == InputSecurity::Auto ? TextSecurity::Disc : TextSecurity::None);

        // Disallow -webkit-user-modify on :pseudo and ::pseudo elements.
        if (element->isInUserAgentShadowTree() && !element->userAgentPart().isNull())
            style.setUserModify(UserModify::ReadOnly);

        if (is<HTMLMarqueeElement>(*element)) {
            // For now, <marquee> requires an overflow clip to work properly.
            style.setOverflowX(Overflow::Hidden);
            style.setOverflowY(Overflow::Hidden);

            bool isVertical = style.marqueeDirection() == MarqueeDirection::Up || style.marqueeDirection() == MarqueeDirection::Down;
            // Make horizontal marquees not wrap.
            if (!isVertical) {
                style.setWhiteSpaceCollapse(WhiteSpaceCollapse::Collapse);
                style.setTextWrapMode(TextWrapMode::NoWrap);
                style.setTextAlign(TextAlignMode::Start);
            }
            // Apparently this is the expected legacy behavior.
            if (isVertical && style.height().isAuto())
                style.setHeight(WebCore::Length(200, LengthType::Fixed));
        }

        if (UNLIKELY(m_element->visibilityAdjustment().contains(VisibilityAdjustment::Subtree) || m_parentStyle.isInVisibilityAdjustmentSubtree()))
            style.setIsInVisibilityAdjustmentSubtree();
    }

    if (shouldInheritTextDecorationsInEffect(style, m_element.get()))
        style.addToTextDecorationsInEffect(style.textDecorationLine());
    else
        style.setTextDecorationsInEffect(style.textDecorationLine());

    bool overflowIsClipOrVisible = isOverflowClipOrVisible(style.overflowX()) && isOverflowClipOrVisible(style.overflowX());

    if (!overflowIsClipOrVisible && (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)) {
        // Tables only support overflow:hidden and overflow:visible and ignore anything else,
        // see https://drafts.csswg.org/css2/#overflow. As a table is not a block
        // container box the rules for resolving conflicting x and y values in CSS Overflow Module
        // Level 3 do not apply. Arguably overflow-x and overflow-y aren't allowed on tables but
        // all UAs allow it.
        if (style.overflowX() != Overflow::Hidden)
            style.setOverflowX(Overflow::Visible);
        if (style.overflowY() != Overflow::Hidden)
            style.setOverflowY(Overflow::Visible);
        // If we are left with conflicting overflow values for the x and y axes on a table then resolve
        // both to Overflow::Visible. This is interoperable behaviour but is not specced anywhere.
        if (style.overflowX() == Overflow::Visible)
            style.setOverflowY(Overflow::Visible);
        else if (style.overflowY() == Overflow::Visible)
            style.setOverflowX(Overflow::Visible);
    } else if (!isOverflowClipOrVisible(style.overflowY())) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        // Values of 'clip' and 'visible' can only be used with 'clip' and 'visible'.
        // If they aren't, 'clip' and 'visible' is reset.
        if (style.overflowX() == Overflow::Visible)
            style.setOverflowX(Overflow::Auto);
        else if (style.overflowX() == Overflow::Clip)
            style.setOverflowX(Overflow::Hidden);
    } else if (!isOverflowClipOrVisible(style.overflowX())) {
        // Values of 'clip' and 'visible' can only be used with 'clip' and 'visible'.
        // If they aren't, 'clip' and 'visible' is reset.
        if (style.overflowY() == Overflow::Visible)
            style.setOverflowY(Overflow::Auto);
        else if (style.overflowY() == Overflow::Clip)
            style.setOverflowY(Overflow::Hidden);
    }

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // Style::createForm_document.
    if ((style.overflowY() == Overflow::PagedX || style.overflowY() == Overflow::PagedY) && !(m_element && (m_element->hasTagName(htmlTag) || m_element->hasTagName(bodyTag))))
        style.setColumnStylesFromPaginationMode(WebCore::paginationModeForRenderStyle(style));

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    // Touch overflow scrolling creates a stacking context.
    if (style.hasAutoUsedZIndex() && style.useTouchOverflowScrolling() && (isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY())))
        style.setUsedZIndex(0);
#endif

    // Cull out any useless layers and also repeat patterns into additional layers.
    style.adjustBackgroundLayers();
    style.adjustMaskLayers();

    // Do the same for animations and transitions.
    style.adjustAnimations();
    style.adjustTransitions();

    // Do the same for scroll-timeline and view-timeline longhands.
    style.adjustScrollTimelines();
    style.adjustViewTimelines();

#if PLATFORM(COCOA)
    static const bool shouldAddIntrinsicMarginToFormControls = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotAddIntrinsicMarginsToFormControls);
    if (shouldAddIntrinsicMarginToFormControls) {
        // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
        // alter fonts and heights/widths.
        if (is<HTMLFormControlElement>(m_element) && style.computedFontSize() >= 11) {
            // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
            // so we have to treat all image buttons as though they were explicitly sized.
            if (RefPtr input = dynamicDowncast<HTMLInputElement>(*m_element); !input || !input->isImageButton())
                addIntrinsicMargins(style);
        }
    }
#endif

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        adjustThemeStyle(style, userAgentAppearanceStyle);

    // If we have first-letter pseudo style, do not share this style.
    if (style.hasPseudoStyle(PseudoId::FirstLetter))
        style.setUnique();

    // This should be kept in sync with requiresRenderingConsolidationForViewTransition
    if (style.preserves3D()) {
        bool forceToFlat = style.overflowX() != Overflow::Visible
            || style.hasOpacity()
            || style.overflowY() != Overflow::Visible
            || style.hasClip()
            || style.clipPath()
            || style.hasFilter()
            || style.hasIsolation()
            || style.hasMask()
            || style.hasBackdropFilter()
            || style.hasBlendMode()
            || !style.viewTransitionName().isNone();
        if (RefPtr element = m_element) {
            auto styleable = Styleable::fromElement(*element);
            forceToFlat |= styleable.capturedInViewTransition();
        }
        style.setTransformStyleForcedToFlat(forceToFlat);
    }

    if (RefPtr element = dynamicDowncast<SVGElement>(m_element))
        adjustSVGElementStyle(style, *element);

    // If the inherited value of justify-items includes the 'legacy' keyword (plus 'left', 'right' or
    // 'center'), 'legacy' computes to the the inherited value. Otherwise, 'auto' computes to 'normal'.
    if (m_parentBoxStyle.justifyItems().positionType() == ItemPositionType::Legacy && style.justifyItems().position() == ItemPosition::Legacy)
        style.setJustifyItems(m_parentBoxStyle.justifyItems());

    style.setUsedTouchActions(computeUsedTouchActions(style, m_parentStyle.usedTouchActions()));

    // Counterparts in Element::addToTopLayer/removeFromTopLayer & SharingResolver::canShareStyleWithElement need to match!
    auto hasInertAttribute = [] (const Element* element) -> bool {
        return is<HTMLElement>(element) && element->hasAttributeWithoutSynchronization(HTMLNames::inertAttr);
    };
    auto isInertSubtreeRoot = [this, hasInertAttribute] (const Element* element) -> bool {
        if (m_document->activeModalDialog() && element == m_document->documentElement())
            return true;
        if (hasInertAttribute(element))
            return true;
#if ENABLE(FULLSCREEN_API)
        if (CheckedPtr fullscreenManager = m_document->fullscreenManagerIfExists(); fullscreenManager && fullscreenManager->fullscreenElement() && element == m_document->documentElement())
            return true;
#endif
        return false;
    };
    if (isInertSubtreeRoot(m_element.get()))
        style.setEffectiveInert(true);

    if (RefPtr element = m_element) {
        // Make sure the active dialog is interactable when the whole document is blocked by the modal dialog
        if (element == m_document->activeModalDialog() && !hasInertAttribute(element.get()))
            style.setEffectiveInert(false);

#if ENABLE(FULLSCREEN_API)
        if (CheckedPtr fullscreenManager = m_document->fullscreenManagerIfExists(); fullscreenManager && m_element == fullscreenManager->fullscreenElement() && !hasInertAttribute(m_element.get()))
            style.setEffectiveInert(false);
#endif

        style.setEventListenerRegionTypes(computeEventListenerRegionTypes(m_document, style, *m_element, m_parentStyle.eventListenerRegionTypes()));

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        // Every element will automatically get an interaction region which is not useful, ignoring the `cursor: pointer;` on the body.
        if (is<HTMLBodyElement>(*m_element) && style.cursor() == CursorType::Pointer && style.eventListenerRegionTypes().contains(EventListenerRegionType::MouseClick))
            style.setCursor(CursorType::Auto);
#endif

#if ENABLE(TEXT_AUTOSIZING)
        if (m_document->settings().textAutosizingUsesIdempotentMode())
            adjustForTextAutosizing(style, *m_element);
#endif
    }

    if (m_parentStyle.contentVisibility() != ContentVisibility::Hidden) {
        auto isSkippedContentRoot = style.contentVisibility() != ContentVisibility::Visible && doesSizeContainmentApplyByDisplayType(style) && m_element && !m_element->isRelevantToUser();
        if (isSkippedContentRoot)
            style.setUsedContentVisibility(style.contentVisibility());
    }

    if (style.contentVisibility() == ContentVisibility::Auto) {
        style.containIntrinsicWidthAddAuto();
        style.containIntrinsicHeightAddAuto();
    }

    adjustForSiteSpecificQuirks(style);
}

static bool hasEffectiveDisplayNoneForDisplayContents(const Element& element)
{
    using namespace ElementNames;

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

    // https://drafts.csswg.org/css-display-3/#unbox-html
    switch (element.elementName()) {
    case HTML::br:
    case HTML::wbr:
    case HTML::meter:
    case HTML::applet:
    case HTML::progress:
    case HTML::canvas:
    case HTML::embed:
    case HTML::object:
    case HTML::audio:
    case HTML::iframe:
    case HTML::img:
    case HTML::video:
    case HTML::frame:
    case HTML::frameset:
    case HTML::input:
    case HTML::textarea:
    case HTML::select:
        return true;
    default:
        break;
    }

    return false;
}

void Adjuster::adjustDisplayContentsStyle(RenderStyle& style) const
{
    bool isInTopLayer = isInTopLayerOrBackdrop(style, m_element.get());
    if (isInTopLayer || m_document->documentElement() == m_element.get()) {
        style.setEffectiveDisplay(DisplayType::Block);
        return;
    }

    if (!m_element && style.pseudoElementType() != PseudoId::Before && style.pseudoElementType() != PseudoId::After) {
        style.setEffectiveDisplay(DisplayType::None);
        return;
    }

    if (m_element && hasEffectiveDisplayNoneForDisplayContents(*m_element))
        style.setEffectiveDisplay(DisplayType::None);
}

void Adjuster::adjustSVGElementStyle(RenderStyle& style, const SVGElement& svgElement)
{
    // Only the root <svg> element in an SVG document fragment tree honors css position
    if (!svgElement.isOutermostSVGSVGElement())
        style.setPosition(RenderStyle::initialPosition());

    // SVG2: A new stacking context must be established at an SVG element for its descendants if:
    // - it is the root element
    // - the "z-index" property applies to the element and its computed value is an integer
    // - the element is an outermost svg element, or a "foreignObject", "image", "marker", "mask", "pattern", "symbol" or "use" element
    // - the element is an inner "svg" element and the computed value of its "overflow" property is a value other than visible
    // - the element is subject to explicit clipping:
    //   - the "clip" property applies to the element and it has a computed value other than auto
    //   - the "clip-path" property applies to the element and it has a computed value other than none
    // - the "mask" property applies to the element and it has a computed value other than none
    // - the "filter" property applies to the element and it has a computed value other than none
    // - a property defined in another specification is applied and that property is defined to establish a stacking context in SVG
    //
    // Some of the rules above were already enforced in StyleResolver::adjust() - for those cases assertions were added.
    if (svgElement.document().settings().layerBasedSVGEngineEnabled() && style.hasAutoUsedZIndex()) {
        // adjust() has already assigned a z-index of 0 if clip / filter is present or the element is the root element.
        ASSERT(!style.clipPath());
        ASSERT(!style.hasFilter());

        if (svgElement.isOutermostSVGSVGElement()
            || svgElement.hasTagName(SVGNames::foreignObjectTag)
            || svgElement.hasTagName(SVGNames::imageTag)
            || svgElement.hasTagName(SVGNames::markerTag)
            || svgElement.hasTagName(SVGNames::maskTag)
            || svgElement.hasTagName(SVGNames::patternTag)
            || svgElement.hasTagName(SVGNames::symbolTag)
            || svgElement.hasTagName(SVGNames::useTag)
            || (svgElement.isInnerSVGSVGElement() && (style.overflowX() != Overflow::Visible || style.overflowY() != Overflow::Visible))
            || style.hasPositionedMask())
        style.setUsedZIndex(0);
    }

    // (Legacy)RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
    // not be scaled again.
    if (svgElement.hasTagName(SVGNames::foreignObjectTag))
        style.setUsedZoom(RenderStyle::initialZoom());

    // SVG text layout code expects us to be a block-level style element.
    if ((svgElement.hasTagName(SVGNames::foreignObjectTag) || svgElement.hasTagName(SVGNames::textTag)) && style.isDisplayInlineType())
        style.setEffectiveDisplay(DisplayType::Block);
}

void Adjuster::adjustAnimatedStyle(RenderStyle& style, OptionSet<AnimationImpact> impact) const
{
    adjust(style, nullptr);

    // Set an explicit used z-index in two cases:
    // 1. When the element respects z-index, and the style has an explicit z-index set (for example, the animation
    //    itself may animate z-index).
    // 2. When we want the stacking context side-effets of explicit z-index, via forceStackingContext.
    // It's important to not clobber an existing used z-index, since an earlier animation may have set it, but we
    // may still need to update the used z-index value from the specified value.
    
    if (style.hasAutoUsedZIndex() && impact.contains(AnimationImpact::ForcesStackingContext))
        style.setUsedZIndex(0);
}

void Adjuster::adjustThemeStyle(RenderStyle& style, const RenderStyle* userAgentAppearanceStyle) const
{
    ASSERT(style.hasAppearance());
    auto isOldWidthAuto = style.width().isAuto();
    auto isOldMinWidthAuto = style.minWidth().isAuto();
    auto isOldHeightAuto = style.height().isAuto();
    auto isOldMinHeightAuto = style.minHeight().isAuto();

    RenderTheme::singleton().adjustStyle(style, m_element.get(), userAgentAppearanceStyle);

    if (style.containsSize()) {
        if (style.containIntrinsicWidthType() != ContainIntrinsicSizeType::None) {
            if (isOldWidthAuto)
                style.setWidth(WebCore::Length(LengthType::Auto));
            if (isOldMinWidthAuto)
                style.setMinWidth(WebCore::Length(LengthType::Auto));
        }
        if (style.containIntrinsicHeightType() != ContainIntrinsicSizeType::None) {
            if (isOldHeightAuto)
                style.setHeight(WebCore::Length(LengthType::Auto));
            if (isOldMinHeightAuto)
                style.setMinHeight(WebCore::Length(LengthType::Auto));
        }
    }
}

void Adjuster::adjustForSiteSpecificQuirks(RenderStyle& style) const
{
    if (!m_element)
        return;

    if (is<HTMLBodyElement>(m_element) && m_document->quirks().needsBodyScrollbarWidthNoneDisabledQuirk()) {
        if (style.scrollbarWidth() == ScrollbarWidth::None)
            style.setScrollbarWidth(ScrollbarWidth::Auto);
    }

    if (m_document->quirks().needsGMailOverflowScrollQuirk()) {
        // This turns sidebar scrollable without mouse move event.
        static MainThreadNeverDestroyed<const AtomString> roleValue("navigation"_s);
        if (style.overflowY() == Overflow::Hidden && m_element->attributeWithoutSynchronization(roleAttr) == roleValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (m_document->quirks().needsIPadSkypeOverflowScrollQuirk()) {
        // This makes the layout scrollable and makes visible the buttons hidden outside of the viewport.
        // static MainThreadNeverDestroyed<const AtomString> selectorValue(".app-container .noFocusOutline > div"_s);
        if (RefPtr div = dynamicDowncast<HTMLDivElement>(m_element)) {
            auto matchesNoFocus = div->matches(".app-container .noFocusOutline > div"_s);
            if (matchesNoFocus.hasException())
                return;
            if (matchesNoFocus.returnValue()) {
                if (style.overflowY() == Overflow::Hidden)
                    style.setOverflowY(Overflow::Scroll);
            }
        }
    }
    if (m_document->quirks().needsYouTubeOverflowScrollQuirk()) {
        // This turns sidebar scrollable without hover.
        static MainThreadNeverDestroyed<const AtomString> idValue("guide-inner-content"_s);
        if (style.overflowY() == Overflow::Hidden && m_element->idForStyleResolution() == idValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (m_document->quirks().needsWeChatScrollingQuirk()) {
        static MainThreadNeverDestroyed<const AtomString> class1("tree-select"_s);
        static MainThreadNeverDestroyed<const AtomString> class2("v-tree-select"_s);
        const auto& flexBasis = style.flexBasis();
        if (style.minHeight().isAuto()
            && style.display() == DisplayType::Flex
            && style.flexGrow() == 1
            && style.flexShrink() == 1
            && (flexBasis.isPercent() || flexBasis.isFixed())
            && flexBasis.value() == 0
            && const_cast<Element*>(m_element.get())->classList().contains(class1)
            && const_cast<Element*>(m_element.get())->classList().contains(class2))
            style.setMinHeight(WebCore::Length(0, LengthType::Fixed));
    }
    if (m_document->quirks().needsPrimeVideoUserSelectNoneQuirk()) {
        static MainThreadNeverDestroyed<const AtomString> className("webPlayerSDKUiContainer"_s);
        if (m_element->hasClassName(className))
            style.setUserSelect(UserSelect::None);
    }

#if PLATFORM(IOS_FAMILY)
    if (m_document->quirks().needsGoogleMapsScrollingQuirk()) {
        static MainThreadNeverDestroyed<const AtomString> className("PUtLdf"_s);
        if (is<HTMLBodyElement>(*m_element) && m_element->hasClassName(className))
            style.setUsedTouchActions({ TouchAction::Auto });
    }
#endif

#if ENABLE(VIDEO)
    if (m_document->quirks().needsFullscreenDisplayNoneQuirk()) {
        if (RefPtr div = dynamicDowncast<HTMLDivElement>(m_element); div && style.display() == DisplayType::None) {
            static MainThreadNeverDestroyed<const AtomString> instreamNativeVideoDivClass("instream-native-video--mobile"_s);
            static MainThreadNeverDestroyed<const AtomString> videoElementID("vjs_video_3_html5_api"_s);

            if (div->hasClassName(instreamNativeVideoDivClass)) {
                RefPtr video = dynamicDowncast<HTMLVideoElement>(div->treeScope().getElementById(videoElementID));
                if (video && video->isFullscreen())
                    style.setEffectiveDisplay(DisplayType::Block);
            }
        }
    }
#if ENABLE(FULLSCREEN_API)
    if (CheckedPtr fullscreenManager = m_document->fullscreenManagerIfExists(); fullscreenManager && m_document->quirks().needsFullscreenObjectFitQuirk()) {
        static MainThreadNeverDestroyed<const AtomString> playerClassName("top-player-video-element"_s);
        bool isFullscreen = fullscreenManager->isFullscreen();
        RefPtr video = dynamicDowncast<HTMLVideoElement>(m_element);
        if (video && isFullscreen && video->hasClassName(playerClassName) && style.objectFit() == ObjectFit::Fill)
            style.setObjectFit(ObjectFit::Contain);
    }
#endif
#endif
}

void Adjuster::propagateToDocumentElementAndInitialContainingBlock(Update& update, const Document& document)
{
    auto* body = document.body();
    auto* bodyStyle = body ? update.elementStyle(*body) : nullptr;
    auto* documentElementStyle = update.elementStyle(*document.documentElement());

    if (!documentElementStyle)
        return;

    // https://drafts.csswg.org/css-contain-2/#contain-property
    // "Additionally, when any containments are active on either the HTML html or body elements, propagation of
    // properties from the body element to the initial containing block, the viewport, or the canvas background, is disabled."
    auto shouldPropagateFromBody = [&] {
        if (bodyStyle && !bodyStyle->usedContain().isEmpty())
            return false;
        return documentElementStyle->usedContain().isEmpty();
    }();

    auto writingMode = [&] {
        // FIXME: The spec says body should win.
        if (documentElementStyle->hasExplicitlySetWritingMode())
            return documentElementStyle->writingMode().computedWritingMode();
        if (shouldPropagateFromBody && bodyStyle && bodyStyle->hasExplicitlySetWritingMode())
            return bodyStyle->writingMode().computedWritingMode();
        return RenderStyle::initialWritingMode();
    }();

    auto direction = [&] {
        if (documentElementStyle->hasExplicitlySetDirection())
            return documentElementStyle->writingMode().computedTextDirection();
        if (shouldPropagateFromBody && bodyStyle && bodyStyle->hasExplicitlySetDirection())
            return bodyStyle->writingMode().computedTextDirection();
        return RenderStyle::initialDirection();
    }();

    // https://drafts.csswg.org/css-writing-modes-3/#icb
    WritingMode viewWritingMode = document.renderView()->writingMode();
    if (writingMode != viewWritingMode.computedWritingMode() || direction != viewWritingMode.computedTextDirection()) {
        auto newRootStyle = RenderStyle::clonePtr(document.renderView()->style());
        newRootStyle->setWritingMode(writingMode);
        newRootStyle->setDirection(direction);
        newRootStyle->setColumnStylesFromPaginationMode(document.view()->pagination().mode);
        update.addInitialContainingBlockUpdate(WTFMove(newRootStyle));
    }

    // https://drafts.csswg.org/css-writing-modes-3/#principal-flow
    if (writingMode != documentElementStyle->writingMode().computedWritingMode() || direction != documentElementStyle->writingMode().computedTextDirection()) {
        auto* documentElementUpdate = update.elementUpdate(*document.documentElement());
        if (!documentElementUpdate) {
            update.addElement(*document.documentElement(), nullptr, { RenderStyle::clonePtr(*documentElementStyle) });
            documentElementUpdate = update.elementUpdate(*document.documentElement());
        }
        documentElementUpdate->style->setWritingMode(writingMode);
        documentElementUpdate->style->setDirection(direction);
        documentElementUpdate->change = std::max(documentElementUpdate->change, Change::Inherited);
    }
}

std::unique_ptr<RenderStyle> Adjuster::restoreUsedDocumentElementStyleToComputed(const RenderStyle& style)
{
    if (style.writingMode().computedWritingMode() == RenderStyle::initialWritingMode() && style.writingMode().computedTextDirection() == RenderStyle::initialDirection())
        return { };

    auto adjusted = RenderStyle::clonePtr(style);
    if (!style.hasExplicitlySetWritingMode())
        adjusted->setWritingMode(RenderStyle::initialWritingMode());
    if (!style.hasExplicitlySetDirection())
        adjusted->setDirection(RenderStyle::initialDirection());

    return adjusted;
}

#if ENABLE(TEXT_AUTOSIZING)
static bool hasTextChild(const Element& element)
{
    for (auto* child = element.firstChild(); child; child = child->nextSibling()) {
        if (is<Text>(child))
            return true;
    }
    return false;
}

auto Adjuster::adjustmentForTextAutosizing(const RenderStyle& style, const Element& element) -> AdjustmentForTextAutosizing
{
    AdjustmentForTextAutosizing adjustmentForTextAutosizing;

    auto& document = element.document();
    if (!document.settings().textAutosizingEnabled()
        || !document.settings().textAutosizingUsesIdempotentMode()
        || document.settings().idempotentModeAutosizingOnlyHonorsPercentages())
        return adjustmentForTextAutosizing;

    auto newStatus = AutosizeStatus::computeStatus(style);
    if (newStatus != style.autosizeStatus())
        adjustmentForTextAutosizing.newStatus = newStatus;

    if (style.textSizeAdjust().isNone())
        return adjustmentForTextAutosizing;

    float initialScale = document.page() ? document.page()->initialScaleIgnoringContentSize() : 1;
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

        adjustmentForTextAutosizing.newLineHeight = minimumLineHeight;
    };

    auto fontDescription = style.fontDescription();
    auto initialComputedFontSize = fontDescription.computedSize();
    auto specifiedFontSize = fontDescription.specifiedSize();
    bool isCandidate = style.isIdempotentTextAutosizingCandidate(newStatus);
    if (!isCandidate && WTF::areEssentiallyEqual(initialComputedFontSize, specifiedFontSize))
        return adjustmentForTextAutosizing;

    auto adjustedFontSize = AutosizeStatus::idempotentTextSize(fontDescription.specifiedSize(), initialScale);
    if (isCandidate && WTF::areEssentiallyEqual(initialComputedFontSize, adjustedFontSize))
        return adjustmentForTextAutosizing;

    if (!hasTextChild(element))
        return adjustmentForTextAutosizing;

    adjustmentForTextAutosizing.newFontSize = isCandidate ? adjustedFontSize : specifiedFontSize;

    // FIXME: We should restore computed line height to its original value in the case where the element is not
    // an idempotent text autosizing candidate; otherwise, if an element that is a text autosizing candidate contains
    // children which are not autosized, the non-autosized content will end up with a boosted line height.
    if (isCandidate)
        adjustLineHeightIfNeeded(adjustedFontSize);

    return adjustmentForTextAutosizing;
}

bool Adjuster::adjustForTextAutosizing(RenderStyle& style, const Element& element, AdjustmentForTextAutosizing adjustment)
{
    AutosizeStatus::updateStatus(style);
    if (auto newFontSize = adjustment.newFontSize) {
        auto fontDescription = style.fontDescription();
        fontDescription.setComputedSize(*newFontSize);
        style.setFontDescription(WTFMove(fontDescription));
        style.fontCascade().update(&element.document().fontSelector());
    }
    if (auto newLineHeight = adjustment.newLineHeight)
        style.setLineHeight({ *newLineHeight, LengthType::Fixed });
    if (auto newStatus = adjustment.newStatus)
        style.setAutosizeStatus(*newStatus);
    return adjustment.newFontSize || adjustment.newLineHeight;
}

bool Adjuster::adjustForTextAutosizing(RenderStyle& style, const Element& element)
{
    return adjustForTextAutosizing(style, element, adjustmentForTextAutosizing(style, element));
}
#endif

void Adjuster::adjustVisibilityForPseudoElement(RenderStyle& style, const Element& host)
{
    if ((style.pseudoElementType() == PseudoId::After && host.visibilityAdjustment().contains(VisibilityAdjustment::AfterPseudo))
        || (style.pseudoElementType() == PseudoId::Before && host.visibilityAdjustment().contains(VisibilityAdjustment::BeforePseudo)))
        style.setIsInVisibilityAdjustmentSubtree();
}

}
}
