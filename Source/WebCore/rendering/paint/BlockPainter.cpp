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
#include "PaintOrderIterator.h"
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

bool BlockPainter::isExcludedFromNormalFlowPainting(const RenderBox& child)
{
    if (child.isExcludedAndPlacedInBorder() || child.isOutsideListMarker())
        return true;
    if (child.isSkippedContent()) {
        ASSERT(child.isColumnSpanner());
        return true;
    }
    return false;
}

bool BlockPainter::isNonPaintingTraversalPhase(PaintPhase phase)
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

void BlockPainter::adjustPaintInfoForBlockChildren(const RenderBlock& block, PaintInfo& paintInfo)
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

    paintInfo.phase = phaseForChildren();
    paintInfo.updateSubtreePaintRootForChildren(&block);

    if (paintInfo.eventRegionContext())
        paintInfo.paintBehavior.add(PaintBehavior::EventRegionIncludeBackground);
}

bool BlockPainter::isVisualContentClippedOut(const RenderBlock& block, const PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset)
{
    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView paints the root's background.
    if (block.isDocumentElementRenderer())
        return false;

    if (paintInfo.paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent) && block.hasLayer() && block.layer()->usesCompositedScrolling())
        return false;

    auto overflowBox = block.visualOverflowRect();
    block.blockStartRelativeToPhysical(overflowBox);
    overflowBox.moveBy(adjustedPaintOffset);
    return !overflowBox.intersects(paintInfo.rect);
}

