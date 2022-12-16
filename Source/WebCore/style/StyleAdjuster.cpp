/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
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
#include "DOMWindow.h"
#include "ElementInlines.h"
#include "ElementName.h"
#include "EventNames.h"
#include "FrameView.h"
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
#include "MathMLElement.h"
#include "ModalContainerObserver.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleUpdate.h"
#include "Text.h"
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

Adjuster::Adjuster(const Document& document, const RenderStyle& parentStyle, const RenderStyle* parentBoxStyle, const Element* element)
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
    const int intrinsicMargin = clampToInteger(2 * style.effectiveZoom());

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "hasQuirk" to decide the margin wasn't set is kind of lame.
    if (style.width().isIntrinsicOrAuto()) {
        if (style.marginLeft().hasQuirk())
            style.setMarginLeft(Length(intrinsicMargin, LengthType::Fixed));
        if (style.marginRight().hasQuirk())
            style.setMarginRight(Length(intrinsicMargin, LengthType::Fixed));
    }

    if (style.height().isAuto()) {
        if (style.marginTop().hasQuirk())
            style.setMarginTop(Length(intrinsicMargin, LengthType::Fixed));
        if (style.marginBottom().hasQuirk())
            style.setMarginBottom(Length(intrinsicMargin, LengthType::Fixed));
    }
}
#endif

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
        return display;
    case DisplayType::InlineTable:
        return DisplayType::Table;
    case DisplayType::InlineBox:
        return DisplayType::Box;
    case DisplayType::InlineFlex:
        return DisplayType::Flex;
    case DisplayType::InlineGrid:
        return DisplayType::Grid;

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

