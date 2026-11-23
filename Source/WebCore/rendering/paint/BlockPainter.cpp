/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "BlockPainter.h"

#include "AccessibilityRegionContext.h"
#include "BorderShape.h"
#include "BoxDecorationPainter.h"
#include "EventRegion.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLTextFormControlElement.h"
#include "InlineWalker.h"
#include "LayoutIntegrationFlexLayout.h"
#include "LayoutIntegrationLineLayout.h"
#include "LineClampUpdater.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Logging.h"
#include "MaskPainter.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PaintInfoInlines.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderMathMLOperator.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderObjectInlines.h"
#include "RenderSVGText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Paint {

BlockPainter::BlockPainter(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_block(block)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
{
}

void BlockPainter::paint()
{
    auto adjustedPaintOffset = m_paintOffset + m_block->location();
    PaintPhase phase = m_paintInfo.phase;

    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView paints the root's background.
    auto visualContentIsClippedOut = [&](LayoutRect paintingRect) {
        if (m_block->isDocumentElementRenderer())
            return false;

        if (m_paintInfo.paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent) && m_block->hasLayer() && m_block->layer()->usesCompositedScrolling())
            return false;

        auto overflowBox = m_block->visualOverflowRect();
        m_block->blockStartRelativeToPhysical(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        return !overflowBox.intersects(paintingRect);
    };

    if (visualContentIsClippedOut(m_paintInfo.rect))
        return;

    bool pushedClip = m_block->pushContentsClip(m_paintInfo, adjustedPaintOffset);
    m_block->paintObject(m_paintInfo, adjustedPaintOffset);
    if (pushedClip)
        m_block->popContentsClip(m_paintInfo, phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (phase == PaintPhase::BlockBackground || phase == PaintPhase::ChildBlockBackground) {
        CheckedPtr layer = m_block->layer();
        if (m_block->hasNonVisibleOverflow() && layer && layer->scrollableArea() && m_block->usedStyle().visibility() == UsedVisibility::Visible
            && m_paintInfo.shouldPaintWithinRoot(m_block) && !m_paintInfo.paintRootBackgroundOnly()) {
            protect(layer->scrollableArea())->paintOverflowControls(m_paintInfo.context(), m_paintInfo.paintBehavior, roundedIntPoint(adjustedPaintOffset), snappedIntRect(m_paintInfo.rect));
        }
    }
}

PaintInfo BlockPainter::paintInfoForBlockChildren(const RenderBlock& block, const PaintInfo& paintInfo)
{
    auto phaseForChildren = [&] {
        switch (paintInfo.phase) {
        case PaintPhase::ChildOutlines:
            return PaintPhase::Outline;
        case PaintPhase::ChildBlockBackgrounds:
            return PaintPhase::ChildBlockBackground;
        default:
            return paintInfo.phase;
        }
    };

    PaintInfo paintInfoForChildren(paintInfo);
    paintInfoForChildren.phase = phaseForChildren();
    paintInfoForChildren.updateSubtreePaintRootForChildren(&block);

    if (paintInfoForChildren.eventRegionContext())
        paintInfoForChildren.paintBehavior.add(PaintBehavior::EventRegionIncludeBackground);

    return paintInfoForChildren;
}

static bool isNonPaintingTraversalPhase(PaintPhase phase)
{
    return phase == PaintPhase::Selection
        || phase == PaintPhase::TextClip
        || phase == PaintPhase::EventRegion
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        || phase == PaintPhase::AXCustomColorComputeBackdrops
        || phase == PaintPhase::AXCustomColorCollectBackgrounds
#endif
        || phase == PaintPhase::Accessibility;
}

void BlockPainter::paintContents(const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(m_block));

    if (m_block->childrenInline())
        paintInlineChildren(paintOffset);
    else {
        PaintInfo paintInfoForChildren = paintInfoForBlockChildren(m_block, m_paintInfo);

        // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
        // NSViews. Do not add any more code for this.
        bool usePrintRect = !m_block->view().printRect().isEmpty();
        BlockPainter { m_block, m_paintInfo, paintOffset }.paintChildren(paintInfoForChildren, usePrintRect);
    }
}

void BlockPainter::paintChildren(PaintInfo& paintInfoForChild, bool usePrintRect)
{
    ASSERT(!isSkippedContentRoot(m_block));

    if (CheckedPtr grid = dynamicDowncast<RenderGrid>(m_block)) {
        ASSERT(!grid->currentGrid().needsItemsPlacement());
        for (CheckedRef gridItem : grid->currentGrid().orderIterator().gridItems())
            paintChild(gridItem, paintInfoForChild, usePrintRect, PaintAsInlineBlock);
        return;
    }

    if (CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(m_block)) {
        for (auto& renderer : flexBox->flexLayout().flexItems()) {
            CheckedPtr flexItem = renderer;
            if (flexItem && !paintChild(*flexItem, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
                return;
        }
        return;
    }

#if ENABLE(MATHML)
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (CheckedPtr mathOperator = dynamicDowncast<RenderMathMLOperator>(m_block); mathOperator && mathOperator->paintsOperatorInsteadOfChildren())
        return;
    if (CheckedPtr token = dynamicDowncast<RenderMathMLToken>(m_block); token && token->hasMathVariantGlyph())
        return;
#endif

    // Painting reaches the floats in the Float phase, after the block backgrounds. The non-painting traversal phases walk the tree
    // once, so they take the floats after the other children.
    auto takesFloatsLast = isNonPaintingTraversalPhase(m_paintInfo.phase);
    for (CheckedRef child : childrenOfType<RenderBox>(m_block.get())) {
        if (takesFloatsLast && child->isFloating())
            continue;
        if (!paintChild(child, paintInfoForChild, usePrintRect))
            return;
    }
    if (!takesFloatsLast)
        return;
    for (CheckedRef child : childrenOfType<RenderBox>(m_block.get())) {
        if (child->isFloating() && !paintChild(child, paintInfoForChild, usePrintRect))
            return;
    }
}

bool BlockPainter::paintChild(RenderBox& child, PaintInfo& paintInfoForChild, bool usePrintRect, PaintBlockType paintType)
{
    ASSERT(!isSkippedContentRoot(m_block));

    if (child.isExcludedAndPlacedInBorder())
        return true;

    if (child.isOutsideListMarker())
        return true;

    if (child.isSkippedContent()) {
        ASSERT(child.isColumnSpanner());
        return true;
    }

    CheckedRef view = m_block->view();

    // Check for page-break-before: always, and if it's set, break and bail.
    bool checkBeforeAlways = !m_block->childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakBefore()));
    LayoutUnit absoluteChildY = m_paintOffset.y() + child.y();
    if (checkBeforeAlways
        && absoluteChildY > m_paintInfo.rect.y()
        && absoluteChildY < m_paintInfo.rect.maxY()) {
        view->setBestTruncatedAt(absoluteChildY, m_block.ptr(), true);
        return false;
    }

    if (!child.isFloating() && child.isBlockLevelReplacedOrAtomicInline() && usePrintRect && child.borderBoxHeight() <= view->printRect().height()) {
        // Paginate block-level replaced elements.
        if (absoluteChildY + child.borderBoxHeight() > view->printRect().maxY()) {
            if (absoluteChildY < view->truncatedAt())
                view->setBestTruncatedAt(absoluteChildY, &child);
            // If we were able to truncate, don't paint.
            if (absoluteChildY >= view->truncatedAt())
                return false;
        }
    }

    LayoutPoint childPoint = m_block->blockStartRelativeToPhysicalForChild(child, m_paintOffset);
    if (!child.hasSelfPaintingLayer()) {
        if (child.isFloating()) {
            // Only a RenderBlockFlow places floats.
            if (is<RenderBlockFlow>(m_block))
                paintFloat(child, childPoint);
        } else {
            GraphicsContextStateSaver fragmentainerStateSaver(paintInfoForChild.context(), false);
            auto paintInfoForFragmentainer = std::optional<PaintInfo> { };
            if (paintInfoForChild.fragmentainer) {
                paintInfoForFragmentainer = child.paintInfoForFragmentainer(paintInfoForChild, childPoint, fragmentainerStateSaver);
                if (!paintInfoForFragmentainer)
                    return true;
            }
            auto& childPaintInfo = paintInfoForFragmentainer ? *paintInfoForFragmentainer : paintInfoForChild;
            if (paintType == PaintAsInlineBlock)
                child.paintAsInlineBlock(childPaintInfo, childPoint);
            else
                child.paint(childPaintInfo, childPoint);
        }
    }

    // Check for page-break-after: always, and if it's set, break and bail.
    bool checkAfterAlways = !m_block->childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakAfter()));
    if (checkAfterAlways
        && (absoluteChildY + child.borderBoxHeight()) > m_paintInfo.rect.y()
        && (absoluteChildY + child.borderBoxHeight()) < m_paintInfo.rect.maxY()) {
        view->setBestTruncatedAt(absoluteChildY + child.borderBoxHeight() + std::max<LayoutUnit>(0, child.collapsedMarginAfter()), m_block.ptr(), true);
        return false;
    }

    return true;
}