void BlockPainter::paintOverflowControls(const RenderBlock& block, const PaintInfo& paintInfo, PaintPhase phase, const LayoutPoint& adjustedPaintOffset)
{
    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (phase != PaintPhase::BlockBackground && phase != PaintPhase::ChildBlockBackground)
        return;
    CheckedPtr layer = block.layer();
    if (block.hasNonVisibleOverflow() && layer && layer->scrollableArea() && block.usedStyle().visibility() == UsedVisibility::Visible
        && paintInfo.shouldPaintWithinRoot(block) && !paintInfo.paintRootBackgroundOnly()) {
        protect(layer->scrollableArea())->paintOverflowControls(paintInfo.context(), paintInfo.paintBehavior, roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
    }
}

BlockPainter::BlockPainter(RenderBlock& root, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_root(root)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
{
}

void BlockPainter::pushParent(const RenderBlock* parentBlock)
{
    std::optional<PaintInfoState> savedPaintInfoState;
    if (parentBlock) {
        savedPaintInfoState = PaintInfoState { m_paintInfo.phase, m_paintInfo.subtreePaintRoot, m_paintInfo.paintBehavior };
        adjustPaintInfoForBlockChildren(*parentBlock, m_paintInfo);
    }
    m_parentStack.append({ WTF::move(savedPaintInfoState), m_paintInfo.phase });
}

void BlockPainter::popParent()
{
    auto parent = m_parentStack.takeLast();
    if (auto& state = parent.savedPaintInfoState) {
        m_paintInfo.phase = state->phase;
        m_paintInfo.subtreePaintRoot = state->subtreePaintRoot.get();
        m_paintInfo.paintBehavior = state->paintBehavior;
    }
}

void BlockPainter::paintSubtree()
{
    using Event = PaintOrderIterator::Event;

    auto& paintInfo = m_paintInfo;
    // The non-painting traversal phases walk the tree once, so they take the floats after the other children.
    auto floatOrder = isNonPaintingTraversalPhase(paintInfo.phase) ? PaintOrderIterator::FloatOrder::AfterOtherChildren : PaintOrderIterator::FloatOrder::TreeOrder;
    for (auto iterator = PaintOrderIterator { m_root, m_paintOffset, PaintOrderIterator::Direction::Forward, floatOrder }; !iterator.atEnd(); iterator.traverseNext()) {
        CheckedRef block = iterator.block();
        auto paintOffset = iterator.paintOffset();

        if (iterator.event() == Event::EnterBlock) {
            ASSERT(m_parentStack.size() == iterator.depth() - 1);
            CheckedPtr enclosingBlock = iterator.enclosingBlock();
            if (enclosingBlock && isExcludedFromNormalFlowPainting(block)) {
                iterator.skipBlock();
                continue;
            }
            pushParent(enclosingBlock.get());
            if (isVisualContentClippedOut(block, paintInfo, paintOffset)) {
                popParent();
                iterator.skipBlock();
                continue;
            }
            parent().pushedClip = block->pushContentsClip(paintInfo, paintOffset);
            continue;
        }

        ASSERT(m_parentStack.size() == iterator.depth());

        switch (iterator.event()) {
        case Event::EnterBlock:
            ASSERT_NOT_REACHED();
            break;
        case Event::BeforeContents:
            switch (paintBeforeContents(block, paintInfo, paintOffset)) {
            case ContentsAction::Paint:
                break;
            case ContentsAction::Skip:
                iterator.skipContents();
                break;
            case ContentsAction::Stop:
                iterator.skipToLeave();
                break;
            }
            break;
        case Event::InlineContent:
            // Inline content gets the block's own paint info. It adjusts it for blocks-in-inline itself.
            paintInlineChildren(block, paintInfo, iterator.scrolledOffset());
            break;
        case Event::Children: {
            auto paintInfoForChildren = paintInfoForBlockChildren(block, paintInfo);
            paintChildren(block, paintInfo, iterator.scrolledOffset(), paintInfoForChildren);
            break;
        }
        case Event::Child: {
            CheckedRef child = iterator.child();
            if (isExcludedFromNormalFlowPainting(child))
                break;
            if (child->isFloating()) {
                // Only a RenderBlockFlow places floats.
                if (is<RenderBlockFlow>(block))
                    paintFloat(paintInfo, child, iterator.childPaintOffset());
                break;
            }
            auto paintInfoForChild = paintInfoForBlockChildren(block, paintInfo);
            child->paint(paintInfoForChild, iterator.childPaintOffset());
            break;
        }
        case Event::AfterContents:
            paintAfterContents(block, paintInfo, paintOffset);
            break;
        case Event::LeaveBlock:
            if (parent().pushedClip)
                block->popContentsClip(paintInfo, parent().phase, paintOffset);
            paintOverflowControls(block, paintInfo, parent().phase, paintOffset);
            popParent();
            break;
        }
    }
    ASSERT(m_parentStack.isEmpty());
}

void BlockPainter::paint(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (PaintOrderIterator::canIterate(block)) {
        BlockPainter { block, paintInfo, paintOffset }.paintSubtree();
        return;
    }

    auto adjustedPaintOffset = paintOffset + block.location();
    if (isVisualContentClippedOut(block, paintInfo, adjustedPaintOffset))
        return;

    auto phase = paintInfo.phase;
    bool pushedClip = block.pushContentsClip(paintInfo, adjustedPaintOffset);
    block.paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        block.popContentsClip(paintInfo, phase, adjustedPaintOffset);

    paintOverflowControls(block, paintInfo, phase, adjustedPaintOffset);
}


PaintInfo BlockPainter::paintInfoForBlockChildren(const RenderBlock& block, const PaintInfo& paintInfo)
{
    PaintInfo paintInfoForChildren(paintInfo);
    adjustPaintInfoForBlockChildren(block, paintInfoForChildren);
    return paintInfoForChildren;
}

void BlockPainter::paintContents(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset)
{
    ASSERT(!isSkippedContentRoot(block));

    if (block.childrenInline())
        paintInlineChildren(block, paintInfo, scrolledOffset);
    else {
        PaintInfo paintInfoForChildren = paintInfoForBlockChildren(block, paintInfo);
        paintChildren(block, paintInfo, scrolledOffset, paintInfoForChildren);
    }
}

void BlockPainter::paintChildren(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset, PaintInfo& paintInfoForChild)
{
    ASSERT(!isSkippedContentRoot(block));

    if (CheckedPtr grid = dynamicDowncast<RenderGrid>(block)) {
        ASSERT(!grid->currentGrid().needsItemsPlacement());
        for (CheckedRef gridItem : grid->currentGrid().orderIterator().gridItems())
            paintChild(block, paintInfo, scrolledOffset, gridItem, paintInfoForChild, PaintAsInlineBlock);
        return;
    }

    if (CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(block)) {
        for (auto& renderer : flexBox->flexLayout().flexItems()) {
            CheckedPtr flexItem = renderer;
            if (flexItem)
                paintChild(block, paintInfo, scrolledOffset, *flexItem, paintInfoForChild, PaintAsInlineBlock);
        }
        return;
    }

#if ENABLE(MATHML)
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (CheckedPtr mathOperator = dynamicDowncast<RenderMathMLOperator>(block); mathOperator && mathOperator->paintsOperatorInsteadOfChildren())
        return;
    if (CheckedPtr token = dynamicDowncast<RenderMathMLToken>(block); token && token->hasMathVariantGlyph())
        return;
#endif

    // Painting reaches the floats in the Float phase, after the block backgrounds. The non-painting traversal phases walk the tree
    // once, so they take the floats after the other children.
    auto takesFloatsLast = isNonPaintingTraversalPhase(paintInfo.phase);
    for (CheckedRef child : childrenOfType<RenderBox>(block)) {
        if (takesFloatsLast && child->isFloating())
            continue;
        paintChild(block, paintInfo, scrolledOffset, child, paintInfoForChild);
    }
    if (!takesFloatsLast)
        return;
    for (CheckedRef child : childrenOfType<RenderBox>(block)) {
        if (child->isFloating())
            paintChild(block, paintInfo, scrolledOffset, child, paintInfoForChild);
    }
}

void BlockPainter::paintChild(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset, RenderBox& child, PaintInfo& paintInfoForChild, PaintBlockType paintType)
{
    ASSERT(!isSkippedContentRoot(block));

    if (isExcludedFromNormalFlowPainting(child))
        return;

    LayoutPoint childPoint = block.blockStartRelativeToPhysicalForChild(child, scrolledOffset);
    if (!child.hasSelfPaintingLayer()) {
        if (child.isFloating()) {
            // Only a RenderBlockFlow places floats.
            if (is<RenderBlockFlow>(block))
                paintFloat(paintInfo, child, childPoint);
        } else if (paintType == PaintAsInlineBlock)
            child.paintAsInlineBlock(paintInfoForChild, childPoint);
        else
            child.paint(paintInfoForChild, childPoint);
    }
}

void BlockPainter::paintCarets(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase == PaintPhase::Foreground) {
        paintCaret(block, paintInfo, paintOffset, CursorCaret);
        paintCaret(block, paintInfo, paintOffset, DragCaret);
    }
}

void BlockPainter::paintCaret(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset, CaretType type)
{
    auto shouldPaintCaret = [&](RenderBlock* caretPainter, bool isContentEditable) {
        if (caretPainter != &block)
            return false;

        return isContentEditable || block.settings().caretBrowsingEnabled();
    };

    Ref frame = block.frame();

    switch (type) {
    case CaretType::CursorCaret: {
        CheckedRef selection = frame->selection();
        CheckedPtr caretPainter = selection->caretRendererWithoutUpdatingLayout();
        if (!caretPainter)
            return;

        bool isContentEditable = selection->selection().hasEditableStyle();

        if (shouldPaintCaret(caretPainter.get(), isContentEditable))
            selection->paintCaret(paintInfo.context(), paintOffset);
        break;
    }
    case CaretType::DragCaret: {
        auto& dragCaretController = block.page().dragCaretController();
        CheckedPtr caretPainter = dragCaretController.caretRenderer();
        if (!caretPainter)
            return;

        bool isContentEditable = dragCaretController.isContentEditable();
        if (shouldPaintCaret(caretPainter.get(), isContentEditable))
            dragCaretController.paintDragCaret(frame.ptr(), paintInfo.context(), paintOffset);

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

void BlockPainter::paintObject(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto action = paintBeforeContents(block, paintInfo, paintOffset);
    if (action == ContentsAction::Stop)
        return;
    if (action == ContentsAction::Paint)
        paintContents(block, paintInfo, scrolledPaintOffset(block, paintOffset));
    paintAfterContents(block, paintInfo, paintOffset);
}

LayoutPoint BlockPainter::scrolledPaintOffset(const RenderBlock& block, const LayoutPoint& paintOffset)
{
    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    scrolledOffset.moveBy(-block.scrollPosition());
    return scrolledOffset;
}

BlockPainter::ContentsAction BlockPainter::paintBeforeContents(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    auto shouldPaintContent = !isSkippedContentRoot(block);

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && block.usedStyle().visibility() == UsedVisibility::Visible) {
        if (block.hasVisibleBoxDecorations())
            BoxDecorationPainter { block, paintInfo, paintOffset }.paintBoxDecorations();
        paintDebugBoxShadowIfApplicable(block, paintInfo.context(), { paintOffset, block.borderBoxSize() });
    }

    // Paint legends just above the border before we scroll or clip.
    if (shouldPaintContent && (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground || paintPhase == PaintPhase::Selection))
        paintExcludedChildrenInBorder(block, paintInfo, paintOffset);

    if (paintPhase == PaintPhase::Mask && block.usedStyle().visibility() == UsedVisibility::Visible) {
        MaskPainter { block, paintInfo, paintOffset }.paintMask();
        return ContentsAction::Stop;
    }

    if (paintPhase == PaintPhase::ClippingMask && block.usedStyle().visibility() == UsedVisibility::Visible) {
        MaskPainter { block, paintInfo, paintOffset }.paintClippingMask();
        return ContentsAction::Stop;
    }

    // If just painting the root background, then return.
    if (paintInfo.paintRootBackgroundOnly())
        return ContentsAction::Stop;

    if (paintPhase == PaintPhase::Accessibility)
        protect(paintInfo.accessibilityRegionContext())->takeBounds(block, paintOffset);

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (paintPhase == PaintPhase::AXCustomColorCollectBackgrounds)
        protect(paintInfo.axCustomColorBackdropContext())->recordBackdrop(block, FloatRect { LayoutRect(paintOffset, block.borderBoxSize()) }, paintInfo.paintBehavior);
#endif

    if (paintPhase == PaintPhase::EventRegion) {
        auto borderRect = LayoutRect(paintOffset, block.borderBoxSize());

        Ref document = block.document();
        CheckedPtr eventRegionContext = paintInfo.eventRegionContext();
        if (paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeBackground) && block.visibleToHitTesting()) {
            auto borderShape = BorderShape::shapeForBorderRect(block.usedStyle(), borderRect);
            LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << block << " uniting region " << borderShape.deprecatedRoundedRect() << " event listener types " << block.style().eventListenerRegionTypes());
            bool overrideUserModifyIsEditable = block.isRenderTextControl() && protect(downcast<RenderTextControl>(block).textFormControlElement())->isInnerTextElementEditable();
            eventRegionContext->unite(borderShape.deprecatedPixelSnappedRoundedRect(document->deviceScaleFactor()), block, protect(block.style()), overrideUserModifyIsEditable);
        }

        if (!paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeForeground))
            return ContentsAction::Stop;

        bool needsTraverseDescendants = block.hasVisualOverflow() || block.containsFloats() || !eventRegionContext->contains(enclosingIntRect(borderRect)) || block.view().needsEventRegionUpdateForNonCompositedFrame();
        LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << block << " needsTraverseDescendants for event region: hasVisualOverflow: " << block.hasVisualOverflow() << " containsFloats: "
            << block.containsFloats() <<  " border box is outside current region: " << !eventRegionContext->contains(enclosingIntRect(borderRect)) << " needsEventRegionUpdateForNonCompositedFrame:" << block.view().needsEventRegionUpdateForNonCompositedFrame());
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
        if (!block.isRenderTextControl()) {
            needsTraverseDescendants |= document->mayHaveEditableElements() && protect(block.page())->shouldBuildEditableRegion();
            LOG_WITH_STREAM(EventRegions, stream << "  needs editable event region: " << (document->mayHaveEditableElements() && block.page().shouldBuildEditableRegion()));
        }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        needsTraverseDescendants |= block.page().shouldBuildInteractionRegions();
#endif

#if ENABLE(TOUCH_EVENT_REGIONS)
        ASSERT_IMPLIES(!needsTraverseDescendants, !block.hasVisualOverflow() && !block.containsFloats());

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
            needsTraverseDescendants = eventRegionContext->mayHaveDescendantTouchEventTargets(block)
                || !eventRegionContext->touchEventListenerRegionsContain(enclosingIntRect(borderRect), block.style().eventListenerRegionTypes());
            LOG_WITH_STREAM(EventRegions, stream << "  needs to traverse descendants for touch event listeners: " << needsTraverseDescendants);
        }