static bool shouldInheritTextDecorationsInEffect(const RenderStyle& style, const Element* element)
{
    if (style.isFloating() || style.hasOutOfFlowPosition())
        return false;

    auto isAtUserAgentShadowBoundary = [&] {
        if (!element)
            return false;
        auto* parentNode = element->parentNode();
        return parentNode && parentNode->isUserAgentShadowRoot();
    }();

    // There is no other good way to prevent decorations from affecting user agent shadow trees.
    if (isAtUserAgentShadowBoundary)
        return false;

    switch (style.display()) {
    case DisplayType::Table:
    case DisplayType::InlineTable:
    case DisplayType::InlineBlock:
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

void Adjuster::adjustEventListenerRegionTypesForRootStyle(RenderStyle& rootStyle, const Document& document)
{
    auto regionTypes = computeEventListenerRegionTypes(document, rootStyle, document, { });
    if (auto* window = document.domWindow())
        regionTypes.add(computeEventListenerRegionTypes(document, rootStyle, *window, { }));

    rootStyle.setEventListenerRegionTypes(regionTypes);
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
    if (document.page() && document.page()->shouldBuildInteractionRegions() && eventTarget.isNode()) {
        const auto& node = downcast<Node>(eventTarget);
        if (node.willRespondToMouseClickEventsWithEditability(node.computeEditabilityForMouseClickEvents(&style)))
            types.add(EventListenerRegionType::MouseClick);
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

void Adjuster::adjust(RenderStyle& style, const RenderStyle* userAgentAppearanceStyle) const
{
    if (style.display() == DisplayType::Contents)
        adjustDisplayContentsStyle(style);

    if (style.display() != DisplayType::None && style.display() != DisplayType::Contents) {
        if (m_element) {
            // Tables never support the -webkit-* values for text-align and will reset back to the default.
            if (is<HTMLTableElement>(*m_element) && (style.textAlign() == TextAlignMode::WebKitLeft || style.textAlign() == TextAlignMode::WebKitCenter || style.textAlign() == TextAlignMode::WebKitRight))
                style.setTextAlign(TextAlignMode::Start);

            // Frames and framesets never honor position:relative or position:absolute. This is necessary to
            // fix a crash where a site tries to position these objects. They also never honor display.
            if (m_element->hasTagName(frameTag) || m_element->hasTagName(framesetTag)) {
                style.setPosition(PositionType::Static);
                style.setEffectiveDisplay(DisplayType::Block);
            }

            // Ruby text does not support float or position. This might change with evolution of the specification.
            if (m_element->hasTagName(rtTag)) {
                style.setPosition(PositionType::Static);
                style.setFloating(Float::None);
            }

            if (m_element->hasTagName(legendTag))
                style.setEffectiveDisplay(equivalentBlockDisplay(style));
        }

        // Top layer elements are always position: absolute; unless the position is set to fixed.
        // https://fullscreen.spec.whatwg.org/#new-stacking-layer
        if (m_element != m_document.documentElement() && style.position() != PositionType::Absolute && style.position() != PositionType::Fixed && isInTopLayerOrBackdrop(style, m_element))
            style.setPosition(PositionType::Absolute);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (m_element && m_document.documentElement() == m_element))
            style.setEffectiveDisplay(equivalentBlockDisplay(style));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == DisplayType::Inline && style.styleType() == PseudoId::None && style.writingMode() != m_parentStyle.writingMode())
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
            style.setWritingMode(m_parentStyle.writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than WritingMode::TopToBottom.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style.writingMode() != WritingMode::TopToBottom && (style.display() == DisplayType::Box || style.display() == DisplayType::InlineBox))
            style.setWritingMode(WritingMode::TopToBottom);

        // https://www.w3.org/TR/css-display/#transformations
        // "A parent with a grid or flex display value blockifies the box’s display type."
        if (m_parentBoxStyle.isDisplayFlexibleOrGridBox()) {
            style.setFloating(Float::None);
            style.setEffectiveDisplay(equivalentBlockDisplay(style));
        }
    }

    auto hasAutoZIndex = [](const RenderStyle& style, const RenderStyle& parentBoxStyle, const Element* element) {
        if (style.hasAutoSpecifiedZIndex())
            return true;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        // SVG2: Contrary to the rules in CSS 2.1, the z-index property applies to all SVG elements regardless
        // of the value of the position property, with one exception: as for boxes in CSS 2.1, outer ‘svg’ elements
        // must be positioned for z-index to apply to them.
        if (element && element->document().settings().layerBasedSVGEngineEnabled()) {
            if (auto* svgElement = dynamicDowncast<SVGElement>(element); svgElement && svgElement->isOutermostSVGSVGElement())
                return element->renderer() && element->renderer()->style().position() == PositionType::Static;

            return false;
        }
#else
        UNUSED_PARAM(element);
#endif

        // Make sure our z-index value is only applied if the object is positioned.
        return style.position() == PositionType::Static && !parentBoxStyle.isDisplayFlexibleOrGridBox();
    };

    if (hasAutoZIndex(style, m_parentBoxStyle, m_element))
        style.setHasAutoUsedZIndex();
    else
        style.setUsedZIndex(style.specifiedZIndex());

    // For SVG compatibility purposes we have to consider the 'animatedLocalTransform' besides the RenderStyle to query
    // if an element has a transform. SVG transforms are not stored on the RenderStyle, and thus we need a special case here.
    // Same for the additional translation component present in RenderSVGTransformableContainer (that stems from <use> x/y
    // properties, that are transferred to the internal RenderSVGTransformableContainer), or for the viewBox-induced transformation
    // in RenderSVGViewportContainer. They all need to return true for 'hasTransformRelatedProperty'.
    auto hasTransformRelatedProperty = [](const RenderStyle& style, const Element* element) {
        if (style.hasTransformRelatedProperty())
            return true;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (element && element->document().settings().layerBasedSVGEngineEnabled()) {
            if (auto* graphicsElement = dynamicDowncast<SVGGraphicsElement>(element); graphicsElement && graphicsElement->hasTransformRelatedAttributes())
                return true;
        }
#else
        UNUSED_PARAM(element);
#endif

        return false;
    };

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoUsedZIndex()) {
        if ((m_element && m_document.documentElement() == m_element)
            || style.hasOpacity()
            || hasTransformRelatedProperty(style, m_element)
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
            || style.willChangeCreatesStackingContext()
            || isInTopLayerOrBackdrop(style, m_element))
            style.setUsedZIndex(0);
    }

    if (m_element) {
        // Textarea considers overflow visible as auto.
        if (is<HTMLTextAreaElement>(*m_element)) {
            style.setOverflowX(style.overflowX() == Overflow::Visible ? Overflow::Auto : style.overflowX());
            style.setOverflowY(style.overflowY() == Overflow::Visible ? Overflow::Auto : style.overflowY());
        }

        if (is<HTMLInputElement>(*m_element) && downcast<HTMLInputElement>(*m_element).isPasswordField())
            style.setTextSecurity(style.inputSecurity() == InputSecurity::Auto ? TextSecurity::Disc : TextSecurity::None);

        // Disallow -webkit-user-modify on :pseudo and ::pseudo elements.
        if (!m_element->shadowPseudoId().isNull())
            style.setUserModify(UserModify::ReadOnly);

        if (is<HTMLMarqueeElement>(*m_element)) {
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
                style.setHeight(Length(200, LengthType::Fixed));
        }
    }

    if (shouldInheritTextDecorationsInEffect(style, m_element))
        style.addToTextDecorationsInEffect(style.textDecorationLine());
    else
        style.setTextDecorationsInEffect(style.textDecorationLine());

    auto overflowReplacement = [] (Overflow overflow, Overflow overflowInOtherDimension) -> std::optional<Overflow> {
        if (overflow != Overflow::Visible && overflow != Overflow::Clip) {
            if (overflowInOtherDimension == Overflow::Visible)
                return Overflow::Auto;
            if (overflowInOtherDimension == Overflow::Clip)
                return Overflow::Hidden;
        }
        return std::nullopt;
    };

    // If either overflow value is not visible, change to auto. Similarly if either overflow
    // value is not clip, change to hidden.
    // FIXME: Once we implement pagination controls, overflow-x should default to hidden
    // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
    // default to auto so we can at least scroll through the pages.
    if (auto replacement = overflowReplacement(style.overflowY(), style.overflowX()))
        style.setOverflowX(*replacement);
    else if (auto replacement = overflowReplacement(style.overflowX(), style.overflowY()))
        style.setOverflowY(*replacement);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // Style::createForm_document.
    if ((style.overflowY() == Overflow::PagedX || style.overflowY() == Overflow::PagedY) && !(m_element && (m_element->hasTagName(htmlTag) || m_element->hasTagName(bodyTag))))
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

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotAddIntrinsicMarginsToFormControls)) {
        // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
        // alter fonts and heights/widths.
        if (is<HTMLFormControlElement>(m_element) && style.computedFontPixelSize() >= 11) {
            // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
            // so we have to treat all image buttons as though they were explicitly sized.
            if (!is<HTMLInputElement>(*m_element) || !downcast<HTMLInputElement>(*m_element).isImageButton())
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

    if (style.preserves3D()) {
        bool forceToFlat = style.overflowX() != Overflow::Visible
            || style.hasOpacity()
            || style.overflowY() != Overflow::Visible
            || style.hasClip()
            || style.clipPath()
            || style.hasFilter()
            || style.hasIsolation()
            || style.hasMask()
#if ENABLE(FILTERS_LEVEL_2)
            || style.hasBackdropFilter()
#endif
            || style.hasBlendMode();
        style.setTransformStyleForcedToFlat(forceToFlat);
    }

    if (is<SVGElement>(m_element))
        adjustSVGElementStyle(style, downcast<SVGElement>(*m_element));

    // If the inherited value of justify-items includes the 'legacy' keyword (plus 'left', 'right' or
    // 'center'), 'legacy' computes to the the inherited value. Otherwise, 'auto' computes to 'normal'.
    if (m_parentBoxStyle.justifyItems().positionType() == ItemPositionType::Legacy && style.justifyItems().position() == ItemPosition::Legacy)
        style.setJustifyItems(m_parentBoxStyle.justifyItems());

    style.setEffectiveTouchActions(computeEffectiveTouchActions(style, m_parentStyle.effectiveTouchActions()));

    // Counterparts in Element::addToTopLayer/removeFromTopLayer & SharingResolver::canShareStyleWithElement need to match!
    auto hasInertAttribute = [this] (const Element* element) -> bool {
        return m_document.settings().inertAttributeEnabled() && is<HTMLElement>(element) && element->hasAttributeWithoutSynchronization(HTMLNames::inertAttr);
    };
    auto isInertSubtreeRoot = [this, hasInertAttribute] (const Element* element) -> bool {
        if (m_document.activeModalDialog() && element == m_document.documentElement())
            return true;
        if (hasInertAttribute(element))
            return true;
#if ENABLE(FULLSCREEN_API)
        if (m_document.fullscreenManager().fullscreenElement() && element == m_document.documentElement())
            return true;
#endif
        return false;
    };
    if (isInertSubtreeRoot(m_element))
        style.setEffectiveInert(true);

    if (m_element) {
        // Make sure the active dialog is interactable when the whole document is blocked by the modal dialog
        if (m_element == m_document.activeModalDialog() && !hasInertAttribute(m_element))
            style.setEffectiveInert(false);

#if ENABLE(FULLSCREEN_API)
        if (m_element == m_document.fullscreenManager().fullscreenElement() && !hasInertAttribute(m_element))
            style.setEffectiveInert(false);
#endif

        style.setEventListenerRegionTypes(computeEventListenerRegionTypes(m_document, style, *m_element, m_parentStyle.eventListenerRegionTypes()));

#if ENABLE(TEXT_AUTOSIZING)
        if (m_document.settings().textAutosizingUsesIdempotentMode())
            adjustForTextAutosizing(style, *m_element);
#endif

        if (auto observer = m_element->document().modalContainerObserverIfExists()) {
            if (observer->shouldHide(*m_element))
                style.setDisplay(DisplayType::None);
            if (observer->shouldMakeVerticallyScrollable(*m_element))
                style.setOverflowY(Overflow::Auto);
        }
    }

    if (style.contentVisibility() == ContentVisibility::Hidden)
        style.setEffectiveSkipsContent(true);

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
    switch (element.tagQName().elementName()) {
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
    bool isInTopLayer = isInTopLayerOrBackdrop(style, m_element);
    if (isInTopLayer || m_document.documentElement() == m_element) {
        style.setEffectiveDisplay(DisplayType::Block);
        return;
    }

    if (!m_element && style.styleType() != PseudoId::Before && style.styleType() != PseudoId::After) {
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
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
    // Some of the rules above were already enforced in StyleResolver::adjustRenderStyle() - for those cases assertions were added.
    if (svgElement.document().settings().layerBasedSVGEngineEnabled() && style.hasAutoUsedZIndex()) {
        // adjustRenderStyle() has already assigned a z-index of 0 if clip / filter is present or the element is the root element.
        ASSERT(!style.hasClip());
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
#endif

    // (Legacy)RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
    // not be scaled again.
    if (svgElement.hasTagName(SVGNames::foreignObjectTag))
        style.setEffectiveZoom(RenderStyle::initialZoom());

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

    RenderTheme::singleton().adjustStyle(style, m_element, userAgentAppearanceStyle);

    if (style.containsSize()) {
        if (style.containIntrinsicWidthType() != ContainIntrinsicSizeType::None) {
            if (isOldWidthAuto)
                style.setWidth(Length(LengthType::Auto));
            if (isOldMinWidthAuto)
                style.setMinWidth(Length(LengthType::Auto));
        }
        if (style.containIntrinsicHeightType() != ContainIntrinsicSizeType::None) {
            if (isOldHeightAuto)
                style.setHeight(Length(LengthType::Auto));
            if (isOldMinHeightAuto)
                style.setMinHeight(Length(LengthType::Auto));
        }
    }
}

void Adjuster::adjustForSiteSpecificQuirks(RenderStyle& style) const
{
    if (!m_element)
        return;

    if (m_document.quirks().needsGMailOverflowScrollQuirk()) {
        // This turns sidebar scrollable without mouse move event.
        static MainThreadNeverDestroyed<const AtomString> roleValue("navigation"_s);
        if (style.overflowY() == Overflow::Hidden && m_element->attributeWithoutSynchronization(roleAttr) == roleValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (m_document.quirks().needsYouTubeOverflowScrollQuirk()) {
        // This turns sidebar scrollable without hover.
        static MainThreadNeverDestroyed<const AtomString> idValue("guide-inner-content"_s);
        if (style.overflowY() == Overflow::Hidden && m_element->idForStyleResolution() == idValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (m_document.quirks().needsWeChatScrollingQuirk()) {
        static MainThreadNeverDestroyed<const AtomString> class1("tree-select"_s);
        static MainThreadNeverDestroyed<const AtomString> class2("v-tree-select"_s);
        const auto& flexBasis = style.flexBasis();
        if (style.minHeight().isAuto()
            && style.display() == DisplayType::Flex
            && style.flexGrow() == 1
            && style.flexShrink() == 1
            && (flexBasis.isPercent() || flexBasis.isFixed())
            && flexBasis.value() == 0
            && const_cast<Element*>(m_element)->classList().contains(class1)
            && const_cast<Element*>(m_element)->classList().contains(class2))
            style.setMinHeight(Length(0, LengthType::Fixed));
    }
#if ENABLE(VIDEO)
    if (m_document.quirks().needsFullscreenDisplayNoneQuirk()) {
        if (is<HTMLDivElement>(m_element) && style.display() == DisplayType::None) {
            static MainThreadNeverDestroyed<const AtomString> instreamNativeVideoDivClass("instream-native-video--mobile"_s);
            static MainThreadNeverDestroyed<const AtomString> videoElementID("vjs_video_3_html5_api"_s);

            auto& div = downcast<HTMLDivElement>(*m_element);
            if (div.hasClass() && div.classNames().contains(instreamNativeVideoDivClass)) {
                auto* video = div.treeScope().getElementById(videoElementID);
                if (is<HTMLVideoElement>(video) && downcast<HTMLVideoElement>(*video).isFullscreen())
                    style.setEffectiveDisplay(DisplayType::Block);
            }
        }
    }
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
        if (bodyStyle && !bodyStyle->effectiveContainment().isEmpty())
            return false;
        return documentElementStyle->effectiveContainment().isEmpty();
    }();

    auto writingMode = [&] {
        // FIXME: The spec says body should win.
        if (documentElementStyle->hasExplicitlySetWritingMode())
            return documentElementStyle->writingMode();
        if (shouldPropagateFromBody && bodyStyle && bodyStyle->hasExplicitlySetWritingMode())
            return bodyStyle->writingMode();
        return RenderStyle::initialWritingMode();
    }();

    auto direction = [&] {
        if (documentElementStyle->hasExplicitlySetDirection())
            return documentElementStyle->direction();
        if (shouldPropagateFromBody && bodyStyle && bodyStyle->hasExplicitlySetDirection())
            return bodyStyle->direction();
        return RenderStyle::initialDirection();
    }();

    // https://drafts.csswg.org/css-writing-modes-3/#icb
    auto& viewStyle = document.renderView()->style();
    if (writingMode != viewStyle.writingMode() || direction != viewStyle.direction()) {
        auto newRootStyle = RenderStyle::clonePtr(viewStyle);
        newRootStyle->setWritingMode(writingMode);
        newRootStyle->setDirection(direction);
        newRootStyle->setColumnStylesFromPaginationMode(document.view()->pagination().mode);
        update.addInitialContainingBlockUpdate(WTFMove(newRootStyle));
    }

    // https://drafts.csswg.org/css-writing-modes-3/#principal-flow
    if (writingMode != documentElementStyle->writingMode() || direction != documentElementStyle->direction()) {
        auto* documentElementUpdate = update.elementUpdate(*document.documentElement());
        if (!documentElementUpdate) {
            update.addElement(*document.documentElement(), nullptr, { RenderStyle::clonePtr(*documentElementStyle) });
            documentElementUpdate = update.elementUpdate(*document.documentElement());
        }
        documentElementUpdate->style->setWritingMode(writingMode);
        documentElementUpdate->style->setDirection(direction);
        documentElementUpdate->change = determineChange(*documentElementStyle, *documentElementUpdate->style);
    }
}

std::unique_ptr<RenderStyle> Adjuster::restoreUsedDocumentElementStyleToComputed(const RenderStyle& style)
{
    if (style.writingMode() == RenderStyle::initialWritingMode() && style.direction() == RenderStyle::initialDirection())
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

}
}