void BlockPainter::paintCarets()
{
    if (m_paintInfo.phase == PaintPhase::Foreground) {
        paintCaret(CursorCaret);
        paintCaret(DragCaret);
    }
}

void BlockPainter::paintCaret(CaretType type)
{
    auto shouldPaintCaret = [&](RenderBlock* caretPainter, bool isContentEditable) {
        if (caretPainter != m_block.ptr())
            return false;

        return isContentEditable || m_block->settings().caretBrowsingEnabled();
    };

    Ref frame = m_block->frame();

    switch (type) {
    case CaretType::CursorCaret: {
        CheckedRef selection = frame->selection();
        CheckedPtr caretPainter = selection->caretRendererWithoutUpdatingLayout();
        if (!caretPainter)
            return;

        bool isContentEditable = selection->selection().hasEditableStyle();

        if (shouldPaintCaret(caretPainter.get(), isContentEditable))
            selection->paintCaret(m_paintInfo.context(), m_paintOffset);
        break;
    }
    case CaretType::DragCaret: {
        auto& dragCaretController = m_block->page().dragCaretController();
        CheckedPtr caretPainter = dragCaretController.caretRenderer();
        if (!caretPainter)
            return;

        bool isContentEditable = dragCaretController.isContentEditable();
        if (shouldPaintCaret(caretPainter.get(), isContentEditable))
            dragCaretController.paintDragCaret(frame.ptr(), m_paintInfo.context(), m_paintOffset);

        break;
    }
    }
}