#endif

        if (!needsTraverseDescendants)
            return ContentsAction::Stop;
    }

    auto scrolledOffset = scrolledPaintOffset(block, paintOffset);

    // Column rules need to account for scrolling and clipping.
    // FIXME: Clipping of column rules does not work. We will need a separate paint phase for column rules I suspect in order to get
    // clipping correct (since it has to paint as background but is still considered "contents").
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && block.usedStyle().visibility() == UsedVisibility::Visible)
        paintColumnRules(block, paintInfo, scrolledOffset);

    // Done with backgrounds, borders and column rules.
    if (paintPhase == PaintPhase::BlockBackground)
        return ContentsAction::Stop;

    if (!shouldPaintContent || paintPhase == PaintPhase::SelfOutline)
        return ContentsAction::Skip;
    return ContentsAction::Paint;
}

void BlockPainter::paintAfterContents(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    auto shouldPaintContent = !isSkippedContentRoot(block);
    auto scrolledOffset = scrolledPaintOffset(block, paintOffset);

    // 3. paint selection
    if (!block.document().printing()) {
        // Fill in gaps in selection on lines, between blocks and "empty space" when content is skipped.
        block.paintSelection(paintInfo, scrolledOffset);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && block.hasOutline() && block.usedStyle().visibility() == UsedVisibility::Visible)
        block.paintOutline(paintInfo, LayoutRect(paintOffset, block.borderBoxSize()));

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhase::Foreground,
    // then paint the caret.
    if (shouldPaintContent)
        paintCarets(block, paintInfo, paintOffset);
}

