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
#include "StyleAdjuster.h"

#include "AnimationBase.h"
#include "CSSFontSelector.h"
#include "DOMWindow.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLMarqueeElement.h"
#include "HTMLNames.h"
#include "HTMLSlotElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "MathMLElement.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocument.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "Text.h"

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

static inline bool isAtShadowBoundary(const Element& element)
{
    auto* parentNode = element.parentNode();
    return parentNode && parentNode->isShadowRoot();
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
    auto regionTypes = computeEventListenerRegionTypes(document, { });
    if (auto* window = document.domWindow())
        regionTypes.add(computeEventListenerRegionTypes(*window, { }));

    rootStyle.setEventListenerRegionTypes(regionTypes);
}

OptionSet<EventListenerRegionType> Adjuster::computeEventListenerRegionTypes(const EventTarget& eventTarget, OptionSet<EventListenerRegionType> parentTypes)
{
#if !PLATFORM(IOS_FAMILY)
    if (!eventTarget.hasEventListeners())
        return parentTypes;

    auto types = parentTypes;

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

    findListeners(eventNames().wheelEvent, EventListenerRegionType::Wheel, EventListenerRegionType::NonPassiveWheel);
    findListeners(eventNames().mousewheelEvent, EventListenerRegionType::Wheel, EventListenerRegionType::NonPassiveWheel);

    return types;
#else
    UNUSED_PARAM(eventTarget);
    UNUSED_PARAM(parentTypes);
    return { };
#endif
}