static void paintDebugBoxShadowIfApplicable(const RenderBlock& block, GraphicsContext& context, const LayoutRect& paintRect)
{
    // FIXME: Use a more generic, modern-layout wide setting instead.
    if (!block.settings().legacyLineLayoutVisualCoverageEnabled())
        return;

    if (!is<RenderFlexibleBox>(block))
        return;

    constexpr size_t shadowExtent = 3;
    GraphicsContextStateSaver stateSaver(context);

    auto shadowRect = paintRect;
    shadowRect.inflate(shadowExtent);
    context.clip(shadowRect);
    context.setDropShadow({ { -shadowRect.width(), 0 }, 30, SRGBA<uint8_t> { 0, 180, 230, 200 }, ShadowRadiusMode::Default });
    context.clipOut(paintRect);
    shadowRect.move(shadowRect.width(), 0);
    context.fillRect(shadowRect, Color::black);
}

void BlockPainter::paintObject()
{
    PaintPhase paintPhase = m_paintInfo.phase;
    auto shouldPaintContent = !isSkippedContentRoot(m_block);

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && m_block->usedStyle().visibility() == UsedVisibility::Visible) {
        if (m_block->hasVisibleBoxDecorations())
            BoxDecorationPainter { m_block, m_paintInfo, m_paintOffset }.paintBoxDecorations();
        paintDebugBoxShadowIfApplicable(m_block, m_paintInfo.context(), { m_paintOffset, m_block->borderBoxSize() });
    }

    // Paint legends just above the border before we scroll or clip.
    if (shouldPaintContent && (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground || paintPhase == PaintPhase::Selection))
        paintExcludedChildrenInBorder();

    if (paintPhase == PaintPhase::Mask && m_block->usedStyle().visibility() == UsedVisibility::Visible) {
        MaskPainter { m_block, m_paintInfo, m_paintOffset }.paintMask();
        return;
    }

    if (paintPhase == PaintPhase::ClippingMask && m_block->usedStyle().visibility() == UsedVisibility::Visible) {
        MaskPainter { m_block, m_paintInfo, m_paintOffset }.paintClippingMask();
        return;
    }

    // If just painting the root background, then return.
    if (m_paintInfo.paintRootBackgroundOnly())
        return;

    if (paintPhase == PaintPhase::Accessibility)
        protect(m_paintInfo.accessibilityRegionContext())->takeBounds(m_block, m_paintOffset);

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (paintPhase == PaintPhase::AXCustomColorCollectBackgrounds)
        protect(m_paintInfo.axCustomColorBackdropContext())->recordBackdrop(m_block, FloatRect { LayoutRect(m_paintOffset, m_block->borderBoxSize()) }, m_paintInfo.paintBehavior);