void BlockPainter::paintExcludedChildrenInBorder(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!block.isFieldset())
        return;

    CheckedPtr box = block.findFieldsetLegend();
    if (!box || !box->isExcludedFromNormalLayout() || box->hasSelfPaintingLayer())
        return;

    LayoutPoint childPoint = block.blockStartRelativeToPhysicalForChild(*box, paintOffset);
    box->paintAsInlineBlock(paintInfo, childPoint);
}

void BlockPainter::paintColumnRules(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset)
{
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(block);
    if (!blockFlow || !blockFlow->multiColumnFlow() || paintInfo.context().paintingDisabled())
        return;

    // Iterate over our children and paint the column rules as needed.
    for (CheckedRef columnSet : childrenOfType<RenderMultiColumnSet>(*blockFlow)) {
        LayoutPoint childPoint = columnSet->location() + blockFlow->blockStartRelativeToPhysicalForChild(columnSet, scrolledOffset);
        columnSet->paintColumnRules(paintInfo, childPoint);
    }
}

void BlockPainter::paintFloat(PaintInfo& paintInfo, RenderBox& floatBox, const LayoutPoint& floatBoxLocation)
{
    ASSERT(floatBox.isFloating() && !floatBox.hasSelfPaintingLayer());

    auto preservePhase = isNonPaintingTraversalPhase(paintInfo.phase);
    if (paintInfo.phase != PaintPhase::Float && !preservePhase)
        return;
    // Layout skips the floats that line-clamp hides after the clamp point.
    if (floatBox.isForceHiddenByLineClamp())
        return;

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

void BlockPainter::paintInlineChildren(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset)
{
    ASSERT(block.childrenInline());

    if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(block)) {
        svgText->paintInlineChildren(paintInfo, scrolledOffset);
        return;
    }

    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(block);
    if (!blockFlow)
        return;

    CheckedPtr inlineLayout = blockFlow->inlineLayout();
    if (!inlineLayout)
        return;

    if (paintInfo.phase == PaintPhase::Float && inlineLayout->hasFloats()) {
        paintFloatsAndBlockLevelContentInInline(block, paintInfo, scrolledOffset, *inlineLayout);
        return;
    }

    inlineLayout->paint(paintInfo, scrolledOffset);

    if (isNonPaintingTraversalPhase(paintInfo.phase) && inlineLayout->hasFloats()) {
        for (auto& child : inlineLayout->floatsAndBlockLevelBoxesInTreeOrder()) {
            if (child->isFloating() && !child->hasSelfPaintingLayer())
                paintFloat(paintInfo, child, blockFlow->blockStartRelativeToPhysicalForChild(child, scrolledOffset));
        }
    }
}