void Adjuster::adjust(RenderStyle& style, const RenderStyle* userAgentAppearanceStyle) const
{
    // Cache our original display.
    style.setOriginalDisplay(style.display());

    if (style.display() == DisplayType::Contents)
        adjustDisplayContentsStyle(style);

    if (style.display() != DisplayType::None && style.display() != DisplayType::Contents) {
        if (m_element) {
            // If we have a <td> that specifies a float property, in quirks mode we just drop the float
            // property.
            // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
            // these tags to retain their display types.
            if (m_document.inQuirksMode()) {
                if (m_element->hasTagName(tdTag)) {
                    style.setDisplay(DisplayType::TableCell);
                    style.setFloating(Float::No);
                } else if (is<HTMLTableElement>(*m_element))
                    style.setDisplay(style.isDisplayInlineType() ? DisplayType::InlineTable : DisplayType::Table);
            }

            if (m_element->hasTagName(tdTag) || m_element->hasTagName(thTag)) {
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
            if (is<HTMLTableElement>(*m_element) && (style.textAlign() == TextAlignMode::WebKitLeft || style.textAlign() == TextAlignMode::WebKitCenter || style.textAlign() == TextAlignMode::WebKitRight))
                style.setTextAlign(TextAlignMode::Start);

            // Frames and framesets never honor position:relative or position:absolute. This is necessary to
            // fix a crash where a site tries to position these objects. They also never honor display.
            if (m_element->hasTagName(frameTag) || m_element->hasTagName(framesetTag)) {
                style.setPosition(PositionType::Static);
                style.setDisplay(DisplayType::Block);
            }

            // Ruby text does not support float or position. This might change with evolution of the specification.
            if (m_element->hasTagName(rtTag)) {
                style.setPosition(PositionType::Static);
                style.setFloating(Float::No);
            }

            // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
            // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
            // just a single declaration that sets the 'text-align' property to the value 'center'.
            // https://html.spec.whatwg.org/multipage/rendering.html#rendering
            if (m_element->hasTagName(thTag) && !style.hasExplicitlySetTextAlign() && m_parentStyle.textAlign() == RenderStyle::initialTextAlign())
                style.setTextAlign(TextAlignMode::Center);

            if (m_element->hasTagName(legendTag))
                style.setDisplay(DisplayType::Block);
        }

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (m_element && m_document.documentElement() == m_element))
            style.setDisplay(equivalentBlockDisplay(style, m_document));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == DisplayType::Inline && style.styleType() == PseudoId::None && style.writingMode() != m_parentStyle.writingMode())
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
            style.setWritingMode(m_parentStyle.writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style.writingMode() != TopToBottomWritingMode && (style.display() == DisplayType::Box || style.display() == DisplayType::InlineBox))
            style.setWritingMode(TopToBottomWritingMode);

        // https://www.w3.org/TR/css-display/#transformations
        // "A parent with a grid or flex display value blockifies the box’s display type."
        if (m_parentBoxStyle.isDisplayFlexibleOrGridBox()) {
            style.setFloating(Float::No);
            style.setDisplay(equivalentBlockDisplay(style, m_document));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style.hasAutoSpecifiedZIndex() || (style.position() == PositionType::Static && !m_parentBoxStyle.isDisplayFlexibleOrGridBox()))
        style.setHasAutoUsedZIndex();
    else
        style.setUsedZIndex(style.specifiedZIndex());

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoUsedZIndex()) {
        if ((m_element && m_document.documentElement() == m_element)
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
            style.setUsedZIndex(0);
    }

    if (m_element) {
        // Textarea considers overflow visible as auto.
        if (is<HTMLTextAreaElement>(*m_element)) {
            style.setOverflowX(style.overflowX() == Overflow::Visible ? Overflow::Auto : style.overflowX());
            style.setOverflowY(style.overflowY() == Overflow::Visible ? Overflow::Auto : style.overflowY());
        }

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
                style.setHeight(Length(200, Fixed));
        }
    }

    if (doesNotInheritTextDecoration(style, m_element))
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

    // Menulists should have visible overflow
    if (style.appearance() == MenulistPart) {
        style.setOverflowX(Overflow::Visible);
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

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (is<HTMLFormControlElement>(m_element) && style.computedFontPixelSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!is<HTMLInputElement>(*m_element) || !downcast<HTMLInputElement>(*m_element).isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        RenderTheme::singleton().adjustStyle(style, m_element, userAgentAppearanceStyle);

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

    if (is<SVGElement>(m_element))
        adjustSVGElementStyle(style, downcast<SVGElement>(*m_element));

    // If the inherited value of justify-items includes the 'legacy' keyword (plus 'left', 'right' or
    // 'center'), 'legacy' computes to the the inherited value. Otherwise, 'auto' computes to 'normal'.
    if (m_parentBoxStyle.justifyItems().positionType() == ItemPositionType::Legacy && style.justifyItems().position() == ItemPosition::Legacy)
        style.setJustifyItems(m_parentBoxStyle.justifyItems());

    style.setEffectiveTouchActions(computeEffectiveTouchActions(style, m_parentStyle.effectiveTouchActions()));

    if (m_element)
        style.setEventListenerRegionTypes(computeEventListenerRegionTypes(*m_element, m_parentStyle.eventListenerRegionTypes()));

#if ENABLE(TEXT_AUTOSIZING)
    if (m_element && m_document.settings().textAutosizingUsesIdempotentMode())
        adjustForTextAutosizing(style, *m_element);
#endif

    adjustForSiteSpecificQuirks(style);
}

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

void Adjuster::adjustDisplayContentsStyle(RenderStyle& style) const
{
    if (!m_element) {
        if (style.styleType() != PseudoId::Before && style.styleType() != PseudoId::After)
            style.setDisplay(DisplayType::None);
        return;
    }

    if (m_document.documentElement() == m_element) {
        style.setDisplay(DisplayType::Block);
        return;
    }

    if (hasEffectiveDisplayNoneForDisplayContents(*m_element))
        style.setDisplay(DisplayType::None);
}

void Adjuster::adjustSVGElementStyle(RenderStyle& style, const SVGElement& svgElement)
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

void Adjuster::adjustAnimatedStyle(RenderStyle& style, const RenderStyle* parentBoxStyle, OptionSet<AnimationImpact> impact)
{
    // Set an explicit used z-index in two cases:
    // 1. When the element respects z-index, and the style has an explicit z-index set (for example, the animation
    //    itself may animate z-index).
    // 2. When we want the stacking context side-effets of explicit z-index, via forceStackingContext.
    // It's important to not clobber an existing used z-index, since an earlier animation may have set it, but we
    // may still need to update the used z-index value from the specified value.
    bool elementRespectsZIndex = style.position() != PositionType::Static || (parentBoxStyle && parentBoxStyle->isDisplayFlexibleOrGridBox());

    if (elementRespectsZIndex && !style.hasAutoSpecifiedZIndex())
        style.setUsedZIndex(style.specifiedZIndex());
    else if (impact.contains(AnimationImpact::ForcesStackingContext))
        style.setUsedZIndex(0);
}

void Adjuster::adjustForSiteSpecificQuirks(RenderStyle& style) const
{
    if (!m_element)
        return;

    if (m_document.quirks().needsGMailOverflowScrollQuirk()) {
        // This turns sidebar scrollable without mouse move event.
        static MainThreadNeverDestroyed<const AtomString> roleValue("navigation", AtomString::ConstructFromLiteral);
        if (style.overflowY() == Overflow::Hidden && m_element->attributeWithoutSynchronization(roleAttr) == roleValue)
            style.setOverflowY(Overflow::Auto);
    }
    if (m_document.quirks().needsYouTubeOverflowScrollQuirk()) {
        // This turns sidebar scrollable without hover.
        static MainThreadNeverDestroyed<const AtomString> idValue("guide-inner-content", AtomString::ConstructFromLiteral);
        if (style.overflowY() == Overflow::Hidden && m_element->idForStyleResolution() == idValue)
            style.setOverflowY(Overflow::Auto);
    }
#if ENABLE(VIDEO)
    if (m_document.quirks().needsFullscreenDisplayNoneQuirk()) {
        if (is<HTMLDivElement>(m_element) && style.display() == DisplayType::None) {
            static MainThreadNeverDestroyed<const AtomString> instreamNativeVideoDivClass("instream-native-video--mobile", AtomString::ConstructFromLiteral);
            static MainThreadNeverDestroyed<const AtomString> videoElementID("vjs_video_3_html5_api", AtomString::ConstructFromLiteral);

            auto& div = downcast<HTMLDivElement>(*m_element);
            if (div.hasClass() && div.classNames().contains(instreamNativeVideoDivClass)) {
                auto* video = div.treeScope().getElementById(videoElementID);
                if (is<HTMLVideoElement>(video) && downcast<HTMLVideoElement>(*video).isFullscreen())
                    style.setDisplay(DisplayType::Block);
            }
        }
    }
#endif
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
        style.setLineHeight({ *newLineHeight, Fixed });
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