#endif

    if (paintPhase == PaintPhase::EventRegion) {
        auto borderRect = LayoutRect(m_paintOffset, m_block->borderBoxSize());

        Ref document = m_block->document();
        CheckedPtr eventRegionContext = m_paintInfo.eventRegionContext();
        if (m_paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeBackground) && m_block->visibleToHitTesting()) {
            auto borderShape = BorderShape::shapeForBorderRect(m_block->usedStyle(), borderRect);
            LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << m_block.get() << " uniting region " << borderShape.deprecatedRoundedRect() << " event listener types " << m_block->style().eventListenerRegionTypes());
            bool overrideUserModifyIsEditable = m_block->isRenderTextControl() && protect(downcast<RenderTextControl>(m_block).textFormControlElement())->isInnerTextElementEditable();
            eventRegionContext->unite(borderShape.deprecatedPixelSnappedRoundedRect(document->deviceScaleFactor()), m_block, protect(m_block->style()), overrideUserModifyIsEditable);
        }

        if (!m_paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeForeground))
            return;

        bool needsTraverseDescendants = m_block->hasVisualOverflow() || m_block->containsFloats() || !eventRegionContext->contains(enclosingIntRect(borderRect)) || m_block->view().needsEventRegionUpdateForNonCompositedFrame();
        LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << m_block.get() << " needsTraverseDescendants for event region: hasVisualOverflow: " << m_block->hasVisualOverflow() << " containsFloats: "
            << m_block->containsFloats() <<  " border box is outside current region: " << !eventRegionContext->contains(enclosingIntRect(borderRect)) << " needsEventRegionUpdateForNonCompositedFrame:" << m_block->view().needsEventRegionUpdateForNonCompositedFrame());
#if ENABLE(TOUCH_ACTION_REGIONS)
        needsTraverseDescendants |= document->mayHaveElementsWithNonAutoTouchAction();
        LOG_WITH_STREAM(EventRegions, stream << "  may have touch-action elements: " << document->mayHaveElementsWithNonAutoTouchAction());
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
        needsTraverseDescendants |= document->hasWheelEventHandlers() || document->mayHaveListBoxes();
        LOG_WITH_STREAM(EventRegions, stream << "  has wheel event handlers: " << document->hasWheelEventHandlers() << " may have list boxes: " << document->mayHaveListBoxes());
#endif

#if ENABLE(EDITABLE_REGION)
        // We treat the entire text control as editable to match users' expectation even
        // though it's actually the inner text element of the control that is editable.
        // So, no need to traverse to find the inner text element in this case.
        if (!m_block->isRenderTextControl()) {
            needsTraverseDescendants |= document->mayHaveEditableElements() && protect(m_block->page())->shouldBuildEditableRegion();
            LOG_WITH_STREAM(EventRegions, stream << "  needs editable event region: " << (document->mayHaveEditableElements() && m_block->page().shouldBuildEditableRegion()));
        }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        needsTraverseDescendants |= m_block->page().shouldBuildInteractionRegions();
#endif