void BlockPainter::paintBlockLevelContentInInline(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(block.childrenInline());

    PaintInfo paintInfoForChildren = paintInfoForBlockChildren(block, paintInfo);

    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(block)); !walker.atEnd(); walker.advance()) {
        CheckedPtr child = dynamicDowncast<RenderBox>(walker.current());
        // paintInlineChildren() paints the floats.
        if (!child || child->isFloating())
            continue;
        paintChild(block, paintInfo, paintOffset, *child, paintInfoForChildren);
    }
}

void BlockPainter::paintFloatsAndBlockLevelContentInInline(RenderBlock& block, PaintInfo& paintInfo, const LayoutPoint& scrolledOffset, const LayoutIntegration::LineLayout& inlineLayout)
{
    ASSERT(paintInfo.phase == PaintPhase::Float);

    PaintInfo paintInfoForChildren = paintInfoForBlockChildren(block, paintInfo);

    for (auto& child : inlineLayout.floatsAndBlockLevelBoxesInTreeOrder()) {
        // Like InlineContentPainter.
        if (!child->isFloating() && !paintInfo.shouldPaintWithinRoot(child))
            continue;
        paintChild(block, paintInfo, scrolledOffset, child, paintInfoForChildren);
    }
}

} // namespace Paint
} // namespace WebCore