#if ENABLE(TOUCH_EVENT_REGIONS)
        ASSERT_IMPLIES(!needsTraverseDescendants, !m_block->hasVisualOverflow() && !m_block->containsFloats());

        // Skip traversing descendants of this block when building event regions if:
        //
        // 1. This block has no visual overflow or floats, so all descendants paint inside its
        //    border box.
        // 2. No descendant is a touch event target (e.g. an element with its own touch listener).
        //    Since listener types are inherited, every descendant has this block's touch listener
        //    types, and we don't have to traverse descendants to discover differing types.
        // 3. The touch listener regions already cover the clipped border box for those types. By
        //    (1) and (2), they then also cover each descendant's touch rect, so we don't have to
        //    traverse descendants to discover their touch rects.
        if (!needsTraverseDescendants && document->shouldUseTouchEventRegions() && document->hasTouchEventHandlers()) {
            needsTraverseDescendants = eventRegionContext->mayHaveDescendantTouchEventTargets(m_block)
                || !eventRegionContext->touchEventListenerRegionsContain(enclosingIntRect(borderRect), m_block->style().eventListenerRegionTypes());
            LOG_WITH_STREAM(EventRegions, stream << "  needs to traverse descendants for touch event listeners: " << needsTraverseDescendants);
        }
#endif

        if (!needsTraverseDescendants)
            return;
    }

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = m_paintOffset;
    scrolledOffset.moveBy(-m_block->scrollPosition());

    // Column rules need to account for scrolling and clipping.
    // FIXME: Clipping of column rules does not work. We will need a separate paint phase for column rules I suspect in order to get
    // clipping correct (since it has to paint as background but is still considered "contents").
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && m_block->usedStyle().visibility() == UsedVisibility::Visible)
        paintColumnRules(scrolledOffset);

    // Done with backgrounds, borders and column rules.
    if (paintPhase == PaintPhase::BlockBackground)
        return;

    // 2. paint contents
    if (shouldPaintContent) {
        if (paintPhase != PaintPhase::SelfOutline)
            paintContents(scrolledOffset);
    }

    // 3. paint selection
    if (!m_block->document().printing()) {
        // Fill in gaps in selection on lines, between blocks and "empty space" when content is skipped.
        m_block->paintSelection(m_paintInfo, scrolledOffset);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && m_block->hasOutline() && m_block->usedStyle().visibility() == UsedVisibility::Visible) {
        m_block->paintDecorationsPerFragment(m_paintInfo, m_paintOffset, [&](auto& rect) {
            m_block->paintOutline(m_paintInfo, rect);
        });
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhase::Foreground,
    // then paint the caret.
    if (shouldPaintContent)
        paintCarets();
}

void BlockPainter::paintExcludedChildrenInBorder()
{
    if (!m_block->isFieldset())
        return;

    CheckedPtr box = m_block->findFieldsetLegend();
    if (!box || !box->isExcludedFromNormalLayout() || box->hasSelfPaintingLayer())
        return;

    LayoutPoint childPoint = m_block->blockStartRelativeToPhysicalForChild(*box, m_paintOffset);
    box->paintAsInlineBlock(m_paintInfo, childPoint);
}

void BlockPainter::paintColumnRules(const LayoutPoint& paintOffset)
{
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(m_block);
    if (!blockFlow || !blockFlow->multiColumnFlow() || m_paintInfo.context().paintingDisabled())
        return;

    // Iterate over our children and paint the column rules as needed.
    for (CheckedRef columnSet : childrenOfType<RenderMultiColumnSet>(*blockFlow)) {
        LayoutPoint childPoint = columnSet->location() + blockFlow->blockStartRelativeToPhysicalForChild(columnSet, paintOffset);
        columnSet->paintColumnRules(m_paintInfo, childPoint);
    }
}

void BlockPainter::paintFloat(RenderBox& floatBox, const LayoutPoint& floatBoxLocation)
{
    ASSERT(floatBox.isFloating() && !floatBox.hasSelfPaintingLayer());

    auto preservePhase = isNonPaintingTraversalPhase(m_paintInfo.phase);
    if (m_paintInfo.phase != PaintPhase::Float && !preservePhase)
        return;
    // Layout skips the floats that line-clamp hides after the clamp point.
    if (floatBox.isForceHiddenByLineClamp())
        return;
    GraphicsContextStateSaver fragmentainerStateSaver(m_paintInfo.context(), false);
    auto paintInfoForFragmentainer = std::optional<PaintInfo> { };
    if (m_paintInfo.fragmentainer) {
        paintInfoForFragmentainer = floatBox.paintInfoForFragmentainer(m_paintInfo, floatBoxLocation, fragmentainerStateSaver);
        if (!paintInfoForFragmentainer)
            return;
    }
    auto& paintInfo = paintInfoForFragmentainer ? *paintInfoForFragmentainer : m_paintInfo;

    GraphicsContextStateSaver stateSaver(paintInfo.context(), false);

    auto clipBlockEndIfNeeded = [&] {
        // The clip rect in the float's own coordinates, moved to where the float's border box paints.
        auto clipRect = LineClampUpdater::blockEndClipRect(floatBox, floatBox);
        if (!clipRect)
            return;
        clipRect->moveBy(floatBoxLocation + floatBox.locationOffset());
        // Like the layer clip rects, this is within the area being painted (and so it is finite, which the conversion to float needs).
        clipRect->intersect(paintInfo.rect);
        stateSaver.save();
        paintInfo.context().clip(*clipRect);
    };
    clipBlockEndIfNeeded();

    if (preservePhase) {
        floatBox.paint(paintInfo, floatBoxLocation);
        return;
    }
    // "Paint a stacking context given |root| and |canvas|, treating |root| as if it created a new stacking context" (css-position-4, paint a stacking container).
    auto paintInfoForFloat = PaintInfo { paintInfo };

    paintInfoForFloat.phase = PaintPhase::BlockBackground;
    floatBox.paint(paintInfoForFloat, floatBoxLocation);

    paintInfoForFloat.phase = PaintPhase::ChildBlockBackgrounds;
    floatBox.paint(paintInfoForFloat, floatBoxLocation);

    paintInfoForFloat.phase = PaintPhase::Float;
    floatBox.paint(paintInfoForFloat, floatBoxLocation);

    paintInfoForFloat.phase = PaintPhase::Foreground;
    floatBox.paint(paintInfoForFloat, floatBoxLocation);

    paintInfoForFloat.phase = PaintPhase::Outline;
    floatBox.paint(paintInfoForFloat, floatBoxLocation);
}

void BlockPainter::paintInlineChildren(const LayoutPoint& paintOffset)
{
    ASSERT(m_block->childrenInline());

    if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(m_block)) {
        svgText->paintInlineChildren(m_paintInfo, paintOffset);
        return;
    }

    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(m_block);
    if (!blockFlow)
        return;

    CheckedPtr inlineLayout = blockFlow->inlineLayout();
    if (!inlineLayout)
        return;

    if (m_paintInfo.phase == PaintPhase::Float && inlineLayout->hasFloats()) {
        BlockPainter { m_block, m_paintInfo, paintOffset }.paintFloatsAndBlockLevelContentInInline(*inlineLayout);
        return;
    }

    inlineLayout->paint(m_paintInfo, paintOffset);

    if (isNonPaintingTraversalPhase(m_paintInfo.phase) && inlineLayout->hasFloats()) {
        for (auto& child : inlineLayout->floatsAndBlockLevelBoxesInTreeOrder()) {
            if (child->isFloating() && !child->hasSelfPaintingLayer())
                paintFloat(child, blockFlow->blockStartRelativeToPhysicalForChild(child, paintOffset));
        }
    }
}

void BlockPainter::paintBlockLevelContentInInline()
{
    ASSERT(m_block->childrenInline());

    PaintInfo paintInfoForChildren = paintInfoForBlockChildren(m_block, m_paintInfo);

    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(m_block)); !walker.atEnd(); walker.advance()) {
        CheckedPtr child = dynamicDowncast<RenderBox>(walker.current());
        // paintInlineChildren() paints the floats.
        if (!child || child->isFloating())
            continue;
        if (!paintChild(*child, paintInfoForChildren, false))
            return;
    }
}

void BlockPainter::paintFloatsAndBlockLevelContentInInline(const LayoutIntegration::LineLayout& inlineLayout)
{
    ASSERT(m_paintInfo.phase == PaintPhase::Float);

    PaintInfo paintInfoForChildren = paintInfoForBlockChildren(m_block, m_paintInfo);

    for (auto& child : inlineLayout.floatsAndBlockLevelBoxesInTreeOrder()) {
        // Like InlineContentPainter.
        if (!child->isFloating() && !m_paintInfo.shouldPaintWithinRoot(child))
            continue;
        paintChild(child, paintInfoForChildren, false);
    }
}

} // namespace Paint
} // namespace WebCore
