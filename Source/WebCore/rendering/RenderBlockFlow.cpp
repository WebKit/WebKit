/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderBlockFlow.h"

#include "Editor.h"
#include "FloatingObjects.h"
#include "Frame.h"
#include "HitTestLocation.h"
#include "InlineTextBox.h"
#include "LayoutRepainter.h"
#include "RenderFlowThread.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSet.h"
#include "RenderNamedFlowFragment.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SimpleLineLayoutFunctions.h"
#include "VerticalPositionCache.h"
#include "VisiblePosition.h"

#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
#include "ShapeInsideInfo.h"
#endif

#if ENABLE(IOS_TEXT_AUTOSIZING)
#include "HTMLElement.h"
#endif

namespace WebCore {

bool RenderBlock::s_canPropagateFloatIntoSibling = false;

struct SameSizeAsMarginInfo {
    uint32_t bitfields : 16;
    LayoutUnit margins[2];
};

COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginValues) == sizeof(LayoutUnit[4]), MarginValues_should_stay_small);
COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginInfo) == sizeof(SameSizeAsMarginInfo), MarginInfo_should_stay_small);

// Our MarginInfo state used when laying out block children.
RenderBlockFlow::MarginInfo::MarginInfo(RenderBlockFlow& block, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding)
    : m_atBeforeSideOfBlock(true)
    , m_atAfterSideOfBlock(false)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_determinedMarginBeforeQuirk(false)
    , m_discardMargin(false)
{
    const RenderStyle& blockStyle = block.style();
    ASSERT(block.isRenderView() || block.parent());
    m_canCollapseWithChildren = !block.isRenderView() && !block.isRoot() && !block.isOutOfFlowPositioned()
        && !block.isFloating() && !block.isTableCell() && !block.hasOverflowClip() && !block.isInlineBlockOrInlineTable()
        && !block.isRenderFlowThread() && !block.isWritingModeRoot() && !block.parent()->isFlexibleBox()
        && blockStyle.hasAutoColumnCount() && blockStyle.hasAutoColumnWidth() && !blockStyle.columnSpan();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !beforeBorderPadding && blockStyle.marginBeforeCollapse() != MSEPARATE;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    m_canCollapseMarginAfterWithChildren = m_canCollapseWithChildren && !afterBorderPadding
        && (blockStyle.logicalHeight().isAuto() && !blockStyle.logicalHeight().value()) && blockStyle.marginAfterCollapse() != MSEPARATE;
    
    m_quirkContainer = block.isTableCell() || block.isBody();

    m_discardMargin = m_canCollapseMarginBeforeWithChildren && block.mustDiscardMarginBefore();

    m_positiveMargin = (m_canCollapseMarginBeforeWithChildren && !block.mustDiscardMarginBefore()) ? block.maxPositiveMarginBefore() : LayoutUnit();
    m_negativeMargin = (m_canCollapseMarginBeforeWithChildren && !block.mustDiscardMarginBefore()) ? block.maxNegativeMarginBefore() : LayoutUnit();
}

RenderBlockFlow::RenderBlockFlow(Element& element, PassRef<RenderStyle> style)
    : RenderBlock(element, std::move(style), RenderBlockFlowFlag)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    setChildrenInline(true);
}

RenderBlockFlow::RenderBlockFlow(Document& document, PassRef<RenderStyle> style)
    : RenderBlock(document, std::move(style), RenderBlockFlowFlag)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    setChildrenInline(true);
}

RenderBlockFlow::~RenderBlockFlow()
{
}

void RenderBlockFlow::createMultiColumnFlowThread()
{
    RenderMultiColumnFlowThread* flowThread = new RenderMultiColumnFlowThread(document(), RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
    flowThread->initializeStyle();
    moveAllChildrenTo(flowThread, true);
    RenderBlock::addChild(flowThread);
    setMultiColumnFlowThread(flowThread);
}

void RenderBlockFlow::destroyMultiColumnFlowThread()
{
    // Get the flow thread out of our list.
    multiColumnFlowThread()->removeFromParent();
    
    // Destroy all the multicolumn sets.
    destroyLeftoverChildren();
    
    // Move all the children of the flow thread into our block.
    multiColumnFlowThread()->moveAllChildrenTo(this, true);
    
    // Now destroy the flow thread.
    multiColumnFlowThread()->destroy();
    
    // Clear the multi-column flow thread pointer.
    setMultiColumnFlowThread(nullptr);
}

void RenderBlockFlow::insertedIntoTree()
{
    RenderBlock::insertedIntoTree();
    createRenderNamedFlowFragmentIfNeeded();
}

void RenderBlockFlow::willBeDestroyed()
{
    // Mark as being destroyed to avoid trouble with merges in removeChild().
    m_beingDestroyed = true;

    if (renderNamedFlowFragment())
        setRenderNamedFlowFragment(0);

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from. Effects that do :before/:after only on hover could crash otherwise.
    destroyLeftoverChildren();

    // Destroy our continuation before anything other than anonymous children.
    // The reason we don't destroy it before anonymous children is that they may
    // have continuations of their own that are anonymous children of our continuation.
    RenderBoxModelObject* continuation = this->continuation();
    if (continuation) {
        continuation->destroy();
        setContinuation(0);
    }

    if (!documentBeingDestroyed()) {
        if (firstRootBox()) {
            // We can't wait for RenderBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            // FIXME: The FrameSelection should be responsible for this when it
            // is notified of DOM mutations.
            if (isSelectionBorder())
                view().clearSelection();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (auto box = firstRootBox(); box; box = box->nextRootBox()) {
                    while (auto childBox = box->firstChild())
                        childBox->removeFromParent();
                }
            }
        } else if (parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    m_lineBoxes.deleteLineBoxes();

    removeFromDelayedUpdateScrollInfoSet();

    // NOTE: This jumps down to RenderBox, bypassing RenderBlock since it would do duplicate work.
    RenderBox::willBeDestroyed();
}

void RenderBlockFlow::clearFloats()
{
    if (m_floatingObjects)
        m_floatingObjects->setHorizontalWritingMode(isHorizontalWritingMode());

    HashSet<RenderBox*> oldIntrudingFloatSet;
    if (!childrenInline() && m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto end = floatingObjectSet.end();
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->get();
            if (!floatingObject->isDescendant())
                oldIntrudingFloatSet.add(&floatingObject->renderer());
        }
    }

    // Inline blocks are covered by the isReplaced() check in the avoidFloats method.
    if (avoidsFloats() || isRoot() || isRenderView() || isFloatingOrOutOfFlowPositioned() || isTableCell()) {
        if (m_floatingObjects)
            m_floatingObjects->clear();
        if (!oldIntrudingFloatSet.isEmpty())
            markAllDescendantsWithFloatsForLayout();
        return;
    }

    RendererToFloatInfoMap floatMap;

    if (m_floatingObjects) {
        if (childrenInline())
            m_floatingObjects->moveAllToFloatInfoMap(floatMap);
        else
            m_floatingObjects->clear();
    }

    // We should not process floats if the parent node is not a RenderBlock. Otherwise, we will add 
    // floats in an invalid context. This will cause a crash arising from a bad cast on the parent.
    // See <rdar://problem/8049753>, where float property is applied on a text node in a SVG.
    if (!parent() || !parent()->isRenderBlockFlow())
        return;

    // Attempt to locate a previous sibling with overhanging floats. We skip any elements that are
    // out of flow (like floating/positioned elements), and we also skip over any objects that may have shifted
    // to avoid floats.
    RenderBlockFlow* parentBlock = toRenderBlockFlow(parent());
    bool parentHasFloats = false;
    RenderObject* prev = previousSibling();
    while (prev && (prev->isFloatingOrOutOfFlowPositioned() || !prev->isBox() || !prev->isRenderBlockFlow() || toRenderBlockFlow(prev)->avoidsFloats())) {
        if (prev->isFloating())
            parentHasFloats = true;
        prev = prev->previousSibling();
    }

    // First add in floats from the parent. Self-collapsing blocks let their parent track any floats that intrude into
    // them (as opposed to floats they contain themselves) so check for those here too.
    LayoutUnit logicalTopOffset = logicalTop();
    if (parentHasFloats || (parentBlock->lowestFloatLogicalBottom() > logicalTopOffset && prev && toRenderBlockFlow(prev)->isSelfCollapsingBlock()))
        addIntrudingFloats(parentBlock, parentBlock->logicalLeftOffsetForContent(), logicalTopOffset);
    
    LayoutUnit logicalLeftOffset = 0;
    if (prev)
        logicalTopOffset -= toRenderBox(prev)->logicalTop();
    else {
        prev = parentBlock;
        logicalLeftOffset += parentBlock->logicalLeftOffsetForContent();
    }

    // Add overhanging floats from the previous RenderBlock, but only if it has a float that intrudes into our space.    
    RenderBlockFlow* block = toRenderBlockFlow(prev);
    if (block->m_floatingObjects && block->lowestFloatLogicalBottom() > logicalTopOffset)
        addIntrudingFloats(block, logicalLeftOffset, logicalTopOffset);

    if (childrenInline()) {
        LayoutUnit changeLogicalTop = LayoutUnit::max();
        LayoutUnit changeLogicalBottom = LayoutUnit::min();
        if (m_floatingObjects) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            auto end = floatingObjectSet.end();
            for (auto it = floatingObjectSet.begin(); it != end; ++it) {
                FloatingObject* floatingObject = it->get();
                std::unique_ptr<FloatingObject> oldFloatingObject = floatMap.take(&floatingObject->renderer());
                LayoutUnit logicalBottom = logicalBottomForFloat(floatingObject);
                if (oldFloatingObject) {
                    LayoutUnit oldLogicalBottom = logicalBottomForFloat(oldFloatingObject.get());
                    if (logicalWidthForFloat(floatingObject) != logicalWidthForFloat(oldFloatingObject.get()) || logicalLeftForFloat(floatingObject) != logicalLeftForFloat(oldFloatingObject.get())) {
                        changeLogicalTop = 0;
                        changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                    } else {
                        if (logicalBottom != oldLogicalBottom) {
                            changeLogicalTop = std::min(changeLogicalTop, std::min(logicalBottom, oldLogicalBottom));
                            changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                        }
                        LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                        LayoutUnit oldLogicalTop = logicalTopForFloat(oldFloatingObject.get());
                        if (logicalTop != oldLogicalTop) {
                            changeLogicalTop = std::min(changeLogicalTop, std::min(logicalTop, oldLogicalTop));
                            changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalTop, oldLogicalTop));
                        }
                    }

                    if (oldFloatingObject->originatingLine() && !selfNeedsLayout()) {
                        ASSERT(&oldFloatingObject->originatingLine()->renderer() == this);
                        oldFloatingObject->originatingLine()->markDirty();
                    }
                } else {
                    changeLogicalTop = 0;
                    changeLogicalBottom = std::max(changeLogicalBottom, logicalBottom);
                }
            }
        }

        auto end = floatMap.end();
        for (auto it = floatMap.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->value.get();
            if (!floatingObject->isDescendant()) {
                changeLogicalTop = 0;
                changeLogicalBottom = std::max(changeLogicalBottom, logicalBottomForFloat(floatingObject));
            }
        }

        markLinesDirtyInBlockRange(changeLogicalTop, changeLogicalBottom);
    } else if (!oldIntrudingFloatSet.isEmpty()) {
        // If there are previously intruding floats that no longer intrude, then children with floats
        // should also get layout because they might need their floating object lists cleared.
        if (m_floatingObjects->set().size() < oldIntrudingFloatSet.size())
            markAllDescendantsWithFloatsForLayout();
        else {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            auto end = floatingObjectSet.end();
            for (auto it = floatingObjectSet.begin(); it != end && !oldIntrudingFloatSet.isEmpty(); ++it)
                oldIntrudingFloatSet.remove(&(*it)->renderer());
            if (!oldIntrudingFloatSet.isEmpty())
                markAllDescendantsWithFloatsForLayout();
        }
    }
}

void RenderBlockFlow::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    if (updateLogicalWidthAndColumnWidth())
        relayoutChildren = true;

    clearFloats();

    LayoutUnit previousHeight = logicalHeight();
    // FIXME: should this start out as borderAndPaddingLogicalHeight() + scrollbarLogicalHeight(),
    // for consistency with other render classes?
    setLogicalHeight(0);

    bool pageLogicalHeightChanged = false;
    bool hasSpecifiedPageLogicalHeight = false;
    checkForPaginationLogicalHeightChange(pageLogicalHeight, pageLogicalHeightChanged, hasSpecifiedPageLogicalHeight);

    const RenderStyle& styleToUse = style();
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasColumns() || hasTransform() || hasReflection() || styleToUse.isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged, columnInfo());

    prepareShapesAndPaginationBeforeBlockLayout(relayoutChildren);
    if (!relayoutChildren)
        relayoutChildren = namedFlowFragmentNeedsUpdate();

    // We use four values, maxTopPos, maxTopNeg, maxBottomPos, and maxBottomNeg, to track
    // our current maximal positive and negative margins. These values are used when we
    // are collapsed with adjacent blocks, so for example, if you have block A and B
    // collapsing together, then you'd take the maximal positive margin from both A and B
    // and subtract it from the maximal negative margin from both A and B to get the
    // true collapsed margin. This algorithm is recursive, so when we finish layout()
    // our block knows its current maximal positive/negative values.
    //
    // Start out by setting our margin values to our current margins. Table cells have
    // no margins, so we don't fill in the values for table cells.
    bool isCell = isTableCell();
    if (!isCell) {
        initMaxMarginValues();
        
        setHasMarginBeforeQuirk(styleToUse.hasMarginBeforeQuirk());
        setHasMarginAfterQuirk(styleToUse.hasMarginAfterQuirk());
        setPaginationStrut(0);
    }

    LayoutUnit repaintLogicalTop = 0;
    LayoutUnit repaintLogicalBottom = 0;
    LayoutUnit maxFloatLogicalBottom = 0;
    if (!firstChild() && !isAnonymousBlock())
        setChildrenInline(true);
    if (childrenInline())
        layoutInlineChildren(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
    else
        layoutBlockChildren(relayoutChildren, maxFloatLogicalBottom);

    // Expand our intrinsic height to encompass floats.
    LayoutUnit toAdd = borderAndPaddingAfter() + scrollbarLogicalHeight();
    if (lowestFloatLogicalBottom() > (logicalHeight() - toAdd) && expandsToEncloseOverhangingFloats())
        setLogicalHeight(lowestFloatLogicalBottom() + toAdd);
    
    if (relayoutForPagination(hasSpecifiedPageLogicalHeight, pageLogicalHeight, statePusher) || relayoutToAvoidWidows(statePusher)) {
        ASSERT(!shouldBreakAtLineToAvoidWidow());
        return;
    }

    // Calculate our new height.
    LayoutUnit oldHeight = logicalHeight();
    LayoutUnit oldClientAfterEdge = clientLogicalBottom();

    // Before updating the final size of the flow thread make sure a forced break is applied after the content.
    // This ensures the size information is correctly computed for the last auto-height region receiving content.
    if (isRenderFlowThread())
        toRenderFlowThread(this)->applyBreakAfterContent(oldClientAfterEdge);

    updateLogicalHeight();
    LayoutUnit newHeight = logicalHeight();
    if (oldHeight != newHeight) {
        if (oldHeight > newHeight && maxFloatLogicalBottom > newHeight && !childrenInline()) {
            // One of our children's floats may have become an overhanging float for us. We need to look for it.
            for (auto& blockFlow : childrenOfType<RenderBlockFlow>(*this)) {
                if (blockFlow.isFloatingOrOutOfFlowPositioned())
                    continue;
                if (blockFlow.lowestFloatLogicalBottom() + blockFlow.logicalTop() > newHeight)
                    addOverhangingFloats(blockFlow, false);
            }
        }
    }

    bool heightChanged = (previousHeight != newHeight);
    if (heightChanged)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    updateShapesAfterBlockLayout(heightChanged);

    // Add overflow from children (unless we're multi-column, since in that case all our child overflow is clipped anyway).
    computeOverflow(oldClientAfterEdge);
    
    statePusher.pop();

    fitBorderToLinesIfNeeded();

    if (view().layoutState()->m_pageLogicalHeight)
        setPageLogicalOffset(view().layoutState()->pageLogicalOffset(this, logicalTop()));

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    // FIXME: This repaint logic should be moved into a separate helper function!
    // Repaint with our new bounds if they are different from our old bounds.
    bool didFullRepaint = repainter.repaintAfterLayout();
    if (!didFullRepaint && repaintLogicalTop != repaintLogicalBottom && (styleToUse.visibility() == VISIBLE || enclosingLayer()->hasVisibleContent())) {
        // FIXME: We could tighten up the left and right invalidation points if we let layoutInlineChildren fill them in based off the particular lines
        // it had to lay out. We wouldn't need the hasOverflowClip() hack in that case either.
        LayoutUnit repaintLogicalLeft = logicalLeftVisualOverflow();
        LayoutUnit repaintLogicalRight = logicalRightVisualOverflow();
        if (hasOverflowClip()) {
            // If we have clipped overflow, we should use layout overflow as well, since visual overflow from lines didn't propagate to our block's overflow.
            // Note the old code did this as well but even for overflow:visible. The addition of hasOverflowClip() at least tightens up the hack a bit.
            // layoutInlineChildren should be patched to compute the entire repaint rect.
            repaintLogicalLeft = std::min(repaintLogicalLeft, logicalLeftLayoutOverflow());
            repaintLogicalRight = std::max(repaintLogicalRight, logicalRightLayoutOverflow());
        }
        
        LayoutRect repaintRect;
        if (isHorizontalWritingMode())
            repaintRect = LayoutRect(repaintLogicalLeft, repaintLogicalTop, repaintLogicalRight - repaintLogicalLeft, repaintLogicalBottom - repaintLogicalTop);
        else
            repaintRect = LayoutRect(repaintLogicalTop, repaintLogicalLeft, repaintLogicalBottom - repaintLogicalTop, repaintLogicalRight - repaintLogicalLeft);

        // The repaint rect may be split across columns, in which case adjustRectForColumns() will return the union.
        adjustRectForColumns(repaintRect);

        repaintRect.inflate(maximalOutlineSize(PaintPhaseOutline));
        
        if (hasOverflowClip()) {
            // Adjust repaint rect for scroll offset
            repaintRect.move(-scrolledContentOffset());

            // Don't allow this rect to spill out of our overflow box.
            repaintRect.intersect(LayoutRect(LayoutPoint(), size()));
        }

        // Make sure the rect is still non-empty after intersecting for overflow above
        if (!repaintRect.isEmpty()) {
            repaintRectangle(repaintRect); // We need to do a partial repaint of our content.
            if (hasReflection())
                repaintRectangle(reflectedRect(repaintRect));
        }
    }

    clearNeedsLayout();
}

void RenderBlockFlow::layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom)
{
    dirtyForLayoutFromPercentageHeightDescendants();

    LayoutUnit beforeEdge = borderAndPaddingBefore();
    LayoutUnit afterEdge = borderAndPaddingAfter() + scrollbarLogicalHeight();

    setLogicalHeight(beforeEdge);
    
    // Lay out our hypothetical grid line as though it occurs at the top of the block.
    if (view().layoutState()->lineGrid() == this)
        layoutLineGridBox();

    // The margin struct caches all our current margin collapsing state.
    MarginInfo marginInfo(*this, beforeEdge, afterEdge);

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout. The same is true for ruby text.
    // It doesn't get included in the normal layout process but is instead skipped.
    RenderObject* childToExclude = layoutSpecialExcludedChild(relayoutChildren);

    LayoutUnit previousFloatLogicalBottom = 0;
    maxFloatLogicalBottom = 0;

    RenderBox* next = firstChildBox();

    while (next) {
        RenderBox& child = *next;
        next = child.nextSiblingBox();

        if (childToExclude == &child)
            continue; // Skip this child, since it will be positioned by the specialized subclass (fieldsets and ruby runs).

        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);

        if (child.isOutOfFlowPositioned()) {
            child.containingBlock()->insertPositionedObject(child);
            adjustPositionedBlock(child, marginInfo);
            continue;
        }
        if (child.isFloating()) {
            insertFloatingObject(child);
            adjustFloatingBlock(marginInfo);
            continue;
        }

        // Lay out the child.
        layoutBlockChild(child, marginInfo, previousFloatLogicalBottom, maxFloatLogicalBottom);
    }
    
    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    handleAfterSideOfBlock(beforeEdge, afterEdge, marginInfo);
}

void RenderBlockFlow::layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    if (m_lineLayoutPath == UndeterminedPath)
        m_lineLayoutPath = SimpleLineLayout::canUseFor(*this) ? SimpleLinesPath : LineBoxesPath;

    if (m_lineLayoutPath == SimpleLinesPath) {
        deleteLineBoxesBeforeSimpleLineLayout();
        layoutSimpleLines(repaintLogicalTop, repaintLogicalBottom);
        return;
    }

    m_simpleLineLayout = nullptr;
    layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
}

void RenderBlockFlow::layoutBlockChild(RenderBox& child, MarginInfo& marginInfo, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom)
{
    LayoutUnit oldPosMarginBefore = maxPositiveMarginBefore();
    LayoutUnit oldNegMarginBefore = maxNegativeMarginBefore();

    // The child is a normal flow object. Compute the margins we will use for collapsing now.
    child.computeAndSetBlockDirectionMargins(this);

    // Try to guess our correct logical top position. In most cases this guess will
    // be correct. Only if we're wrong (when we compute the real logical top position)
    // will we have to potentially relayout.
    LayoutUnit estimateWithoutPagination;
    LayoutUnit logicalTopEstimate = estimateLogicalTopPosition(child, marginInfo, estimateWithoutPagination);

    // Cache our old rect so that we can dirty the proper repaint rects if the child moves.
    LayoutRect oldRect = child.frameRect();
    LayoutUnit oldLogicalTop = logicalTopForChild(child);

#if !ASSERT_DISABLED
    LayoutSize oldLayoutDelta = view().layoutDelta();
#endif
    // Go ahead and position the child as though it didn't collapse with the top.
    setLogicalTopForChild(child, logicalTopEstimate, ApplyLayoutDelta);
    estimateRegionRangeForBoxChild(child);

    RenderBlockFlow* childBlockFlow = child.isRenderBlockFlow() ? toRenderBlockFlow(&child) : nullptr;
    bool markDescendantsWithFloats = false;
    if (logicalTopEstimate != oldLogicalTop && !child.avoidsFloats() && childBlockFlow && childBlockFlow->containsFloats())
        markDescendantsWithFloats = true;
#if ENABLE(SUBPIXEL_LAYOUT)
    else if (UNLIKELY(logicalTopEstimate.mightBeSaturated()))
        // logicalTopEstimate, returned by estimateLogicalTopPosition, might be saturated for
        // very large elements. If it does the comparison with oldLogicalTop might yield a
        // false negative as adding and removing margins, borders etc from a saturated number
        // might yield incorrect results. If this is the case always mark for layout.
        markDescendantsWithFloats = true;
#endif
    else if (!child.avoidsFloats() || child.shrinkToAvoidFloats()) {
        // If an element might be affected by the presence of floats, then always mark it for
        // layout.
        LayoutUnit fb = std::max(previousFloatLogicalBottom, lowestFloatLogicalBottom());
        if (fb > logicalTopEstimate)
            markDescendantsWithFloats = true;
    }

    if (childBlockFlow) {
        if (markDescendantsWithFloats)
            childBlockFlow->markAllDescendantsWithFloatsForLayout();
        if (!child.isWritingModeRoot())
            previousFloatLogicalBottom = std::max(previousFloatLogicalBottom, oldLogicalTop + childBlockFlow->lowestFloatLogicalBottom());
    }

    if (!child.needsLayout())
        child.markForPaginationRelayoutIfNeeded();

    bool childHadLayout = child.everHadLayout();
    bool childNeededLayout = child.needsLayout();
    if (childNeededLayout)
        child.layout();

    // Cache if we are at the top of the block right now.
    bool atBeforeSideOfBlock = marginInfo.atBeforeSideOfBlock();

    // Now determine the correct ypos based off examination of collapsing margin
    // values.
    LayoutUnit logicalTopBeforeClear = collapseMargins(child, marginInfo);

    // Now check for clear.
    LayoutUnit logicalTopAfterClear = clearFloatsIfNeeded(child, marginInfo, oldPosMarginBefore, oldNegMarginBefore, logicalTopBeforeClear);
    
    bool paginated = view().layoutState()->isPaginated();
    if (paginated)
        logicalTopAfterClear = adjustBlockChildForPagination(logicalTopAfterClear, estimateWithoutPagination, child, atBeforeSideOfBlock && logicalTopBeforeClear == logicalTopAfterClear);

    setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

    // Now we have a final top position. See if it really does end up being different from our estimate.
    // clearFloatsIfNeeded can also mark the child as needing a layout even though we didn't move. This happens
    // when collapseMargins dynamically adds overhanging floats because of a child with negative margins.
    if (logicalTopAfterClear != logicalTopEstimate || child.needsLayout() || (paginated && childBlockFlow && childBlockFlow->shouldBreakAtLineToAvoidWidow())) {
        if (child.shrinkToAvoidFloats()) {
            // The child's width depends on the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child.setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childBlockFlow) {
            if (!child.avoidsFloats() && childBlockFlow->containsFloats())
                childBlockFlow->markAllDescendantsWithFloatsForLayout();
            if (!child.needsLayout())
                child.markForPaginationRelayoutIfNeeded();
        }

        // Our guess was wrong. Make the child lay itself out again.
        child.layoutIfNeeded();
    }

    if (updateRegionRangeForBoxChild(child)) {
        child.setNeedsLayout(MarkOnlyThis);
        child.layoutIfNeeded();
    }

    // We are no longer at the top of the block if we encounter a non-empty child.  
    // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
    if (marginInfo.atBeforeSideOfBlock() && !child.isSelfCollapsingBlock())
        marginInfo.setAtBeforeSideOfBlock(false);

    // Now place the child in the correct left position
    determineLogicalLeftPositionForChild(child, ApplyLayoutDelta);

    LayoutSize childOffset = child.location() - oldRect.location();
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    relayoutShapeDescendantIfMoved(child.isRenderBlock() ? toRenderBlock(&child) : nullptr, childOffset);
#endif

    // Update our height now that the child has been placed in the correct position.
    setLogicalHeight(logicalHeight() + logicalHeightForChild(child));
    if (mustSeparateMarginAfterForChild(child)) {
        setLogicalHeight(logicalHeight() + marginAfterForChild(child));
        marginInfo.clearMargin();
    }
    // If the child has overhanging floats that intrude into following siblings (or possibly out
    // of this block), then the parent gets notified of the floats now.
    if (childBlockFlow && childBlockFlow->containsFloats())
        maxFloatLogicalBottom = std::max(maxFloatLogicalBottom, addOverhangingFloats(*childBlockFlow, !childNeededLayout));

    if (childOffset.width() || childOffset.height()) {
        view().addLayoutDelta(childOffset);

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (childHadLayout && !selfNeedsLayout() && child.checkForRepaintDuringLayout())
            child.repaintDuringLayoutIfMoved(oldRect);
    }

    if (!childHadLayout && child.checkForRepaintDuringLayout()) {
        child.repaint();
        child.repaintOverhangingFloats(true);
    }

    if (paginated) {
        // Check for an after page/column break.
        LayoutUnit newHeight = applyAfterBreak(child, logicalHeight(), marginInfo);
        if (newHeight != height())
            setLogicalHeight(newHeight);
    }

    ASSERT(view().layoutDeltaMatches(oldLayoutDelta));
}

void RenderBlockFlow::adjustPositionedBlock(RenderBox& child, const MarginInfo& marginInfo)
{
    bool isHorizontal = isHorizontalWritingMode();
    bool hasStaticBlockPosition = child.style().hasStaticBlockPosition(isHorizontal);
    
    LayoutUnit logicalTop = logicalHeight();
    updateStaticInlinePositionForChild(child, logicalTop);

    if (!marginInfo.canCollapseWithMarginBefore()) {
        // Positioned blocks don't collapse margins, so add the margin provided by
        // the container now. The child's own margin is added later when calculating its logical top.
        LayoutUnit collapsedBeforePos = marginInfo.positiveMargin();
        LayoutUnit collapsedBeforeNeg = marginInfo.negativeMargin();
        logicalTop += collapsedBeforePos - collapsedBeforeNeg;
    }
    
    RenderLayer* childLayer = child.layer();
    if (childLayer->staticBlockPosition() != logicalTop) {
        childLayer->setStaticBlockPosition(logicalTop);
        if (hasStaticBlockPosition)
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

LayoutUnit RenderBlockFlow::marginOffsetForSelfCollapsingBlock()
{
    ASSERT(isSelfCollapsingBlock());
    RenderBlockFlow* parentBlock = toRenderBlockFlow(parent());
    if (parentBlock && style().clear() && parentBlock->getClearDelta(*this, logicalHeight()))
        return marginValuesForChild(*this).positiveMarginBefore();
    return LayoutUnit();
}

void RenderBlockFlow::determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode applyDelta)
{
    LayoutUnit startPosition = borderStart() + paddingStart();
    if (style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        startPosition -= verticalScrollbarWidth();
    LayoutUnit totalAvailableLogicalWidth = borderAndPaddingLogicalWidth() + availableLogicalWidth();

    // Add in our start margin.
    LayoutUnit childMarginStart = marginStartForChild(child);
    LayoutUnit newPosition = startPosition + childMarginStart;
        
    // Some objects (e.g., tables, horizontal rules, overflow:auto blocks) avoid floats. They need
    // to shift over as necessary to dodge any floats that might get in the way.
    if (child.avoidsFloats() && containsFloats() && !flowThreadContainingBlock())
        newPosition += computeStartPositionDeltaForChildAvoidingFloats(child, marginStartForChild(child));

    setLogicalLeftForChild(child, style().isLeftToRightDirection() ? newPosition : totalAvailableLogicalWidth - newPosition - logicalWidthForChild(child), applyDelta);
}

void RenderBlockFlow::adjustFloatingBlock(const MarginInfo& marginInfo)
{
    // The float should be positioned taking into account the bottom margin
    // of the previous flow. We add that margin into the height, get the
    // float positioned properly, and then subtract the margin out of the
    // height again. In the case of self-collapsing blocks, we always just
    // use the top margins, since the self-collapsing block collapsed its
    // own bottom margin into its top margin.
    //
    // Note also that the previous flow may collapse its margin into the top of
    // our block. If this is the case, then we do not add the margin in to our
    // height when computing the position of the float. This condition can be tested
    // for by simply calling canCollapseWithMarginBefore. See
    // http://www.hixie.ch/tests/adhoc/css/box/block/margin-collapse/046.html for
    // an example of this scenario.
    LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? LayoutUnit() : marginInfo.margin();
    setLogicalHeight(logicalHeight() + marginOffset);
    positionNewFloats();
    setLogicalHeight(logicalHeight() - marginOffset);
}

void RenderBlockFlow::updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop)
{
    if (child.style().isOriginalDisplayInlineType())
        setStaticInlinePositionForChild(child, logicalTop, startAlignedOffsetForLine(logicalTop, false));
    else
        setStaticInlinePositionForChild(child, logicalTop, startOffsetForContent(logicalTop));
}

void RenderBlockFlow::setStaticInlinePositionForChild(RenderBox& child, LayoutUnit blockOffset, LayoutUnit inlinePosition)
{
    if (flowThreadContainingBlock()) {
        // Shift the inline position to exclude the region offset.
        inlinePosition += startOffsetForContent() - startOffsetForContent(blockOffset);
    }
    child.layer()->setStaticInlinePosition(inlinePosition);
}

RenderBlockFlow::MarginValues RenderBlockFlow::marginValuesForChild(RenderBox& child) const
{
    LayoutUnit childBeforePositive = 0;
    LayoutUnit childBeforeNegative = 0;
    LayoutUnit childAfterPositive = 0;
    LayoutUnit childAfterNegative = 0;

    LayoutUnit beforeMargin = 0;
    LayoutUnit afterMargin = 0;

    RenderBlockFlow* childRenderBlock = child.isRenderBlockFlow() ? toRenderBlockFlow(&child) : nullptr;
    
    // If the child has the same directionality as we do, then we can just return its
    // margins in the same direction.
    if (!child.isWritingModeRoot()) {
        if (childRenderBlock) {
            childBeforePositive = childRenderBlock->maxPositiveMarginBefore();
            childBeforeNegative = childRenderBlock->maxNegativeMarginBefore();
            childAfterPositive = childRenderBlock->maxPositiveMarginAfter();
            childAfterNegative = childRenderBlock->maxNegativeMarginAfter();
        } else {
            beforeMargin = child.marginBefore();
            afterMargin = child.marginAfter();
        }
    } else if (child.isHorizontalWritingMode() == isHorizontalWritingMode()) {
        // The child has a different directionality. If the child is parallel, then it's just
        // flipped relative to us. We can use the margins for the opposite edges.
        if (childRenderBlock) {
            childBeforePositive = childRenderBlock->maxPositiveMarginAfter();
            childBeforeNegative = childRenderBlock->maxNegativeMarginAfter();
            childAfterPositive = childRenderBlock->maxPositiveMarginBefore();
            childAfterNegative = childRenderBlock->maxNegativeMarginBefore();
        } else {
            beforeMargin = child.marginAfter();
            afterMargin = child.marginBefore();
        }
    } else {
        // The child is perpendicular to us, which means its margins don't collapse but are on the
        // "logical left/right" sides of the child box. We can just return the raw margin in this case.
        beforeMargin = marginBeforeForChild(child);
        afterMargin = marginAfterForChild(child);
    }

    // Resolve uncollapsing margins into their positive/negative buckets.
    if (beforeMargin) {
        if (beforeMargin > 0)
            childBeforePositive = beforeMargin;
        else
            childBeforeNegative = -beforeMargin;
    }
    if (afterMargin) {
        if (afterMargin > 0)
            childAfterPositive = afterMargin;
        else
            childAfterNegative = -afterMargin;
    }

    return MarginValues(childBeforePositive, childBeforeNegative, childAfterPositive, childAfterNegative);
}

LayoutUnit RenderBlockFlow::collapseMargins(RenderBox& child, MarginInfo& marginInfo)
{
    bool childDiscardMarginBefore = mustDiscardMarginBeforeForChild(child);
    bool childDiscardMarginAfter = mustDiscardMarginAfterForChild(child);
    bool childIsSelfCollapsing = child.isSelfCollapsingBlock();

    // The child discards the before margin when the the after margin has discard in the case of a self collapsing block.
    childDiscardMarginBefore = childDiscardMarginBefore || (childDiscardMarginAfter && childIsSelfCollapsing);

    // Get the four margin values for the child and cache them.
    const MarginValues childMargins = marginValuesForChild(child);

    // Get our max pos and neg top margins.
    LayoutUnit posTop = childMargins.positiveMarginBefore();
    LayoutUnit negTop = childMargins.negativeMarginBefore();

    // For self-collapsing blocks, collapse our bottom margins into our
    // top to get new posTop and negTop values.
    if (childIsSelfCollapsing) {
        posTop = std::max(posTop, childMargins.positiveMarginAfter());
        negTop = std::max(negTop, childMargins.negativeMarginAfter());
    }
    
    // See if the top margin is quirky. We only care if this child has
    // margins that will collapse with us.
    bool topQuirk = hasMarginBeforeQuirk(child);

    if (marginInfo.canCollapseWithMarginBefore()) {
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child is collapsing with the top of the
            // block. If it has larger margin values, then we need to update
            // our own maximal values.
            if (!document().inQuirksMode() || !marginInfo.quirkContainer() || !topQuirk)
                setMaxMarginBeforeValues(std::max(posTop, maxPositiveMarginBefore()), std::max(negTop, maxNegativeMarginBefore()));

            // The minute any of the margins involved isn't a quirk, don't
            // collapse it away, even if the margin is smaller (www.webreference.com
            // has an example of this, a <dt> with 0.8em author-specified inside
            // a <dl> inside a <td>.
            if (!marginInfo.determinedMarginBeforeQuirk() && !topQuirk && (posTop - negTop)) {
                setHasMarginBeforeQuirk(false);
                marginInfo.setDeterminedMarginBeforeQuirk(true);
            }

            if (!marginInfo.determinedMarginBeforeQuirk() && topQuirk && !marginBefore())
                // We have no top margin and our top child has a quirky margin.
                // We will pick up this quirky margin and pass it through.
                // This deals with the <td><div><p> case.
                // Don't do this for a block that split two inlines though. You do
                // still apply margins in this case.
                setHasMarginBeforeQuirk(true);
        } else
            // The before margin of the container will also discard all the margins it is collapsing with.
            setMustDiscardMarginBefore();
    }

    // Once we find a child with discardMarginBefore all the margins collapsing with us must also discard. 
    if (childDiscardMarginBefore) {
        marginInfo.setDiscardMargin(true);
        marginInfo.clearMargin();
    }

    if (marginInfo.quirkContainer() && marginInfo.atBeforeSideOfBlock() && (posTop - negTop))
        marginInfo.setHasMarginBeforeQuirk(topQuirk);

    LayoutUnit beforeCollapseLogicalTop = logicalHeight();
    LayoutUnit logicalTop = beforeCollapseLogicalTop;

    LayoutUnit clearanceForSelfCollapsingBlock;
    RenderObject* prev = child.previousSibling();
    // If the child's previous sibling is a self-collapsing block that cleared a float then its top border edge has been set at the bottom border edge
    // of the float. Since we want to collapse the child's top margin with the self-collapsing block's top and bottom margins we need to adjust our parent's height to match the 
    // margin top of the self-collapsing block. If the resulting collapsed margin leaves the child still intruding into the float then we will want to clear it.
    if (!marginInfo.canCollapseWithMarginBefore() && prev && prev->isRenderBlockFlow() && toRenderBlockFlow(prev)->isSelfCollapsingBlock()) {
        clearanceForSelfCollapsingBlock = toRenderBlockFlow(prev)->marginOffsetForSelfCollapsingBlock();
        setLogicalHeight(logicalHeight() - clearanceForSelfCollapsingBlock);
    }

    if (childIsSelfCollapsing) {
        // For a self collapsing block both the before and after margins get discarded. The block doesn't contribute anything to the height of the block.
        // Also, the child's top position equals the logical height of the container.
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child has no height. We need to compute our
            // position before we collapse the child's margins together,
            // so that we can get an accurate position for the zero-height block.
            LayoutUnit collapsedBeforePos = std::max(marginInfo.positiveMargin(), childMargins.positiveMarginBefore());
            LayoutUnit collapsedBeforeNeg = std::max(marginInfo.negativeMargin(), childMargins.negativeMarginBefore());
            marginInfo.setMargin(collapsedBeforePos, collapsedBeforeNeg);
            
            // Now collapse the child's margins together, which means examining our
            // bottom margin values as well. 
            marginInfo.setPositiveMarginIfLarger(childMargins.positiveMarginAfter());
            marginInfo.setNegativeMarginIfLarger(childMargins.negativeMarginAfter());

            if (!marginInfo.canCollapseWithMarginBefore())
                // We need to make sure that the position of the self-collapsing block
                // is correct, since it could have overflowing content
                // that needs to be positioned correctly (e.g., a block that
                // had a specified height of 0 but that actually had subcontent).
                logicalTop = logicalHeight() + collapsedBeforePos - collapsedBeforeNeg;
        }
    } else {
        if (mustSeparateMarginBeforeForChild(child)) {
            ASSERT(!marginInfo.discardMargin() || (marginInfo.discardMargin() && !marginInfo.margin()));
            // If we are at the before side of the block and we collapse, ignore the computed margin
            // and just add the child margin to the container height. This will correctly position
            // the child inside the container.
            LayoutUnit separateMargin = !marginInfo.canCollapseWithMarginBefore() ? marginInfo.margin() : LayoutUnit::fromPixel(0);
            setLogicalHeight(logicalHeight() + separateMargin + marginBeforeForChild(child));
            logicalTop = logicalHeight();
        } else if (!marginInfo.discardMargin() && (!marginInfo.atBeforeSideOfBlock()
            || (!marginInfo.canCollapseMarginBeforeWithChildren()
            && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginBeforeQuirk())))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            setLogicalHeight(logicalHeight() + std::max(marginInfo.positiveMargin(), posTop) - std::max(marginInfo.negativeMargin(), negTop));
            logicalTop = logicalHeight();
        }

        marginInfo.setDiscardMargin(childDiscardMarginAfter);
        
        if (!marginInfo.discardMargin()) {
            marginInfo.setPositiveMargin(childMargins.positiveMarginAfter());
            marginInfo.setNegativeMargin(childMargins.negativeMarginAfter());
        } else
            marginInfo.clearMargin();

        if (marginInfo.margin())
            marginInfo.setHasMarginAfterQuirk(hasMarginAfterQuirk(child));
    }
    
    // If margins would pull us past the top of the next page, then we need to pull back and pretend like the margins
    // collapsed into the page edge.
    LayoutState* layoutState = view().layoutState();
    if (layoutState->isPaginated() && layoutState->pageLogicalHeight() && logicalTop > beforeCollapseLogicalTop
        && hasNextPage(beforeCollapseLogicalTop)) {
        LayoutUnit oldLogicalTop = logicalTop;
        logicalTop = std::min(logicalTop, nextPageLogicalTop(beforeCollapseLogicalTop));
        setLogicalHeight(logicalHeight() + (logicalTop - oldLogicalTop));
    }

    if (prev && prev->isRenderBlockFlow() && !prev->isFloatingOrOutOfFlowPositioned()) {
        // If |child| is a self-collapsing block it may have collapsed into a previous sibling and although it hasn't reduced the height of the parent yet
        // any floats from the parent will now overhang.
        RenderBlockFlow& block = toRenderBlockFlow(*prev);
        LayoutUnit oldLogicalHeight = logicalHeight();
        setLogicalHeight(logicalTop);
        if (block.containsFloats() && !block.avoidsFloats() && (block.logicalTop() + block.lowestFloatLogicalBottom()) > logicalTop)
            addOverhangingFloats(block, false);
        setLogicalHeight(oldLogicalHeight);

        // If |child|'s previous sibling is a self-collapsing block that cleared a float and margin collapsing resulted in |child| moving up
        // into the margin area of the self-collapsing block then the float it clears is now intruding into |child|. Layout again so that we can look for
        // floats in the parent that overhang |child|'s new logical top.
        bool logicalTopIntrudesIntoFloat = clearanceForSelfCollapsingBlock > 0 && logicalTop < beforeCollapseLogicalTop;
        if (logicalTopIntrudesIntoFloat && containsFloats() && !child.avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
            child.setNeedsLayout();
    }

    return logicalTop;
}

LayoutUnit RenderBlockFlow::clearFloatsIfNeeded(RenderBox& child, MarginInfo& marginInfo, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos)
{
    LayoutUnit heightIncrease = getClearDelta(child, yPos);
    if (!heightIncrease)
        return yPos;

    if (child.isSelfCollapsingBlock()) {
        bool childDiscardMargin = mustDiscardMarginBeforeForChild(child) || mustDiscardMarginAfterForChild(child);

        // For self-collapsing blocks that clear, they can still collapse their
        // margins with following siblings. Reset the current margins to represent
        // the self-collapsing block's margins only.
        // If DISCARD is specified for -webkit-margin-collapse, reset the margin values.
        MarginValues childMargins = marginValuesForChild(child);
        if (!childDiscardMargin) {
            marginInfo.setPositiveMargin(std::max(childMargins.positiveMarginBefore(), childMargins.positiveMarginAfter()));
            marginInfo.setNegativeMargin(std::max(childMargins.negativeMarginBefore(), childMargins.negativeMarginAfter()));
        } else
            marginInfo.clearMargin();
        marginInfo.setDiscardMargin(childDiscardMargin);

        // CSS2.1 states:
        // "If the top and bottom margins of an element with clearance are adjoining, its margins collapse with 
        // the adjoining margins of following siblings but that resulting margin does not collapse with the bottom margin of the parent block."
        // So the parent's bottom margin cannot collapse through this block or any subsequent self-collapsing blocks. Check subsequent siblings
        // for a block with height - if none is found then don't allow the margins to collapse with the parent.
        bool wouldCollapseMarginsWithParent = marginInfo.canCollapseMarginAfterWithChildren();
        for (RenderBox* curr = child.nextSiblingBox(); curr && wouldCollapseMarginsWithParent; curr = curr->nextSiblingBox()) {
            if (!curr->isFloatingOrOutOfFlowPositioned() && !curr->isSelfCollapsingBlock())
                wouldCollapseMarginsWithParent = false;
        }
        if (wouldCollapseMarginsWithParent)
            marginInfo.setCanCollapseMarginAfterWithChildren(false);

        // For now set the border-top of |child| flush with the bottom border-edge of the float so it can layout any floating or positioned children of
        // its own at the correct vertical position. If subsequent siblings attempt to collapse with |child|'s margins in |collapseMargins| we will
        // adjust the height of the parent to |child|'s margin top (which if it is positive sits up 'inside' the float it's clearing) so that all three 
        // margins can collapse at the correct vertical position.
        // Per CSS2.1 we need to ensure that any negative margin-top clears |child| beyond the bottom border-edge of the float so that the top border edge of the child
        // (i.e. its clearance)  is at a position that satisfies the equation: "the amount of clearance is set so that clearance + margin-top = [height of float],
        // i.e., clearance = [height of float] - margin-top".
        setLogicalHeight(child.logicalTop() + childMargins.negativeMarginBefore());
    } else
        // Increase our height by the amount we had to clear.
        setLogicalHeight(logicalHeight() + heightIncrease);
    
    if (marginInfo.canCollapseWithMarginBefore()) {
        // We can no longer collapse with the top of the block since a clear
        // occurred. The empty blocks collapse into the cleared block.
        // FIXME: This isn't quite correct. Need clarification for what to do
        // if the height the cleared block is offset by is smaller than the
        // margins involved.
        setMaxMarginBeforeValues(oldTopPosMargin, oldTopNegMargin);
        marginInfo.setAtBeforeSideOfBlock(false);

        // In case the child discarded the before margin of the block we need to reset the mustDiscardMarginBefore flag to the initial value.
        setMustDiscardMarginBefore(style().marginBeforeCollapse() == MDISCARD);
    }

    return yPos + heightIncrease;
}

void RenderBlockFlow::marginBeforeEstimateForChild(RenderBox& child, LayoutUnit& positiveMarginBefore, LayoutUnit& negativeMarginBefore, bool& discardMarginBefore) const
{
    // Give up if in quirks mode and we're a body/table cell and the top margin of the child box is quirky.
    // Give up if the child specified -webkit-margin-collapse: separate that prevents collapsing.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if ((document().inQuirksMode() && hasMarginAfterQuirk(child) && (isTableCell() || isBody())) || child.style().marginBeforeCollapse() == MSEPARATE)
        return;

    // The margins are discarded by a child that specified -webkit-margin-collapse: discard.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if (child.style().marginBeforeCollapse() == MDISCARD) {
        positiveMarginBefore = 0;
        negativeMarginBefore = 0;
        discardMarginBefore = true;
        return;
    }

    LayoutUnit beforeChildMargin = marginBeforeForChild(child);
    positiveMarginBefore = std::max(positiveMarginBefore, beforeChildMargin);
    negativeMarginBefore = std::max(negativeMarginBefore, -beforeChildMargin);

    if (!child.isRenderBlockFlow())
        return;
    
    RenderBlockFlow& childBlock = toRenderBlockFlow(child);
    if (childBlock.childrenInline() || childBlock.isWritingModeRoot())
        return;

    MarginInfo childMarginInfo(childBlock, childBlock.borderAndPaddingBefore(), childBlock.borderAndPaddingAfter());
    if (!childMarginInfo.canCollapseMarginBeforeWithChildren())
        return;

    RenderBox* grandchildBox = childBlock.firstChildBox();
    for (; grandchildBox; grandchildBox = grandchildBox->nextSiblingBox()) {
        if (!grandchildBox->isFloatingOrOutOfFlowPositioned())
            break;
    }
    
    // Give up if there is clearance on the box, since it probably won't collapse into us.
    if (!grandchildBox || grandchildBox->style().clear() != CNONE)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(this);
        if (grandchildBox->isRenderBlock()) {
            RenderBlock* grandchildBlock = toRenderBlock(grandchildBox);
            grandchildBlock->setHasMarginBeforeQuirk(grandchildBox->style().hasMarginBeforeQuirk());
            grandchildBlock->setHasMarginAfterQuirk(grandchildBox->style().hasMarginAfterQuirk());
        }
    }

    // Collapse the margin of the grandchild box with our own to produce an estimate.
    childBlock.marginBeforeEstimateForChild(*grandchildBox, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
}

LayoutUnit RenderBlockFlow::estimateLogicalTopPosition(RenderBox& child, const MarginInfo& marginInfo, LayoutUnit& estimateWithoutPagination)
{
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    LayoutUnit logicalTopEstimate = logicalHeight();
    if (!marginInfo.canCollapseWithMarginBefore()) {
        LayoutUnit positiveMarginBefore = 0;
        LayoutUnit negativeMarginBefore = 0;
        bool discardMarginBefore = false;
        if (child.selfNeedsLayout()) {
            // Try to do a basic estimation of how the collapse is going to go.
            marginBeforeEstimateForChild(child, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
        } else {
            // Use the cached collapsed margin values from a previous layout. Most of the time they
            // will be right.
            MarginValues marginValues = marginValuesForChild(child);
            positiveMarginBefore = std::max(positiveMarginBefore, marginValues.positiveMarginBefore());
            negativeMarginBefore = std::max(negativeMarginBefore, marginValues.negativeMarginBefore());
            discardMarginBefore = mustDiscardMarginBeforeForChild(child);
        }

        // Collapse the result with our current margins.
        if (!discardMarginBefore)
            logicalTopEstimate += std::max(marginInfo.positiveMargin(), positiveMarginBefore) - std::max(marginInfo.negativeMargin(), negativeMarginBefore);
    }

    // Adjust logicalTopEstimate down to the next page if the margins are so large that we don't fit on the current
    // page.
    LayoutState* layoutState = view().layoutState();
    if (layoutState->isPaginated() && layoutState->pageLogicalHeight() && logicalTopEstimate > logicalHeight()
        && hasNextPage(logicalHeight()))
        logicalTopEstimate = std::min(logicalTopEstimate, nextPageLogicalTop(logicalHeight()));

    logicalTopEstimate += getClearDelta(child, logicalTopEstimate);
    
    estimateWithoutPagination = logicalTopEstimate;

    if (layoutState->isPaginated()) {
        // If the object has a page or column break value of "before", then we should shift to the top of the next page.
        logicalTopEstimate = applyBeforeBreak(child, logicalTopEstimate);
    
        // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
        logicalTopEstimate = adjustForUnsplittableChild(child, logicalTopEstimate);
        
        if (!child.selfNeedsLayout() && child.isRenderBlock())
            logicalTopEstimate += toRenderBlock(child).paginationStrut();
    }

    return logicalTopEstimate;
}

void RenderBlockFlow::setCollapsedBottomMargin(const MarginInfo& marginInfo)
{
    if (marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()) {
        // Update the after side margin of the container to discard if the after margin of the last child also discards and we collapse with it.
        // Don't update the max margin values because we won't need them anyway.
        if (marginInfo.discardMargin()) {
            setMustDiscardMarginAfter();
            return;
        }

        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        setMaxMarginAfterValues(std::max(maxPositiveMarginAfter(), marginInfo.positiveMargin()), std::max(maxNegativeMarginAfter(), marginInfo.negativeMargin()));

        if (!marginInfo.hasMarginAfterQuirk())
            setHasMarginAfterQuirk(false);

        if (marginInfo.hasMarginAfterQuirk() && !marginAfter())
            // We have no bottom margin and our last child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            setHasMarginAfterQuirk(true);
    }
}

void RenderBlockFlow::handleAfterSideOfBlock(LayoutUnit beforeSide, LayoutUnit afterSide, MarginInfo& marginInfo)
{
    marginInfo.setAtAfterSideOfBlock(true);

    // If our last child was a self-collapsing block with clearance then our logical height is flush with the
    // bottom edge of the float that the child clears. The correct vertical position for the margin-collapsing we want
    // to perform now is at the child's margin-top - so adjust our height to that position.
    RenderObject* lastBlock = lastChild();
    if (lastBlock && lastBlock->isRenderBlockFlow() && toRenderBlockFlow(lastBlock)->isSelfCollapsingBlock())
        setLogicalHeight(logicalHeight() - toRenderBlockFlow(lastBlock)->marginOffsetForSelfCollapsingBlock());

    // If we can't collapse with children then go ahead and add in the bottom margin.
    if (!marginInfo.discardMargin() && (!marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()
        && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginAfterQuirk())))
        setLogicalHeight(logicalHeight() + marginInfo.margin());
        
    // Now add in our bottom border/padding.
    setLogicalHeight(logicalHeight() + afterSide);

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    setLogicalHeight(std::max(logicalHeight(), beforeSide + afterSide));

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);
}

void RenderBlockFlow::setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!hasRareBlockFlowData()) {
        if (pos == RenderBlockFlowRareData::positiveMarginBeforeDefault(*this) && neg == RenderBlockFlowRareData::negativeMarginBeforeDefault(*this))
            return;
        materializeRareBlockFlowData();
    }

    rareBlockFlowData()->m_margins.setPositiveMarginBefore(pos);
    rareBlockFlowData()->m_margins.setNegativeMarginBefore(neg);
}

void RenderBlockFlow::setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!hasRareBlockFlowData()) {
        if (pos == RenderBlockFlowRareData::positiveMarginAfterDefault(*this) && neg == RenderBlockFlowRareData::negativeMarginAfterDefault(*this))
            return;
        materializeRareBlockFlowData();
    }

    rareBlockFlowData()->m_margins.setPositiveMarginAfter(pos);
    rareBlockFlowData()->m_margins.setNegativeMarginAfter(neg);
}

void RenderBlockFlow::setMustDiscardMarginBefore(bool value)
{
    if (style().marginBeforeCollapse() == MDISCARD) {
        ASSERT(value);
        return;
    }

    if (!hasRareBlockFlowData()) {
        if (!value)
            return;
        materializeRareBlockFlowData();
    }

    rareBlockFlowData()->m_discardMarginBefore = value;
}

void RenderBlockFlow::setMustDiscardMarginAfter(bool value)
{
    if (style().marginAfterCollapse() == MDISCARD) {
        ASSERT(value);
        return;
    }

    if (!hasRareBlockFlowData()) {
        if (!value)
            return;
        materializeRareBlockFlowData();
    }

    rareBlockFlowData()->m_discardMarginAfter = value;
}

bool RenderBlockFlow::mustDiscardMarginBefore() const
{
    return style().marginBeforeCollapse() == MDISCARD || (hasRareBlockFlowData() && rareBlockFlowData()->m_discardMarginBefore);
}

bool RenderBlockFlow::mustDiscardMarginAfter() const
{
    return style().marginAfterCollapse() == MDISCARD || (hasRareBlockFlowData() && rareBlockFlowData()->m_discardMarginAfter);
}

bool RenderBlockFlow::mustDiscardMarginBeforeForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return child.isRenderBlockFlow() ? toRenderBlockFlow(child).mustDiscardMarginBefore() : (child.style().marginBeforeCollapse() == MDISCARD);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isRenderBlockFlow() ? toRenderBlockFlow(child).mustDiscardMarginAfter() : (child.style().marginAfterCollapse() == MDISCARD);

    // FIXME: We return false here because the implementation is not geometrically complete. We have values only for before/after, not start/end.
    // In case the boxes are perpendicular we assume the property is not specified.
    return false;
}

bool RenderBlockFlow::mustDiscardMarginAfterForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return child.isRenderBlockFlow() ? toRenderBlockFlow(child).mustDiscardMarginAfter() : (child.style().marginAfterCollapse() == MDISCARD);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.isRenderBlockFlow() ? toRenderBlockFlow(child).mustDiscardMarginBefore() : (child.style().marginBeforeCollapse() == MDISCARD);

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginBeforeForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const RenderStyle& childStyle = child.style();
    if (!child.isWritingModeRoot())
        return childStyle.marginBeforeCollapse() == MSEPARATE;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginAfterCollapse() == MSEPARATE;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginAfterForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const RenderStyle& childStyle = child.style();
    if (!child.isWritingModeRoot())
        return childStyle.marginAfterCollapse() == MSEPARATE;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginBeforeCollapse() == MSEPARATE;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

static bool inNormalFlow(RenderBox& child)
{
    RenderBlock* curr = child.containingBlock();
    while (curr && curr != &child.view()) {
        if (curr->hasColumns() || curr->isRenderFlowThread())
            return true;
        if (curr->isFloatingOrOutOfFlowPositioned())
            return false;
        curr = curr->containingBlock();
    }
    return true;
}

LayoutUnit RenderBlockFlow::applyBeforeBreak(RenderBox& child, LayoutUnit logicalOffset)
{
    // FIXME: Add page break checking here when we support printing.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool isInsideMulticolFlowThread = flowThread && !flowThread->isRenderNamedFlowThread();
    bool checkColumnBreaks = isInsideMulticolFlowThread || view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight; // FIXME: Once columns can print we have to check this.
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    bool checkBeforeAlways = (checkColumnBreaks && child.style().columnBreakBefore() == PBALWAYS)
        || (checkPageBreaks && child.style().pageBreakBefore() == PBALWAYS)
        || (checkRegionBreaks && child.style().regionBreakBefore() == PBALWAYS);
    if (checkBeforeAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        if (checkColumnBreaks) {
            if (isInsideMulticolFlowThread)
                checkRegionBreaks = true;
            else
                view().layoutState()->addForcedColumnBreak(&child, logicalOffset);
        }
        if (checkRegionBreaks) {
            LayoutUnit offsetBreakAdjustment = 0;
            if (flowThread->addForcedRegionBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset, &child, true, &offsetBreakAdjustment))
                return logicalOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::applyAfterBreak(RenderBox& child, LayoutUnit logicalOffset, MarginInfo& marginInfo)
{
    // FIXME: Add page break checking here when we support printing.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool isInsideMulticolFlowThread = flowThread && !flowThread->isRenderNamedFlowThread();
    bool checkColumnBreaks = isInsideMulticolFlowThread || view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight; // FIXME: Once columns can print we have to check this.
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    bool checkAfterAlways = (checkColumnBreaks && child.style().columnBreakAfter() == PBALWAYS)
        || (checkPageBreaks && child.style().pageBreakAfter() == PBALWAYS)
        || (checkRegionBreaks && child.style().regionBreakAfter() == PBALWAYS);
    if (checkAfterAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? LayoutUnit() : marginInfo.margin();

        // So our margin doesn't participate in the next collapsing steps.
        marginInfo.clearMargin();

        if (checkColumnBreaks) {
            if (isInsideMulticolFlowThread)
                checkRegionBreaks = true;
            else
                view().layoutState()->addForcedColumnBreak(&child, logicalOffset);
        }
        if (checkRegionBreaks) {
            LayoutUnit offsetBreakAdjustment = 0;
            if (flowThread->addForcedRegionBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset + marginOffset, &child, false, &offsetBreakAdjustment))
                return logicalOffset + marginOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox& child, bool atBeforeSideOfBlock)
{
    RenderBlock* childRenderBlock = child.isRenderBlock() ? toRenderBlock(&child) : nullptr;

    if (estimateWithoutPagination != logicalTopAfterClear) {
        // Our guess prior to pagination movement was wrong. Before we attempt to paginate, let's try again at the new
        // position.
        setLogicalHeight(logicalTopAfterClear);
        setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

        if (child.shrinkToAvoidFloats()) {
            // The child's width depends on the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child.setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childRenderBlock) {
            if (!child.avoidsFloats() && childRenderBlock->containsFloats())
                toRenderBlockFlow(childRenderBlock)->markAllDescendantsWithFloatsForLayout();
            if (!child.needsLayout())
                child.markForPaginationRelayoutIfNeeded();
        }

        // Our guess was wrong. Make the child lay itself out again.
        child.layoutIfNeeded();
    }

    LayoutUnit oldTop = logicalTopAfterClear;

    // If the object has a page or column break value of "before", then we should shift to the top of the next page.
    LayoutUnit result = applyBeforeBreak(child, logicalTopAfterClear);

    if (pageLogicalHeightForOffset(result)) {
        LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(result, ExcludePageBoundary);
        LayoutUnit spaceShortage = child.logicalHeight() - remainingLogicalHeight;
        if (spaceShortage > 0) {
            // If the child crosses a column boundary, report a break, in case nothing inside it has already
            // done so. The column balancer needs to know how much it has to stretch the columns to make more
            // content fit. If no breaks are reported (but do occur), the balancer will have no clue. FIXME:
            // This should be improved, though, because here we just pretend that the child is
            // unsplittable. A splittable child, on the other hand, has break opportunities at every position
            // where there's no child content, border or padding. In other words, we risk stretching more
            // than necessary.
            setPageBreak(result, spaceShortage);
        }
    }

    // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
    LayoutUnit logicalTopBeforeUnsplittableAdjustment = result;
    LayoutUnit logicalTopAfterUnsplittableAdjustment = adjustForUnsplittableChild(child, result);
    
    LayoutUnit paginationStrut = 0;
    LayoutUnit unsplittableAdjustmentDelta = logicalTopAfterUnsplittableAdjustment - logicalTopBeforeUnsplittableAdjustment;
    if (unsplittableAdjustmentDelta)
        paginationStrut = unsplittableAdjustmentDelta;
    else if (childRenderBlock && childRenderBlock->paginationStrut())
        paginationStrut = childRenderBlock->paginationStrut();

    if (paginationStrut) {
        // We are willing to propagate out to our parent block as long as we were at the top of the block prior
        // to collapsing our margins, and as long as we didn't clear or move as a result of other pagination.
        if (atBeforeSideOfBlock && oldTop == result && !isOutOfFlowPositioned() && !isTableCell()) {
            // FIXME: Should really check if we're exceeding the page height before propagating the strut, but we don't
            // have all the information to do so (the strut only has the remaining amount to push). Gecko gets this wrong too
            // and pushes to the next page anyway, so not too concerned about it.
            setPaginationStrut(result + paginationStrut);
            if (childRenderBlock)
                childRenderBlock->setPaginationStrut(0);
        } else
            result += paginationStrut;
    }

    // Similar to how we apply clearance. Go ahead and boost height() to be the place where we're going to position the child.
    setLogicalHeight(logicalHeight() + (result - oldTop));
    
    // Return the final adjusted logical top.
    return result;
}

static inline LayoutUnit calculateMinimumPageHeight(RenderStyle* renderStyle, RootInlineBox* lastLine, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // We may require a certain minimum number of lines per page in order to satisfy
    // orphans and widows, and that may affect the minimum page height.
    unsigned lineCount = std::max<unsigned>(renderStyle->hasAutoOrphans() ? 1 : renderStyle->orphans(), renderStyle->hasAutoWidows() ? 1 : renderStyle->widows());
    if (lineCount > 1) {
        RootInlineBox* line = lastLine;
        for (unsigned i = 1; i < lineCount && line->prevRootBox(); i++)
            line = line->prevRootBox();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        LayoutRect overflow = line->logicalVisualOverflowRect(line->lineTop(), line->lineBottom());
        lineTop = std::min(line->lineTopWithLeading(), overflow.y());
    }
    return lineBottom - lineTop;
}

void RenderBlockFlow::adjustLinePositionForPagination(RootInlineBox* lineBox, LayoutUnit& delta, RenderFlowThread* flowThread)
{
    // FIXME: For now we paginate using line overflow. This ensures that lines don't overlap at all when we
    // put a strut between them for pagination purposes. However, this really isn't the desired rendering, since
    // the line on the top of the next page will appear too far down relative to the same kind of line at the top
    // of the first column.
    //
    // The rendering we would like to see is one where the lineTopWithLeading is at the top of the column, and any line overflow
    // simply spills out above the top of the column. This effect would match what happens at the top of the first column.
    // We can't achieve this rendering, however, until we stop columns from clipping to the column bounds (thus allowing
    // for overflow to occur), and then cache visible overflow for each column rect.
    //
    // Furthermore, the paint we have to do when a column has overflow has to be special. We need to exclude
    // content that paints in a previous column (and content that paints in the following column).
    //
    // For now we'll at least honor the lineTopWithLeading when paginating if it is above the logical top overflow. This will
    // at least make positive leading work in typical cases.
    //
    // FIXME: Another problem with simply moving lines is that the available line width may change (because of floats).
    // Technically if the location we move the line to has a different line width than our old position, then we need to dirty the
    // line and all following lines.
    LayoutRect logicalVisualOverflow = lineBox->logicalVisualOverflowRect(lineBox->lineTop(), lineBox->lineBottom());
    LayoutUnit logicalOffset = std::min(lineBox->lineTopWithLeading(), logicalVisualOverflow.y());
    LayoutUnit logicalBottom = std::max(lineBox->lineBottomWithLeading(), logicalVisualOverflow.maxY());
    LayoutUnit lineHeight = logicalBottom - logicalOffset;
    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(&style(), lineBox, logicalOffset, logicalBottom));
    logicalOffset += delta;
    lineBox->setPaginationStrut(0);
    lineBox->setIsFirstAfterPageBreak(false);
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    bool hasUniformPageLogicalHeight = !flowThread || flowThread->regionsHaveUniformLogicalHeight();
    // If lineHeight is greater than pageLogicalHeight, but logicalVisualOverflow.height() still fits, we are
    // still going to add a strut, so that the visible overflow fits on a single page.
    if (!pageLogicalHeight || (hasUniformPageLogicalHeight && logicalVisualOverflow.height() > pageLogicalHeight)
        || !hasNextPage(logicalOffset))
        // FIXME: In case the line aligns with the top of the page (or it's slightly shifted downwards) it will not be marked as the first line in the page.
        // From here, the fix is not straightforward because it's not easy to always determine when the current line is the first in the page.
        return;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);

    int lineIndex = lineCount(lineBox);
    if (remainingLogicalHeight < lineHeight || (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex)) {
        if (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex) {
            clearShouldBreakAtLineToAvoidWidow();
            setDidBreakAtLineToAvoidWidow();
        }
        // If we have a non-uniform page height, then we have to shift further possibly.
        if (!hasUniformPageLogicalHeight && !pushToNextPageWithMinimumLogicalHeight(remainingLogicalHeight, logicalOffset, lineHeight))
            return;
        if (lineHeight > pageLogicalHeight) {
            // Split the top margin in order to avoid splitting the visible part of the line.
            remainingLogicalHeight -= std::min(lineHeight - pageLogicalHeight, std::max<LayoutUnit>(0, logicalVisualOverflow.y() - lineBox->lineTopWithLeading()));
        }
        LayoutUnit totalLogicalHeight = lineHeight + std::max<LayoutUnit>(0, logicalOffset);
        LayoutUnit pageLogicalHeightAtNewOffset = hasUniformPageLogicalHeight ? pageLogicalHeight : pageLogicalHeightForOffset(logicalOffset + remainingLogicalHeight);
        setPageBreak(logicalOffset, lineHeight - remainingLogicalHeight);
        if (((lineBox == firstRootBox() && totalLogicalHeight < pageLogicalHeightAtNewOffset) || (!style().hasAutoOrphans() && style().orphans() >= lineIndex))
            && !isOutOfFlowPositioned() && !isTableCell())
            setPaginationStrut(remainingLogicalHeight + std::max<LayoutUnit>(0, logicalOffset));
        else {
            delta += remainingLogicalHeight;
            lineBox->setPaginationStrut(remainingLogicalHeight);
            lineBox->setIsFirstAfterPageBreak(true);
        }
    } else if (remainingLogicalHeight == pageLogicalHeight) {
        // We're at the very top of a page or column.
        if (lineBox != firstRootBox())
            lineBox->setIsFirstAfterPageBreak(true);
        if (lineBox != firstRootBox() || offsetFromLogicalTopOfFirstPage())
            setPageBreak(logicalOffset, lineHeight);
    }
}

void RenderBlockFlow::setBreakAtLineToAvoidWidow(int lineToBreak)
{
    ASSERT(lineToBreak >= 0);
    ASSERT(!ensureRareBlockFlowData().m_didBreakAtLineToAvoidWidow);
    ensureRareBlockFlowData().m_lineBreakToAvoidWidow = lineToBreak;
}

void RenderBlockFlow::setDidBreakAtLineToAvoidWidow()
{
    ASSERT(!shouldBreakAtLineToAvoidWidow());
    if (!hasRareBlockFlowData())
        return;

    rareBlockFlowData()->m_didBreakAtLineToAvoidWidow = true;
}

void RenderBlockFlow::clearDidBreakAtLineToAvoidWidow()
{
    if (!hasRareBlockFlowData())
        return;

    rareBlockFlowData()->m_didBreakAtLineToAvoidWidow = false;
}

void RenderBlockFlow::clearShouldBreakAtLineToAvoidWidow() const
{
    ASSERT(shouldBreakAtLineToAvoidWidow());
    if (!hasRareBlockFlowData())
        return;

    rareBlockFlowData()->m_lineBreakToAvoidWidow = -1;
}

bool RenderBlockFlow::relayoutToAvoidWidows(LayoutStateMaintainer& statePusher)
{
    if (!shouldBreakAtLineToAvoidWidow())
        return false;

    statePusher.pop();
    setEverHadLayout(true);
    layoutBlock(false);
    return true;
}

bool RenderBlockFlow::hasNextPage(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    ASSERT(view().layoutState() && view().layoutState()->isPaginated());

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread)
        return true; // Printing and multi-column both make new pages to accommodate content.

    // See if we're in the last region.
    LayoutUnit pageOffset = offsetFromLogicalTopOfFirstPage() + logicalOffset;
    RenderRegion* region = flowThread->regionAtBlockOffset(this, pageOffset, true);
    if (!region)
        return false;
    if (region->isLastRegion())
        return region->isRenderRegionSet() || region->style().regionFragment() == BreakRegionFragment
            || (pageBoundaryRule == IncludePageBoundary && pageOffset == region->logicalTopForFlowThreadContent());

    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    flowThread->getRegionRangeForBox(this, startRegion, endRegion);

    if (region == endRegion)
        return false;
    return true;
}

LayoutUnit RenderBlockFlow::adjustForUnsplittableChild(RenderBox& child, LayoutUnit logicalOffset, bool includeMargins)
{
    bool checkColumnBreaks = view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight;
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    bool isUnsplittable = child.isUnsplittableForPagination() || (checkColumnBreaks && child.style().columnBreakInside() == PBAVOID)
        || (checkPageBreaks && child.style().pageBreakInside() == PBAVOID)
        || (checkRegionBreaks && child.style().regionBreakInside() == PBAVOID);
    if (!isUnsplittable)
        return logicalOffset;
    LayoutUnit childLogicalHeight = logicalHeightForChild(child) + (includeMargins ? marginBeforeForChild(child) + marginAfterForChild(child) : LayoutUnit());
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    bool hasUniformPageLogicalHeight = !flowThread || flowThread->regionsHaveUniformLogicalHeight();
    updateMinimumPageHeight(logicalOffset, childLogicalHeight);
    if (!pageLogicalHeight || (hasUniformPageLogicalHeight && childLogicalHeight > pageLogicalHeight)
        || !hasNextPage(logicalOffset))
        return logicalOffset;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);
    if (remainingLogicalHeight < childLogicalHeight) {
        if (!hasUniformPageLogicalHeight && !pushToNextPageWithMinimumLogicalHeight(remainingLogicalHeight, logicalOffset, childLogicalHeight))
            return logicalOffset;
        return logicalOffset + remainingLogicalHeight;
    }
    return logicalOffset;
}

bool RenderBlockFlow::pushToNextPageWithMinimumLogicalHeight(LayoutUnit& adjustment, LayoutUnit logicalOffset, LayoutUnit minimumLogicalHeight) const
{
    bool checkRegion = false;
    for (LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment); pageLogicalHeight;
        pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment)) {
        if (minimumLogicalHeight <= pageLogicalHeight)
            return true;
        if (!hasNextPage(logicalOffset + adjustment))
            return false;
        adjustment += pageLogicalHeight;
        checkRegion = true;
    }
    return !checkRegion;
}

void RenderBlockFlow::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->setPageBreak(this, offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void RenderBlockFlow::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderFlowThread* flowThread = flowThreadContainingBlock())
        flowThread->updateMinimumPageHeight(this, offsetFromLogicalTopOfFirstPage() + offset, minHeight);
    else if (ColumnInfo* colInfo = view().layoutState()->m_columnInfo)
        colInfo->updateMinimumColumnHeight(minHeight);
}

LayoutUnit RenderBlockFlow::nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    if (!pageLogicalHeight)
        return logicalOffset;
    
    // The logicalOffset is in our coordinate space.  We can add in our pushed offset.
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset);
    if (pageBoundaryRule == ExcludePageBoundary)
        return logicalOffset + (remainingLogicalHeight ? remainingLogicalHeight : pageLogicalHeight);
    return logicalOffset + remainingLogicalHeight;
}

LayoutUnit RenderBlockFlow::pageLogicalTopForOffset(LayoutUnit offset) const
{
    LayoutUnit firstPageLogicalTop = isHorizontalWritingMode() ? view().layoutState()->m_pageOffset.height() : view().layoutState()->m_pageOffset.width();
    LayoutUnit blockLogicalTop = isHorizontalWritingMode() ? view().layoutState()->m_layoutOffset.height() : view().layoutState()->m_layoutOffset.width();

    LayoutUnit cumulativeOffset = offset + blockLogicalTop;
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = view().layoutState()->pageLogicalHeight();
        if (!pageLogicalHeight)
            return 0;
        return cumulativeOffset - roundToInt(cumulativeOffset - firstPageLogicalTop) % roundToInt(pageLogicalHeight);
    }
    return firstPageLogicalTop + flowThread->pageLogicalTopForOffset(cumulativeOffset - firstPageLogicalTop);
}

LayoutUnit RenderBlockFlow::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread)
        return view().layoutState()->m_pageLogicalHeight;
    return flowThread->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
}

LayoutUnit RenderBlockFlow::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    offset += offsetFromLogicalTopOfFirstPage();
    
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = view().layoutState()->m_pageLogicalHeight;
        LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
        if (pageBoundaryRule == IncludePageBoundary) {
            // If includeBoundaryPoint is true the line exactly on the top edge of a
            // column will act as being part of the previous column.
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }
    
    return flowThread->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}


void RenderBlockFlow::layoutLineGridBox()
{
    if (style().lineGrid() == RenderStyle::initialLineGrid()) {
        setLineGridBox(0);
        return;
    }
    
    setLineGridBox(0);

    auto lineGridBox = std::make_unique<RootInlineBox>(*this);
    lineGridBox->setHasTextChildren(); // Needed to make the line ascent/descent actually be honored in quirks mode.
    lineGridBox->setConstructed();
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    VerticalPositionCache verticalPositionCache;
    lineGridBox->alignBoxesInBlockDirection(logicalHeight(), textBoxDataMap, verticalPositionCache);
    
    setLineGridBox(std::move(lineGridBox));

    // FIXME: If any of the characteristics of the box change compared to the old one, then we need to do a deep dirtying
    // (similar to what happens when the page height changes). Ideally, though, we only do this if someone is actually snapping
    // to this grid.
}

bool RenderBlockFlow::containsFloat(RenderBox& renderer) const
{
    return m_floatingObjects && m_floatingObjects->set().contains<RenderBox&, FloatingObjectHashTranslator>(renderer);
}

void RenderBlockFlow::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    
    // After our style changed, if we lose our ability to propagate floats into next sibling
    // blocks, then we need to find the top most parent containing that overhanging float and
    // then mark its descendants with floats for layout and clear all floats from its next
    // sibling blocks that exist in our floating objects list. See bug 56299 and 62875.
    bool canPropagateFloatIntoSibling = !isFloatingOrOutOfFlowPositioned() && !avoidsFloats();
    if (diff == StyleDifferenceLayout && s_canPropagateFloatIntoSibling && !canPropagateFloatIntoSibling && hasOverhangingFloats()) {
        RenderBlockFlow* parentBlock = this;
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();

        for (auto& ancestor : ancestorsOfType<RenderBlockFlow>(*this)) {
            if (ancestor.isRenderView())
                break;
            if (ancestor.hasOverhangingFloats()) {
                for (auto it = floatingObjectSet.begin(), end = floatingObjectSet.end(); it != end; ++it) {
                    RenderBox& renderer = (*it)->renderer();
                    if (ancestor.hasOverhangingFloat(renderer)) {
                        parentBlock = &ancestor;
                        break;
                    }
                }
            }
        }

        parentBlock->markAllDescendantsWithFloatsForLayout();
        parentBlock->markSiblingsWithFloatsForLayout();
    }

    if (auto fragment = renderNamedFlowFragment())
        fragment->setStyle(RenderNamedFlowFragment::createStyle(style()));

    if (diff >= StyleDifferenceRepaint)
        invalidateLineLayoutPath();
    
    if (multiColumnFlowThread()) {
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox())
            child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
    }
}

void RenderBlockFlow::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    s_canPropagateFloatIntoSibling = oldStyle ? !isFloatingOrOutOfFlowPositioned() && !avoidsFloats() : false;

    if (oldStyle && parent() && diff == StyleDifferenceLayout && oldStyle->position() != newStyle.position()) {
        if (containsFloats() && !isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
            markAllDescendantsWithFloatsForLayout();
    }

    RenderBlock::styleWillChange(diff, newStyle);
}

void RenderBlockFlow::deleteLines()
{
    if (containsFloats())
        m_floatingObjects->clearLineBoxTreePointers();

    if (m_simpleLineLayout) {
        ASSERT(!m_lineBoxes.firstLineBox());
        m_simpleLineLayout = nullptr;
    } else
        m_lineBoxes.deleteLineBoxTree();

    RenderBlock::deleteLines();
}

void RenderBlockFlow::moveAllChildrenIncludingFloatsTo(RenderBlock* toBlock, bool fullRemoveInsert)
{
    RenderBlockFlow* toBlockFlow = toRenderBlockFlow(toBlock);
    moveAllChildrenTo(toBlockFlow, fullRemoveInsert);

    // When a portion of the render tree is being detached, anonymous blocks
    // will be combined as their children are deleted. In this process, the
    // anonymous block later in the tree is merged into the one preceeding it.
    // It can happen that the later block (this) contains floats that the
    // previous block (toBlockFlow) did not contain, and thus are not in the
    // floating objects list for toBlockFlow. This can result in toBlockFlow
    // containing floats that are not in it's floating objects list, but are in
    // the floating objects lists of siblings and parents. This can cause
    // problems when the float itself is deleted, since the deletion code
    // assumes that if a float is not in it's containing block's floating
    // objects list, it isn't in any floating objects list. In order to
    // preserve this condition (removing it has serious performance
    // implications), we need to copy the floating objects from the old block
    // (this) to the new block (toBlockFlow). The float's metrics will likely
    // all be wrong, but since toBlockFlow is already marked for layout, this
    // will get fixed before anything gets displayed.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=115566
    if (m_floatingObjects) {
        if (!toBlockFlow->m_floatingObjects)
            toBlockFlow->createFloatingObjects();

        const FloatingObjectSet& fromFloatingObjectSet = m_floatingObjects->set();
        auto end = fromFloatingObjectSet.end();

        for (auto it = fromFloatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->get();

            // Don't insert the object again if it's already in the list
            if (toBlockFlow->containsFloat(floatingObject->renderer()))
                continue;

            toBlockFlow->m_floatingObjects->add(floatingObject->unsafeClone());
        }
    }
}

void RenderBlockFlow::addOverflowFromFloats()
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* r = it->get();
        if (r->isDescendant())
            addOverflowFromChild(&r->renderer(), IntSize(xPositionForFloatIncludingMargin(r), yPositionForFloatIncludingMargin(r)));
    }
}

void RenderBlockFlow::computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats)
{
    RenderBlock::computeOverflow(oldClientAfterEdge, recomputeFloats);

    if (!hasColumns() && (recomputeFloats || isRoot() || expandsToEncloseOverhangingFloats() || hasSelfPaintingLayer()))
        addOverflowFromFloats();
}

void RenderBlockFlow::repaintOverhangingFloats(bool paintAllDescendants)
{
    // Repaint any overhanging floats (if we know we're the one to paint them).
    // Otherwise, bail out.
    if (!hasOverhangingFloats())
        return;

    // FIXME: Avoid disabling LayoutState. At the very least, don't disable it for floats originating
    // in this block. Better yet would be to push extra state for the containers of other floats.
    LayoutStateDisabler layoutStateDisabler(&view());
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        // Only repaint the object if it is overhanging, is not in its own layer, and
        // is our responsibility to paint (m_shouldPaint is set). When paintAllDescendants is true, the latter
        // condition is replaced with being a descendant of us.
        if (logicalBottomForFloat(floatingObject) > logicalHeight()
            && !floatingObject->renderer().hasSelfPaintingLayer()
            && (floatingObject->shouldPaint() || (paintAllDescendants && floatingObject->renderer().isDescendantOf(this)))) {
            floatingObject->renderer().repaint();
            floatingObject->renderer().repaintOverhangingFloats(false);
        }
    }
}

void RenderBlockFlow::paintFloats(PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool preservePhase)
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* r = it->get();
        // Only paint the object if our m_shouldPaint flag is set.
        if (r->shouldPaint() && !r->renderer().hasSelfPaintingLayer()) {
            PaintInfo currentPaintInfo(paintInfo);
            currentPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
            // FIXME: LayoutPoint version of xPositionForFloatIncludingMargin would make this much cleaner.
            LayoutPoint childPoint = flipFloatForWritingModeForChild(r, LayoutPoint(paintOffset.x() + xPositionForFloatIncludingMargin(r) - r->renderer().x(), paintOffset.y() + yPositionForFloatIncludingMargin(r) - r->renderer().y()));
            r->renderer().paint(currentPaintInfo, childPoint);
            if (!preservePhase) {
                currentPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
                r->renderer().paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseFloat;
                r->renderer().paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseForeground;
                r->renderer().paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhaseOutline;
                r->renderer().paint(currentPaintInfo, childPoint);
            }
        }
    }
}

void RenderBlockFlow::clipOutFloatingObjects(RenderBlock& rootBlock, const PaintInfo* paintInfo, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto end = floatingObjectSet.end();
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->get();
            LayoutRect floatBox(offsetFromRootBlock.width() + xPositionForFloatIncludingMargin(floatingObject),
                offsetFromRootBlock.height() + yPositionForFloatIncludingMargin(floatingObject),
                floatingObject->renderer().width(), floatingObject->renderer().height());
            rootBlock.flipForWritingMode(floatBox);
            floatBox.move(rootBlockPhysicalPosition.x(), rootBlockPhysicalPosition.y());
            paintInfo->context->clipOut(pixelSnappedIntRect(floatBox));
        }
    }
}

void RenderBlockFlow::createFloatingObjects()
{
    m_floatingObjects = adoptPtr(new FloatingObjects(*this));
}

void RenderBlockFlow::removeFloatingObjects()
{
    if (!m_floatingObjects)
        return;

    m_floatingObjects->clear();
}

FloatingObject* RenderBlockFlow::insertFloatingObject(RenderBox& floatBox)
{
    ASSERT(floatBox.isFloating());

    // Create the list of special objects if we don't aleady have one
    if (!m_floatingObjects)
        createFloatingObjects();
    else {
        // Don't insert the floatingObject again if it's already in the list
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(floatBox);
        if (it != floatingObjectSet.end())
            return it->get();
    }

    // Create the special floatingObject entry & append it to the list

    std::unique_ptr<FloatingObject> floatingObject = FloatingObject::create(floatBox);
    
    // Our location is irrelevant if we're unsplittable or no pagination is in effect.
    // Just go ahead and lay out the float.
    bool isChildRenderBlock = floatBox.isRenderBlock();
    if (isChildRenderBlock && !floatBox.needsLayout() && view().layoutState()->pageLogicalHeightChanged())
        floatBox.setChildNeedsLayout(MarkOnlyThis);
            
    bool needsBlockDirectionLocationSetBeforeLayout = isChildRenderBlock && view().layoutState()->needsBlockDirectionLocationSetBeforeLayout();
    if (!needsBlockDirectionLocationSetBeforeLayout || isWritingModeRoot()) // We are unsplittable if we're a block flow root.
        floatBox.layoutIfNeeded();
    else {
        floatBox.updateLogicalWidth();
        floatBox.computeAndSetBlockDirectionMargins(this);
    }

    setLogicalWidthForFloat(floatingObject.get(), logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));

    return m_floatingObjects->add(std::move(floatingObject));
}

void RenderBlockFlow::removeFloatingObject(RenderBox& floatBox)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(floatBox);
        if (it != floatingObjectSet.end()) {
            FloatingObject* floatingObject = it->get();
            if (childrenInline()) {
                LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                LayoutUnit logicalBottom = logicalBottomForFloat(floatingObject);

                // Fix for https://bugs.webkit.org/show_bug.cgi?id=54995.
                if (logicalBottom < 0 || logicalBottom < logicalTop || logicalTop == LayoutUnit::max())
                    logicalBottom = LayoutUnit::max();
                else {
                    // Special-case zero- and less-than-zero-height floats: those don't touch
                    // the line that they're on, but it still needs to be dirtied. This is
                    // accomplished by pretending they have a height of 1.
                    logicalBottom = std::max(logicalBottom, logicalTop + 1);
                }
                if (floatingObject->originatingLine()) {
                    if (!selfNeedsLayout()) {
                        ASSERT(&floatingObject->originatingLine()->renderer() == this);
                        floatingObject->originatingLine()->markDirty();
                    }
#if !ASSERT_DISABLED
                    floatingObject->setOriginatingLine(0);
#endif
                }
                markLinesDirtyInBlockRange(0, logicalBottom);
            }
            m_floatingObjects->remove(floatingObject);
        }
    }
}

void RenderBlockFlow::removeFloatingObjectsBelow(FloatingObject* lastFloat, int logicalOffset)
{
    if (!containsFloats())
        return;
    
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObject* curr = floatingObjectSet.last().get();
    while (curr != lastFloat && (!curr->isPlaced() || logicalTopForFloat(curr) >= logicalOffset)) {
        m_floatingObjects->remove(curr);
        if (floatingObjectSet.isEmpty())
            break;
        curr = floatingObjectSet.last().get();
    }
}

LayoutUnit RenderBlockFlow::logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        offset = m_floatingObjects->logicalLeftOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalLeftOffsetForLine(offset, applyTextIndent);
}

LayoutUnit RenderBlockFlow::logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        offset = m_floatingObjects->logicalRightOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalRightOffsetForLine(offset, applyTextIndent);
}

LayoutPoint RenderBlockFlow::computeLogicalLocationForFloat(const FloatingObject* floatingObject, LayoutUnit logicalTopOffset) const
{
    RenderBox& childBox = floatingObject->renderer();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
    LayoutUnit logicalRightOffset; // Constant part of right offset.
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    // FIXME Bug 102948: This only works for shape outside directly set on this block.
    ShapeInsideInfo* shapeInsideInfo = this->layoutShapeInsideInfo();
    // FIXME: Implement behavior for right floats.
    if (shapeInsideInfo) {
        LayoutSize floatLogicalSize = logicalSizeForFloat(floatingObject);
        // floatingObject's logicalSize doesn't contain the actual height at this point, so we need to calculate it
        floatLogicalSize.setHeight(logicalHeightForChild(childBox) + marginBeforeForChild(childBox) + marginAfterForChild(childBox));

        // FIXME: If the float doesn't fit in the shape we should push it under the content box
        logicalTopOffset = shapeInsideInfo->computeFirstFitPositionForFloat(floatLogicalSize);
        if (logicalHeight() > logicalTopOffset)
            logicalTopOffset = logicalHeight();

        SegmentList segments = shapeInsideInfo->computeSegmentsForLine(logicalTopOffset, floatLogicalSize.height());
        // FIXME Bug 102949: Add support for shapes with multiple segments.
        if (segments.size() == 1) {
            // The segment offsets are relative to the content box.
            logicalRightOffset = logicalLeftOffset + segments[0].logicalRight;
            logicalLeftOffset += segments[0].logicalLeft;
        }
    } else
#endif
        logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset);

    LayoutUnit floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    bool insideFlowThread = flowThreadContainingBlock();

    if (childBox.style().floating() == LeftFloat) {
        LayoutUnit heightRemainingLeft = 1;
        LayoutUnit heightRemainingRight = 1;
        floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        while (logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight) - floatLogicalLeft < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        floatLogicalLeft = std::max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
    } else {
        LayoutUnit heightRemainingLeft = 1;
        LayoutUnit heightRemainingRight = 1;
        floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
        while (floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft) < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        // Use the original width of the float here, since the local variable
        // |floatLogicalWidth| was capped to the available line width. See
        // fast/block/float/clamped-right-float.html.
        floatLogicalLeft -= logicalWidthForFloat(floatingObject);
    }
    
    return LayoutPoint(floatLogicalLeft, logicalTopOffset);
}

bool RenderBlockFlow::positionNewFloats()
{
    if (!m_floatingObjects)
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    if (floatingObjectSet.isEmpty())
        return false;

    // If all floats have already been positioned, then we have no work to do.
    if (floatingObjectSet.last()->isPlaced())
        return false;

    // Move backwards through our floating object list until we find a float that has
    // already been positioned. Then we'll be able to move forward, positioning all of
    // the new floats that need it.
    auto it = floatingObjectSet.end();
    --it; // Go to last item.
    auto begin = floatingObjectSet.begin();
    FloatingObject* lastPlacedFloatingObject = 0;
    while (it != begin) {
        --it;
        if ((*it)->isPlaced()) {
            lastPlacedFloatingObject = it->get();
            ++it;
            break;
        }
    }

    LayoutUnit logicalTop = logicalHeight();
    
    // The float cannot start above the top position of the last positioned float.
    if (lastPlacedFloatingObject)
        logicalTop = std::max(logicalTopForFloat(lastPlacedFloatingObject), logicalTop);

    auto end = floatingObjectSet.end();
    // Now walk through the set of unpositioned floats and place them.
    for (; it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        if (floatingObject->renderer().containingBlock() != this)
            continue;

        RenderBox& childBox = floatingObject->renderer();

        LayoutUnit childLogicalLeftMargin = style().isLeftToRightDirection() ? marginStartForChild(childBox) : marginEndForChild(childBox);

        LayoutRect oldRect = childBox.frameRect();

        if (childBox.style().clear() & CLEFT)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatLeft), logicalTop);
        if (childBox.style().clear() & CRIGHT)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatRight), logicalTop);

        LayoutPoint floatLogicalLocation = computeLogicalLocationForFloat(floatingObject, logicalTop);

        setLogicalLeftForFloat(floatingObject, floatLogicalLocation.x());

        setLogicalLeftForChild(childBox, floatLogicalLocation.x() + childLogicalLeftMargin);
        setLogicalTopForChild(childBox, floatLogicalLocation.y() + marginBeforeForChild(childBox));

        estimateRegionRangeForBoxChild(childBox);

        LayoutState* layoutState = view().layoutState();
        bool isPaginated = layoutState->isPaginated();
        if (isPaginated && !childBox.needsLayout())
            childBox.markForPaginationRelayoutIfNeeded();
        
        childBox.layoutIfNeeded();

        if (isPaginated) {
            // If we are unsplittable and don't fit, then we need to move down.
            // We include our margins as part of the unsplittable area.
            LayoutUnit newLogicalTop = adjustForUnsplittableChild(childBox, floatLogicalLocation.y(), true);
            
            // See if we have a pagination strut that is making us move down further.
            // Note that an unsplittable child can't also have a pagination strut, so this is
            // exclusive with the case above.
            RenderBlock* childBlock = childBox.isRenderBlock() ? toRenderBlock(&childBox) : nullptr;
            if (childBlock && childBlock->paginationStrut()) {
                newLogicalTop += childBlock->paginationStrut();
                childBlock->setPaginationStrut(0);
            }
            
            if (newLogicalTop != floatLogicalLocation.y()) {
                floatingObject->setPaginationStrut(newLogicalTop - floatLogicalLocation.y());

                floatLogicalLocation = computeLogicalLocationForFloat(floatingObject, newLogicalTop);
                setLogicalLeftForFloat(floatingObject, floatLogicalLocation.x());

                setLogicalLeftForChild(childBox, floatLogicalLocation.x() + childLogicalLeftMargin);
                setLogicalTopForChild(childBox, floatLogicalLocation.y() + marginBeforeForChild(childBox));
        
                if (childBlock)
                    childBlock->setChildNeedsLayout(MarkOnlyThis);
                childBox.layoutIfNeeded();
            }

            if (updateRegionRangeForBoxChild(childBox)) {
                childBox.setNeedsLayout(MarkOnlyThis);
                childBox.layoutIfNeeded();
            }
        }

        setLogicalTopForFloat(floatingObject, floatLogicalLocation.y());

        setLogicalHeightForFloat(floatingObject, logicalHeightForChild(childBox) + marginBeforeForChild(childBox) + marginAfterForChild(childBox));

        m_floatingObjects->addPlacedObject(floatingObject);

#if ENABLE(CSS_SHAPES)
        if (ShapeOutsideInfo* shapeOutside = childBox.shapeOutsideInfo())
            shapeOutside->setReferenceBoxLogicalSize(logicalSizeForChild(childBox));
#endif
        // If the child moved, we have to repaint it.
        if (childBox.checkForRepaintDuringLayout())
            childBox.repaintDuringLayoutIfMoved(oldRect);
    }
    return true;
}

void RenderBlockFlow::newLine(EClear clear)
{
    positionNewFloats();
    // set y position
    LayoutUnit newY = 0;
    switch (clear) {
    case CLEFT:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case CRIGHT:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case CBOTH:
        newY = lowestFloatLogicalBottom();
        break;
    default:
        break;
    }
    if (height() < newY)
        setLogicalHeight(newY);
}

LayoutUnit RenderBlockFlow::logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const
{
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        return m_floatingObjects->logicalLeftOffset(fixedOffset, logicalTop, logicalHeight);

    return fixedOffset;
}

LayoutUnit RenderBlockFlow::logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const
{
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        return m_floatingObjects->logicalRightOffset(fixedOffset, logicalTop, logicalHeight);

    return fixedOffset;
}

LayoutUnit RenderBlockFlow::nextFloatLogicalBottomBelow(LayoutUnit logicalHeight) const
{
    if (!m_floatingObjects)
        return logicalHeight;

    return m_floatingObjects->findNextFloatLogicalBottomBelow(logicalHeight);
}

LayoutUnit RenderBlockFlow::nextFloatLogicalBottomBelowForBlock(LayoutUnit logicalHeight) const
{
    if (!m_floatingObjects)
        return logicalHeight;

    return m_floatingObjects->findNextFloatLogicalBottomBelowForBlock(logicalHeight);
}

LayoutUnit RenderBlockFlow::lowestFloatLogicalBottom(FloatingObject::Type floatType) const
{
    if (!m_floatingObjects)
        return 0;
    LayoutUnit lowestFloatBottom = 0;
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        if (floatingObject->isPlaced() && floatingObject->type() & floatType)
            lowestFloatBottom = std::max(lowestFloatBottom, logicalBottomForFloat(floatingObject));
    }
    return lowestFloatBottom;
}

LayoutUnit RenderBlockFlow::addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (child.hasOverflowClip() || !child.containsFloats() || child.isRoot() || child.hasColumns() || child.isWritingModeRoot())
        return 0;

    LayoutUnit childLogicalTop = child.logicalTop();
    LayoutUnit childLogicalLeft = child.logicalLeft();
    LayoutUnit lowestFloatLogicalBottom = 0;

    // Floats that will remain the child's responsibility to paint should factor into its
    // overflow.
    auto childEnd = child.m_floatingObjects->set().end();
    for (auto childIt = child.m_floatingObjects->set().begin(); childIt != childEnd; ++childIt) {
        FloatingObject* floatingObject = childIt->get();
        LayoutUnit floatLogicalBottom = std::min(logicalBottomForFloat(floatingObject), LayoutUnit::max() - childLogicalTop);
        LayoutUnit logicalBottom = childLogicalTop + floatLogicalBottom;
        lowestFloatLogicalBottom = std::max(lowestFloatLogicalBottom, logicalBottom);

        if (logicalBottom > logicalHeight()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(floatingObject->renderer())) {
                LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(-childLogicalLeft, -childLogicalTop) : LayoutSize(-childLogicalTop, -childLogicalLeft);
                bool shouldPaint = false;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly). We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a self-painting layer boundary.
                if (floatingObject->renderer().enclosingFloatPaintingLayer() == enclosingFloatPaintingLayer()) {
                    floatingObject->setShouldPaint(false);
                    shouldPaint = true;
                }
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                m_floatingObjects->add(floatingObject->copyToNewContainer(offset, shouldPaint, true));
            }
        } else {
            if (makeChildPaintOtherFloats && !floatingObject->shouldPaint() && !floatingObject->renderer().hasSelfPaintingLayer()
                && floatingObject->renderer().isDescendantOf(&child) && floatingObject->renderer().enclosingFloatPaintingLayer() == child.enclosingFloatPaintingLayer()) {
                // The float is not overhanging from this block, so if it is a descendant of the child, the child should
                // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
                // layer.
                // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
                // it should paint.
                floatingObject->setShouldPaint(true);
            }
            
            // Since the float doesn't overhang, it didn't get put into our list. We need to go ahead and add its overflow in to the
            // child now.
            if (floatingObject->isDescendant())
                child.addOverflowFromChild(&floatingObject->renderer(), LayoutSize(xPositionForFloatIncludingMargin(floatingObject), yPositionForFloatIncludingMargin(floatingObject)));
        }
    }
    return lowestFloatLogicalBottom;
}

bool RenderBlockFlow::hasOverhangingFloat(RenderBox& renderer)
{
    if (!m_floatingObjects || hasColumns() || !parent())
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(renderer);
    if (it == floatingObjectSet.end())
        return false;

    return logicalBottomForFloat(it->get()) > logicalHeight();
}

void RenderBlockFlow::addIntrudingFloats(RenderBlockFlow* prev, LayoutUnit logicalLeftOffset, LayoutUnit logicalTopOffset)
{
    ASSERT(!avoidsFloats());

    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!prev->m_floatingObjects)
        return;

    logicalLeftOffset += marginLogicalLeft();

    const FloatingObjectSet& prevSet = prev->m_floatingObjects->set();
    auto prevEnd = prevSet.end();
    for (auto prevIt = prevSet.begin(); prevIt != prevEnd; ++prevIt) {
        FloatingObject* floatingObject = prevIt->get();
        if (logicalBottomForFloat(floatingObject) > logicalTopOffset) {
            if (!m_floatingObjects || !m_floatingObjects->set().contains<FloatingObject&, FloatingObjectHashTranslator>(*floatingObject)) {
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since this margin was added already through the modification of the |logicalLeftOffset| variable
                // above. |logicalLeftOffset| will equal the margin in this case, so it's already been taken
                // into account. Only apply this code if prev is the parent, since otherwise the left margin
                // will get applied twice.
                LayoutSize offset = isHorizontalWritingMode()
                    ? LayoutSize(logicalLeftOffset - (prev != parent() ? prev->marginLeft() : LayoutUnit()), logicalTopOffset)
                    : LayoutSize(logicalTopOffset, logicalLeftOffset - (prev != parent() ? prev->marginTop() : LayoutUnit()));

                m_floatingObjects->add(floatingObject->copyToNewContainer(offset));
            }
        }
    }
}

void RenderBlockFlow::markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove, bool inLayout)
{
    if (!everHadLayout() && !containsFloats())
        return;

    MarkingBehavior markParents = inLayout ? MarkOnlyThis : MarkContainingBlockChain;
    setChildNeedsLayout(markParents);

    if (floatToRemove)
        removeFloatingObject(*floatToRemove);

    if (childrenInline())
        return;

    // Iterate over our children and mark them as needed.
    for (auto& block : childrenOfType<RenderBlock>(*this)) {
        if (!floatToRemove && block.isFloatingOrOutOfFlowPositioned())
            continue;
        if (!block.isRenderBlockFlow()) {
            if (block.shrinkToAvoidFloats() && block.everHadLayout())
                block.setChildNeedsLayout(markParents);
            continue;
        }
        auto& blockFlow = toRenderBlockFlow(block);
        if ((floatToRemove ? blockFlow.containsFloat(*floatToRemove) : blockFlow.containsFloats()) || blockFlow.shrinkToAvoidFloats())
            blockFlow.markAllDescendantsWithFloatsForLayout(floatToRemove, inLayout);
    }
}

void RenderBlockFlow::markSiblingsWithFloatsForLayout(RenderBox* floatToRemove)
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();

    for (RenderObject* next = nextSibling(); next; next = next->nextSibling()) {
        if (!next->isRenderBlockFlow() || next->isFloatingOrOutOfFlowPositioned() || toRenderBlock(next)->avoidsFloats())
            continue;

        RenderBlockFlow* nextBlock = toRenderBlockFlow(next);
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            RenderBox& floatingBox = (*it)->renderer();
            if (floatToRemove && &floatingBox != floatToRemove)
                continue;
            if (nextBlock->containsFloat(floatingBox))
                nextBlock->markAllDescendantsWithFloatsForLayout(&floatingBox);
        }
    }
}

LayoutPoint RenderBlockFlow::flipFloatForWritingModeForChild(const FloatingObject* child, const LayoutPoint& point) const
{
    if (!style().isFlippedBlocksWritingMode())
        return point;
    
    // This is similar to RenderBox::flipForWritingModeForChild. We have to subtract out our left/top offsets twice, since
    // it's going to get added back in. We hide this complication here so that the calling code looks normal for the unflipped
    // case.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child->renderer().height() - 2 * yPositionForFloatIncludingMargin(child));
    return LayoutPoint(point.x() + width() - child->renderer().width() - 2 * xPositionForFloatIncludingMargin(child), point.y());
}

LayoutUnit RenderBlockFlow::getClearDelta(RenderBox& child, LayoutUnit logicalTop)
{
    // There is no need to compute clearance if we have no floats.
    if (!containsFloats())
        return 0;
    
    // At least one float is present. We need to perform the clearance computation.
    bool clearSet = child.style().clear() != CNONE;
    LayoutUnit logicalBottom = 0;
    switch (child.style().clear()) {
    case CNONE:
        break;
    case CLEFT:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case CRIGHT:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case CBOTH:
        logicalBottom = lowestFloatLogicalBottom();
        break;
    }

    // We also clear floats if we are too big to sit on the same line as a float (and wish to avoid floats by default).
    LayoutUnit result = clearSet ? std::max<LayoutUnit>(0, logicalBottom - logicalTop) : LayoutUnit();
    if (!result && child.avoidsFloats()) {
        LayoutUnit newLogicalTop = logicalTop;
        while (true) {
            LayoutUnit availableLogicalWidthAtNewLogicalTopOffset = availableLogicalWidthForLine(newLogicalTop, false, logicalHeightForChild(child));
            if (availableLogicalWidthAtNewLogicalTopOffset == availableLogicalWidthForContent(newLogicalTop))
                return newLogicalTop - logicalTop;

            RenderRegion* region = regionAtBlockOffset(logicalTopForChild(child));
            LayoutRect borderBox = child.borderBoxRectInRegion(region, DoNotCacheRenderBoxRegionInfo);
            LayoutUnit childLogicalWidthAtOldLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            // FIXME: None of this is right for perpendicular writing-mode children.
            LayoutUnit childOldLogicalWidth = child.logicalWidth();
            LayoutUnit childOldMarginLeft = child.marginLeft();
            LayoutUnit childOldMarginRight = child.marginRight();
            LayoutUnit childOldLogicalTop = child.logicalTop();

            child.setLogicalTop(newLogicalTop);
            child.updateLogicalWidth();
            region = regionAtBlockOffset(logicalTopForChild(child));
            borderBox = child.borderBoxRectInRegion(region, DoNotCacheRenderBoxRegionInfo);
            LayoutUnit childLogicalWidthAtNewLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            child.setLogicalTop(childOldLogicalTop);
            child.setLogicalWidth(childOldLogicalWidth);
            child.setMarginLeft(childOldMarginLeft);
            child.setMarginRight(childOldMarginRight);
            
            if (childLogicalWidthAtNewLogicalTopOffset <= availableLogicalWidthAtNewLogicalTopOffset) {
                // Even though we may not be moving, if the logical width did shrink because of the presence of new floats, then
                // we need to force a relayout as though we shifted. This happens because of the dynamic addition of overhanging floats
                // from previous siblings when negative margins exist on a child (see the addOverhangingFloats call at the end of collapseMargins).
                if (childLogicalWidthAtOldLogicalTopOffset != childLogicalWidthAtNewLogicalTopOffset)
                    child.setChildNeedsLayout(MarkOnlyThis);
                return newLogicalTop - logicalTop;
            }

            newLogicalTop = nextFloatLogicalBottomBelowForBlock(newLogicalTop);
            ASSERT(newLogicalTop >= logicalTop);
            if (newLogicalTop < logicalTop)
                break;
        }
        ASSERT_NOT_REACHED();
    }
    return result;
}

bool RenderBlockFlow::hitTestFloats(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!m_floatingObjects)
        return false;

    LayoutPoint adjustedLocation = accumulatedOffset;
    if (isRenderView())
        adjustedLocation += toLayoutSize(toRenderView(*this).frameView().scrollPosition());

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto begin = floatingObjectSet.begin();
    for (auto it = floatingObjectSet.end(); it != begin;) {
        --it;
        FloatingObject* floatingObject = it->get();
        if (floatingObject->shouldPaint() && !floatingObject->renderer().hasSelfPaintingLayer()) {
            LayoutUnit xOffset = xPositionForFloatIncludingMargin(floatingObject) - floatingObject->renderer().x();
            LayoutUnit yOffset = yPositionForFloatIncludingMargin(floatingObject) - floatingObject->renderer().y();
            LayoutPoint childPoint = flipFloatForWritingModeForChild(floatingObject, adjustedLocation + LayoutSize(xOffset, yOffset));
            if (floatingObject->renderer().hitTest(request, result, locationInContainer, childPoint)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(childPoint));
                return true;
            }
        }
    }

    return false;
}

bool RenderBlockFlow::hitTestInlineChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    ASSERT(childrenInline());

    if (m_simpleLineLayout)
        return SimpleLineLayout::hitTestFlow(*this, *m_simpleLineLayout, request, result, locationInContainer, accumulatedOffset, hitTestAction);

    return m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

void RenderBlockFlow::adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const
{
    if (style().visibility() != VISIBLE)
        return;

    // We don't deal with relative positioning.  Our assumption is that you shrink to fit the lines without accounting
    // for either overflow or translations via relative positioning.
    if (childrenInline()) {
        const_cast<RenderBlockFlow&>(*this).ensureLineBoxes();

        for (auto box = firstRootBox(); box; box = box->nextRootBox()) {
            if (box->firstChild())
                left = std::min(left, x + static_cast<LayoutUnit>(box->firstChild()->x()));
            if (box->lastChild())
                right = std::max(right, x + static_cast<LayoutUnit>(ceilf(box->lastChild()->logicalRight())));
        }
    } else {
        for (RenderBox* obj = firstChildBox(); obj; obj = obj->nextSiblingBox()) {
            if (!obj->isFloatingOrOutOfFlowPositioned()) {
                if (obj->isRenderBlockFlow() && !obj->hasOverflowClip())
                    toRenderBlockFlow(obj)->adjustForBorderFit(x + obj->x(), left, right);
                else if (obj->style().visibility() == VISIBLE) {
                    // We are a replaced element or some kind of non-block-flow object.
                    left = std::min(left, x + obj->x());
                    right = std::max(right, x + obj->x() + obj->width());
                }
            }
        }
    }

    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto end = floatingObjectSet.end();
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* r = it->get();
            // Only examine the object if our m_shouldPaint flag is set.
            if (r->shouldPaint()) {
                LayoutUnit floatLeft = xPositionForFloatIncludingMargin(r) - r->renderer().x();
                LayoutUnit floatRight = floatLeft + r->renderer().width();
                left = std::min(left, floatLeft);
                right = std::max(right, floatRight);
            }
        }
    }
}

void RenderBlockFlow::fitBorderToLinesIfNeeded()
{
    if (style().borderFit() == BorderFitBorder || hasOverrideWidth())
        return;

    // Walk any normal flow lines to snugly fit.
    LayoutUnit left = LayoutUnit::max();
    LayoutUnit right = LayoutUnit::min();
    LayoutUnit oldWidth = contentWidth();
    adjustForBorderFit(0, left, right);
    
    // Clamp to our existing edges. We can never grow. We only shrink.
    LayoutUnit leftEdge = borderLeft() + paddingLeft();
    LayoutUnit rightEdge = leftEdge + oldWidth;
    left = std::min(rightEdge, std::max(leftEdge, left));
    right = std::max(leftEdge, std::min(rightEdge, right));
    
    LayoutUnit newContentWidth = right - left;
    if (newContentWidth == oldWidth)
        return;
    
    setOverrideLogicalContentWidth(newContentWidth);
    layoutBlock(false);
    clearOverrideLogicalContentWidth();
}

void RenderBlockFlow::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    RootInlineBox* lowestDirtyLine = lastRootBox();
    RootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->lineBottomWithLeading() >= logicalBottom && logicalBottom < LayoutUnit::max()) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest != highest && (afterLowest->lineBottomWithLeading() >= logicalTop || afterLowest->lineBottomWithLeading() < 0)) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

int RenderBlockFlow::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (!childrenInline())
        return RenderBlock::firstLineBaseline();

    if (!hasLines())
        return -1;

    if (m_simpleLineLayout)
        return SimpleLineLayout::computeFlowFirstLineBaseline(*this, *m_simpleLineLayout);

    ASSERT(firstRootBox());
    return firstRootBox()->logicalTop() + firstLineStyle().fontMetrics().ascent(firstRootBox()->baselineType());
}

int RenderBlockFlow::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return -1;

    if (!childrenInline())
        return RenderBlock::inlineBlockBaseline(lineDirection);

    if (!hasLines()) {
        if (!hasLineIfEmpty())
            return -1;
        const FontMetrics& fontMetrics = firstLineStyle().fontMetrics();
        return fontMetrics.ascent()
             + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
             + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight());
    }

    if (m_simpleLineLayout)
        return SimpleLineLayout::computeFlowLastLineBaseline(*this, *m_simpleLineLayout);

    bool isFirstLine = lastRootBox() == firstRootBox();
    const RenderStyle& style = isFirstLine ? firstLineStyle() : this->style();
    return lastRootBox()->logicalTop() + style.fontMetrics().ascent(lastRootBox()->baselineType());
}

GapRects RenderBlockFlow::inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    ASSERT(!m_simpleLineLayout);

    GapRects result;

    bool containsStart = selectionState() == SelectionStart || selectionState() == SelectionBoth;

    if (!hasLines()) {
        if (containsStart) {
            // Go ahead and update our lastLogicalTop to be the bottom of the block.  <hr>s or empty blocks with height can trip this
            // case.
            lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight();
            lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache);
            lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight(), cache);
        }
        return result;
    }

    RootInlineBox* lastSelectedLine = 0;
    RootInlineBox* curr;
    for (curr = firstRootBox(); curr && !curr->hasSelectedChildren(); curr = curr->nextRootBox()) { }

    // Now paint the gaps for the lines.
    for (; curr && curr->hasSelectedChildren(); curr = curr->nextRootBox()) {
        LayoutUnit selTop =  curr->selectionTopAdjustedForPrecedingBlock();
        LayoutUnit selHeight = curr->selectionHeightAdjustedForPrecedingBlock();

        if (!containsStart && !lastSelectedLine &&
            selectionState() != SelectionStart && selectionState() != SelectionBoth)
            result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, selTop, cache, paintInfo));
        
        LayoutRect logicalRect(curr->logicalLeft(), selTop, curr->logicalWidth(), selTop + selHeight);
        logicalRect.move(isHorizontalWritingMode() ? offsetFromRootBlock : offsetFromRootBlock.transposedSize());
        LayoutRect physicalRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
        if (!paintInfo || (isHorizontalWritingMode() && physicalRect.y() < paintInfo->rect.maxY() && physicalRect.maxY() > paintInfo->rect.y())
            || (!isHorizontalWritingMode() && physicalRect.x() < paintInfo->rect.maxX() && physicalRect.maxX() > paintInfo->rect.x()))
            result.unite(curr->lineSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, selTop, selHeight, cache, paintInfo));

        lastSelectedLine = curr;
    }

    if (containsStart && !lastSelectedLine)
        // VisibleSelection must start just after our last line.
        lastSelectedLine = lastRootBox();

    if (lastSelectedLine && selectionState() != SelectionEnd && selectionState() != SelectionBoth) {
        // Go ahead and update our lastY to be the bottom of the last selected line.
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + lastSelectedLine->selectionBottom();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, lastSelectedLine->selectionBottom(), cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, lastSelectedLine->selectionBottom(), cache);
    }
    return result;
}

void RenderBlockFlow::createRenderNamedFlowFragmentIfNeeded()
{
    if (!document().cssRegionsEnabled() || renderNamedFlowFragment() || isRenderNamedFlowFragment())
        return;

    if (style().isDisplayRegionType() && style().hasFlowFrom()) {
        RenderNamedFlowFragment* flowFragment = new RenderNamedFlowFragment(document(), RenderNamedFlowFragment::createStyle(style()));
        flowFragment->initializeStyle();
        setRenderNamedFlowFragment(flowFragment);
        addChild(renderNamedFlowFragment());
    }
}

bool RenderBlockFlow::canHaveChildren() const
{
    return !renderNamedFlowFragment() ? RenderBlock::canHaveChildren() : renderNamedFlowFragment()->canHaveChildren();
}

bool RenderBlockFlow::canHaveGeneratedChildren() const
{
    return !renderNamedFlowFragment() ? RenderBlock::canHaveGeneratedChildren() : renderNamedFlowFragment()->canHaveGeneratedChildren();
}

bool RenderBlockFlow::namedFlowFragmentNeedsUpdate() const
{
    if (!isRenderNamedFlowFragmentContainer())
        return false;

    return hasRelativeLogicalHeight() && !isRenderView();
}

void RenderBlockFlow::updateLogicalHeight()
{
    RenderBlock::updateLogicalHeight();

    if (renderNamedFlowFragment())
        renderNamedFlowFragment()->setLogicalHeight(std::max<LayoutUnit>(0, logicalHeight() - borderAndPaddingLogicalHeight()));
}

void RenderBlockFlow::setRenderNamedFlowFragment(RenderNamedFlowFragment* flowFragment)
{
    RenderBlockFlowRareData& rareData = ensureRareBlockFlowData();
    if (rareData.m_renderNamedFlowFragment)
        rareData.m_renderNamedFlowFragment->destroy();
    rareData.m_renderNamedFlowFragment = flowFragment;
}

void RenderBlockFlow::setMultiColumnFlowThread(RenderMultiColumnFlowThread* flowThread)
{
    RenderBlockFlowRareData& rareData = ensureRareBlockFlowData();
    rareData.m_multiColumnFlowThread = flowThread;
}

static bool shouldCheckLines(const RenderBlockFlow& blockFlow)
{
    return !blockFlow.isFloatingOrOutOfFlowPositioned() && blockFlow.style().height().isAuto();
}

RootInlineBox* RenderBlockFlow::lineAtIndex(int i) const
{
    ASSERT(i >= 0);

    if (style().visibility() != VISIBLE)
        return nullptr;

    if (childrenInline()) {
        for (auto box = firstRootBox(); box; box = box->nextRootBox()) {
            if (!i--)
                return box;
        }
        return nullptr;
    }

    for (auto& blockFlow : childrenOfType<RenderBlockFlow>(*this)) {
        if (!shouldCheckLines(blockFlow))
            continue;
        if (RootInlineBox* box = blockFlow.lineAtIndex(i))
            return box;
    }

    return nullptr;
}

int RenderBlockFlow::lineCount(const RootInlineBox* stopRootInlineBox, bool* found) const
{
    if (style().visibility() != VISIBLE)
        return 0;

    int count = 0;

    if (childrenInline()) {
        for (auto box = firstRootBox(); box; box = box->nextRootBox()) {
            count++;
            if (box == stopRootInlineBox) {
                if (found)
                    *found = true;
                break;
            }
        }
        return count;
    }

    for (auto& blockFlow : childrenOfType<RenderBlockFlow>(*this)) {
        if (!shouldCheckLines(blockFlow))
            continue;
        bool recursiveFound = false;
        count += blockFlow.lineCount(stopRootInlineBox, &recursiveFound);
        if (recursiveFound) {
            if (found)
                *found = true;
            break;
        }
    }

    return count;
}

static int getHeightForLineCount(const RenderBlockFlow& block, int lineCount, bool includeBottom, int& count)
{
    if (block.style().visibility() != VISIBLE)
        return -1;

    if (block.childrenInline()) {
        for (auto box = block.firstRootBox(); box; box = box->nextRootBox()) {
            if (++count == lineCount)
                return box->lineBottom() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : LayoutUnit());
        }
    } else {
        RenderBox* normalFlowChildWithoutLines = 0;
        for (auto obj = block.firstChildBox(); obj; obj = obj->nextSiblingBox()) {
            if (obj->isRenderBlockFlow() && shouldCheckLines(toRenderBlockFlow(*obj))) {
                int result = getHeightForLineCount(toRenderBlockFlow(*obj), lineCount, false, count);
                if (result != -1)
                    return result + obj->y() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : LayoutUnit());
            } else if (!obj->isFloatingOrOutOfFlowPositioned())
                normalFlowChildWithoutLines = obj;
        }
        if (normalFlowChildWithoutLines && !lineCount)
            return normalFlowChildWithoutLines->y() + normalFlowChildWithoutLines->height();
    }
    
    return -1;
}

int RenderBlockFlow::heightForLineCount(int lineCount)
{
    int count = 0;
    return getHeightForLineCount(*this, lineCount, true, count);
}

void RenderBlockFlow::clearTruncation()
{
    if (style().visibility() != VISIBLE)
        return;

    if (childrenInline() && hasMarkupTruncation()) {
        ensureLineBoxes();

        setHasMarkupTruncation(false);
        for (auto box = firstRootBox(); box; box = box->nextRootBox())
            box->clearTruncation();
        return;
    }

    for (auto& blockFlow : childrenOfType<RenderBlockFlow>(*this)) {
        if (shouldCheckLines(blockFlow))
            blockFlow.clearTruncation();
    }
}

bool RenderBlockFlow::containsNonZeroBidiLevel() const
{
    for (auto root = firstRootBox(); root; root = root->nextRootBox()) {
        for (auto box = root->firstLeafChild(); box; box = box->nextLeafChild()) {
            if (box->bidiLevel())
                return true;
        }
    }
    return false;
}

Position RenderBlockFlow::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->renderer().nonPseudoNode())
        return createLegacyEditingPosition(nonPseudoElement(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    InlineTextBox* textBox = toInlineTextBox(box);
    return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? textBox->start() : textBox->start() + textBox->len());
}

VisiblePosition RenderBlockFlow::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents)
{
    ASSERT(childrenInline());

    ensureLineBoxes();

    if (!firstRootBox())
        return createVisiblePosition(0, DOWNSTREAM);

    bool linesAreFlipped = style().isFlippedLinesWritingMode();
    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineBox* closestBox = 0;
    RootInlineBox* firstRootBoxWithChildren = 0;
    RootInlineBox* lastRootBoxWithChildren = 0;
    for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
        if (!root->firstLeafChild())
            continue;
        if (!firstRootBoxWithChildren)
            firstRootBoxWithChildren = root;

        if (!linesAreFlipped && root->isFirstAfterPageBreak() && (pointInLogicalContents.y() < root->lineTopWithLeading()
            || (blocksAreFlipped && pointInLogicalContents.y() == root->lineTopWithLeading())))
            break;

        lastRootBoxWithChildren = root;

        // check if this root line box is located at this y coordinate
        if (pointInLogicalContents.y() < root->selectionBottom() || (blocksAreFlipped && pointInLogicalContents.y() == root->selectionBottom())) {
            if (linesAreFlipped) {
                RootInlineBox* nextRootBoxWithChildren = root->nextRootBox();
                while (nextRootBoxWithChildren && !nextRootBoxWithChildren->firstLeafChild())
                    nextRootBoxWithChildren = nextRootBoxWithChildren->nextRootBox();

                if (nextRootBoxWithChildren && nextRootBoxWithChildren->isFirstAfterPageBreak() && (pointInLogicalContents.y() > nextRootBoxWithChildren->lineTopWithLeading()
                    || (!blocksAreFlipped && pointInLogicalContents.y() == nextRootBoxWithChildren->lineTopWithLeading())))
                    continue;
            }
            closestBox = root->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
            if (closestBox)
                break;
        }
    }

    bool moveCaretToBoundary = frame().editor().behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastRootBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = lastRootBoxWithChildren->closestLeafChildForLogicalLeftPosition(pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            LayoutUnit firstRootBoxWithChildrenTop = std::min<LayoutUnit>(firstRootBoxWithChildren->selectionTop(), firstRootBoxWithChildren->logicalTop());
            if (pointInLogicalContents.y() < firstRootBoxWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstRootBoxWithChildrenTop)) {
                InlineBox* box = firstRootBoxWithChildren->firstLeafChild();
                if (box->isLineBreak()) {
                    if (InlineBox* newBox = box->nextLeafChildIgnoringLineBreak())
                        box = newBox;
                }
                // y coordinate is above first root line box, so return the start of the first
                return VisiblePosition(positionForBox(box, true), DOWNSTREAM);
            }
        }

        // pass the box a top position that is inside it
        LayoutPoint point(pointInLogicalContents.x(), closestBox->root().blockDirectionPointInLine());
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->renderer().isReplaced())
            return positionForPointRespectingEditingBoundaries(*this, toRenderBox(closestBox->renderer()), point);
        return closestBox->renderer().positionForPoint(point);
    }

    if (lastRootBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineBox* logicallyLastBox;
        if (lastRootBoxWithChildren->getLogicalEndBoxWithNode(logicallyLastBox))
            return VisiblePosition(positionForBox(logicallyLastBox, false), DOWNSTREAM);
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createVisiblePosition(0, DOWNSTREAM);
}

VisiblePosition RenderBlockFlow::positionForPoint(const LayoutPoint& point)
{
    if (auto fragment = renderNamedFlowFragment())
        return fragment->positionForPoint(point);
    return RenderBlock::positionForPoint(point);
}


void RenderBlockFlow::addFocusRingRectsForInlineChildren(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    ASSERT(childrenInline());

    ensureLineBoxes();

    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
        LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
        LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
        if (!rect.isEmpty())
            rects.append(pixelSnappedIntRect(rect));
    }
}

void RenderBlockFlow::paintInlineChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(childrenInline());

    if (m_simpleLineLayout) {
        SimpleLineLayout::paintFlow(*this, *m_simpleLineLayout, paintInfo, paintOffset);
        return;
    }
    m_lineBoxes.paint(this, paintInfo, paintOffset);
}

bool RenderBlockFlow::relayoutForPagination(bool hasSpecifiedPageLogicalHeight, LayoutUnit pageLogicalHeight, LayoutStateMaintainer& statePusher)
{
    if (!hasColumns() && !multiColumnFlowThread())
        return false;

    if (hasColumns()) {
        RefPtr<RenderOverflow> savedOverflow = m_overflow.release();
        if (childrenInline())
            addOverflowFromInlineChildren();
        else
            addOverflowFromBlockChildren();
        LayoutUnit layoutOverflowLogicalBottom = (isHorizontalWritingMode() ? layoutOverflowRect().maxY() : layoutOverflowRect().maxX()) - borderAndPaddingBefore();

        // FIXME: We don't balance properly at all in the presence of forced page breaks. We need to understand what
        // the distance between forced page breaks is so that we can avoid making the minimum column height too tall.
        ColumnInfo* colInfo = columnInfo();
        if (!hasSpecifiedPageLogicalHeight) {
            LayoutUnit columnHeight = pageLogicalHeight;
            int minColumnCount = colInfo->forcedBreaks() + 1;
            int desiredColumnCount = colInfo->desiredColumnCount();
            if (minColumnCount >= desiredColumnCount) {
                // The forced page breaks are in control of the balancing. Just set the column height to the
                // maximum page break distance.
                if (!pageLogicalHeight) {
                    LayoutUnit distanceBetweenBreaks = std::max<LayoutUnit>(colInfo->maximumDistanceBetweenForcedBreaks(),
                        view().layoutState()->pageLogicalOffset(this, borderAndPaddingBefore() + layoutOverflowLogicalBottom) - colInfo->forcedBreakOffset());
                    columnHeight = std::max(colInfo->minimumColumnHeight(), distanceBetweenBreaks);
                }
            } else if (layoutOverflowLogicalBottom > boundedMultiply(pageLogicalHeight, desiredColumnCount)) {
                // Now that we know the intrinsic height of the columns, we have to rebalance them.
                columnHeight = std::max<LayoutUnit>(colInfo->minimumColumnHeight(), ceilf((float)layoutOverflowLogicalBottom / desiredColumnCount));
            }
            
            if (columnHeight && columnHeight != pageLogicalHeight) {
                statePusher.pop();
                setEverHadLayout(true);
                layoutBlock(false, columnHeight);
                return true;
            }
        } 

        if (pageLogicalHeight)
            colInfo->setColumnCountAndHeight(ceilf((float)layoutOverflowLogicalBottom / pageLogicalHeight), pageLogicalHeight);

        if (columnCount(colInfo)) {
            setLogicalHeight(borderAndPaddingBefore() + colInfo->columnHeight() + borderAndPaddingAfter() + scrollbarLogicalHeight());
            clearOverflow();
        } else
            m_overflow = savedOverflow.release();
        return false;
    }
    
    if (!multiColumnFlowThread()->shouldRelayoutForPagination())
        return false;
    
    multiColumnFlowThread()->setNeedsRebalancing(false);
    multiColumnFlowThread()->setInBalancingPass(true); // Prevent re-entering this method (and recursion into layout).

    bool needsRelayout;
    bool neededRelayout = false;
    bool firstPass = true;
    do {
        // Column heights may change here because of balancing. We may have to do multiple layout
        // passes, depending on how the contents is fitted to the changed column heights. In most
        // cases, laying out again twice or even just once will suffice. Sometimes we need more
        // passes than that, though, but the number of retries should not exceed the number of
        // columns, unless we have a bug.
        needsRelayout = false;
        for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox())
            if (childBox != multiColumnFlowThread() && childBox->isRenderMultiColumnSet()) {
                RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(childBox);
                if (multicolSet->recalculateBalancedHeight(firstPass)) {
                    multicolSet->setChildNeedsLayout(MarkOnlyThis);
                    needsRelayout = true;
                }
            }

        if (needsRelayout) {
            // Layout again. Column balancing resulted in a new height.
            neededRelayout = true;
            multiColumnFlowThread()->setChildNeedsLayout(MarkOnlyThis);
            setChildNeedsLayout(MarkOnlyThis);
            if (firstPass)
                statePusher.pop();
            layoutBlock(false);
        }
        firstPass = false;
    } while (needsRelayout);
    
    multiColumnFlowThread()->setInBalancingPass(false);
    
    return neededRelayout;
}

bool RenderBlockFlow::hasLines() const
{
    ASSERT(childrenInline());

    if (m_simpleLineLayout)
        return m_simpleLineLayout->lineCount();

    return lineBoxes().firstLineBox();
}

void RenderBlockFlow::layoutSimpleLines(LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    ASSERT(!m_lineBoxes.firstLineBox());

    m_simpleLineLayout = SimpleLineLayout::create(*this);

    LayoutUnit lineLayoutHeight = SimpleLineLayout::computeFlowHeight(*this, *m_simpleLineLayout);
    LayoutUnit lineLayoutTop = borderAndPaddingBefore();

    repaintLogicalTop = lineLayoutTop;
    repaintLogicalBottom = lineLayoutTop + lineLayoutHeight;

    setLogicalHeight(lineLayoutTop + lineLayoutHeight + borderAndPaddingAfter());
}

void RenderBlockFlow::deleteLineBoxesBeforeSimpleLineLayout()
{
    ASSERT(m_lineLayoutPath == SimpleLinesPath);
    lineBoxes().deleteLineBoxes();
    toRenderText(firstChild())->deleteLineBoxesBeforeSimpleLineLayout();
}

void RenderBlockFlow::ensureLineBoxes()
{
    m_lineLayoutPath = ForceLineBoxesPath;

    if (!m_simpleLineLayout)
        return;
    m_simpleLineLayout = nullptr;

#if !ASSERT_DISABLED
    LayoutUnit oldHeight = logicalHeight();
#endif
    bool didNeedLayout = needsLayout();

    bool relayoutChildren = false;
    LayoutUnit repaintLogicalTop;
    LayoutUnit repaintLogicalBottom;
    layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);

    updateLogicalHeight();
    ASSERT(didNeedLayout || logicalHeight() == oldHeight);

    if (!didNeedLayout)
        clearNeedsLayout();
}

#ifndef NDEBUG
void RenderBlockFlow::showLineTreeAndMark(const InlineBox* markedBox1, const char* markedLabel1, const InlineBox* markedBox2, const char* markedLabel2, const RenderObject* obj) const
{
    RenderBlock::showLineTreeAndMark(markedBox1, markedLabel1, markedBox2, markedLabel2, obj);
    for (const RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->showLineTreeAndMark(markedBox1, markedLabel1, markedBox2, markedLabel2, obj, 1);
}
#endif

RenderBlockFlow::RenderBlockFlowRareData& RenderBlockFlow::ensureRareBlockFlowData()
{
    if (hasRareBlockFlowData())
        return *m_rareBlockFlowData;
    materializeRareBlockFlowData();
    return *m_rareBlockFlowData;
}

void RenderBlockFlow::materializeRareBlockFlowData()
{
    ASSERT(!hasRareBlockFlowData());
    m_rareBlockFlowData = std::make_unique<RenderBlockFlow::RenderBlockFlowRareData>(*this);
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
inline static bool isVisibleRenderText(RenderObject* renderer)
{
    if (!renderer->isText())
        return false;
    RenderText* renderText = toRenderText(renderer);
    return !renderText->linesBoundingBox().isEmpty() && !renderText->text()->containsOnlyWhitespace();
}

inline static bool resizeTextPermitted(RenderObject* render)
{
    // We disallow resizing for text input fields and textarea to address <rdar://problem/5792987> and <rdar://problem/8021123>
    auto renderer = render->parent();
    while (renderer) {
        // Get the first non-shadow HTMLElement and see if it's an input.
        if (renderer->element() && renderer->element()->isHTMLElement() && !renderer->element()->isInShadowTree()) {
            const HTMLElement& element = toHTMLElement(*renderer->element());
            return !isHTMLInputElement(element) && !isHTMLTextAreaElement(element);
        }
        renderer = renderer->parent();
    }
    return true;
}

int RenderBlockFlow::immediateLineCount()
{
    // Copied and modified from RenderBlock::lineCount.
    // Only descend into list items.
    int count = 0;
    if (style().visibility() == VISIBLE) {
        if (childrenInline()) {
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                count++;
        } else {
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling()) {
                if (obj->isListItem())
                    count += toRenderBlockFlow(obj)->lineCount();
            }
        }
    }
    return count;
}

static bool isNonBlocksOrNonFixedHeightListItems(const RenderObject* render)
{
    if (!render->isRenderBlock())
        return true;
    if (render->isListItem())
        return render->style().height().type() != Fixed;
    return false;
}

//  For now, we auto size single lines of text the same as multiple lines.
//  We've been experimenting with low values for single lines of text.
static inline float oneLineTextMultiplier(float specifiedSize)
{
    return std::max((1.0f / log10f(specifiedSize) * 1.7f), 1.0f);
}

static inline float textMultiplier(float specifiedSize)
{
    return std::max((1.0f / log10f(specifiedSize) * 1.95f), 1.0f);
}

void RenderBlockFlow::adjustComputedFontSizes(float size, float visibleWidth)
{
    // Don't do any work if the block is smaller than the visible area.
    if (visibleWidth >= width())
        return;
    
    unsigned lineCount;
    if (m_lineCountForTextAutosizing == NOT_SET) {
        int count = immediateLineCount();
        if (!count)
            lineCount = NO_LINE;
        else if (count == 1)
            lineCount = ONE_LINE;
        else
            lineCount = MULTI_LINE;
    } else
        lineCount = m_lineCountForTextAutosizing;
    
    ASSERT(lineCount != NOT_SET);
    if (lineCount == NO_LINE)
        return;
    
    float actualWidth = m_widthForTextAutosizing != -1 ? static_cast<float>(m_widthForTextAutosizing) : static_cast<float>(width());
    float scale = visibleWidth / actualWidth;
    float minFontSize = roundf(size / scale);
    
    for (RenderObject* descendent = traverseNext(this, isNonBlocksOrNonFixedHeightListItems); descendent; descendent = descendent->traverseNext(this, isNonBlocksOrNonFixedHeightListItems)) {
        if (isVisibleRenderText(descendent) && resizeTextPermitted(descendent)) {
            RenderText* text = toRenderText(descendent);
            RenderStyle& oldStyle = text->style();
            FontDescription fontDescription = oldStyle.fontDescription();
            float specifiedSize = fontDescription.specifiedSize();
            float scaledSize = roundf(specifiedSize * scale);
            if (scaledSize > 0 && scaledSize < minFontSize) {
                // Record the width of the block and the line count the first time we resize text and use it from then on for text resizing.
                // This makes text resizing consistent even if the block's width or line count changes (which can be caused by text resizing itself 5159915).
                if (m_lineCountForTextAutosizing == NOT_SET)
                    m_lineCountForTextAutosizing = lineCount;
                if (m_widthForTextAutosizing == -1)
                    m_widthForTextAutosizing = actualWidth;
                
                float candidateNewSize = 0;
                float lineTextMultiplier = lineCount == ONE_LINE ? oneLineTextMultiplier(specifiedSize) : textMultiplier(specifiedSize);
                candidateNewSize = roundf(std::min(minFontSize, specifiedSize * lineTextMultiplier));
                if (candidateNewSize > specifiedSize && candidateNewSize != fontDescription.computedSize() && text->textNode() && oldStyle.textSizeAdjust().isAuto())
                    document().addAutoSizingNode(text->textNode(), candidateNewSize);
            }
        }
    }
}
#endif // ENABLE(IOS_TEXT_AUTOSIZING)

RenderObject* RenderBlockFlow::layoutSpecialExcludedChild(bool relayoutChildren)
{
    if (!multiColumnFlowThread())
        return 0;

    // Update the dimensions of our regions before we lay out the flow thread.
    // FIXME: Eventually this is going to get way more complicated, and we will be destroying regions
    // instead of trying to keep them around.
    bool shouldInvalidateRegions = false;
    for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox == multiColumnFlowThread())
            continue;

        if (relayoutChildren || childBox->needsLayout()) {
            if (!multiColumnFlowThread()->inBalancingPass() && childBox->isRenderMultiColumnSet())
                toRenderMultiColumnSet(childBox)->prepareForLayout();
            shouldInvalidateRegions = true;
        }
    }
    
    if (shouldInvalidateRegions)
        multiColumnFlowThread()->invalidateRegions();

    if (relayoutChildren)
        multiColumnFlowThread()->setChildNeedsLayout(MarkOnlyThis);
    
    if (multiColumnFlowThread()->requiresBalancing()) {
        // At the end of multicol layout, relayoutForPagination() is called unconditionally, but if
        // no children are to be laid out (e.g. fixed width with layout already being up-to-date),
        // we want to prevent it from doing any work, so that the column balancing machinery doesn't
        // kick in and trigger additional unnecessary layout passes. Actually, it's not just a good
        // idea in general to not waste time on balancing content that hasn't been re-laid out; we
        // are actually required to guarantee this. The calculation of implicit breaks needs to be
        // preceded by a proper layout pass, since it's layout that sets up content runs, and the
        // runs get deleted right after every pass.
        multiColumnFlowThread()->setNeedsRebalancing(shouldInvalidateRegions || multiColumnFlowThread()->needsLayout());
    }

    setLogicalTopForChild(*multiColumnFlowThread(), borderAndPaddingBefore());
    multiColumnFlowThread()->layoutIfNeeded();
    determineLogicalLeftPositionForChild(*multiColumnFlowThread());
    
    return multiColumnFlowThread();
}

void RenderBlockFlow::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (multiColumnFlowThread())
        return multiColumnFlowThread()->addChild(newChild, beforeChild);
    RenderBlock::addChild(newChild, beforeChild);
}

void RenderBlockFlow::checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight)
{
    // If we don't use either of the two column implementations or a flow thread, then bail.
    if (!isRenderFlowThread() && !multiColumnFlowThread() && !hasColumns())
        return;
    
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    if (multiColumnFlowThread()) {
        updateLogicalHeight();
        multiColumnFlowThread()->setColumnHeightAvailable(std::max<LayoutUnit>(contentLogicalHeight(), 0));
        setLogicalHeight(0);
    } else if (hasColumns()) {
        ColumnInfo* colInfo = columnInfo();
    
        if (!pageLogicalHeight) {
            // We need to go ahead and set our explicit page height if one exists, so that we can
            // avoid doing two layout passes.
            updateLogicalHeight();
            LayoutUnit columnHeight = isRenderView() ? view().pageOrViewLogicalHeight() : contentLogicalHeight();
            if (columnHeight > 0) {
                pageLogicalHeight = columnHeight;
                hasSpecifiedPageLogicalHeight = true;
            }
            setLogicalHeight(0);
        }

        if (colInfo->columnHeight() != pageLogicalHeight && everHadLayout())
            pageLogicalHeightChanged = true;

        colInfo->setColumnHeight(pageLogicalHeight);
        
        if (!hasSpecifiedPageLogicalHeight && !pageLogicalHeight)
            colInfo->clearForcedBreaks();

        colInfo->setPaginationUnit(paginationUnit());
    } else if (isRenderFlowThread()) {
        pageLogicalHeight = 1; // This is just a hack to always make sure we have a page logical height.
        pageLogicalHeightChanged = toRenderFlowThread(this)->pageLogicalSizeChanged();
    }
}

void RenderBlockFlow::setComputedColumnCountAndWidth(int count, LayoutUnit width)
{
    if (!document().regionBasedColumnsEnabled())
        return RenderBlock::setComputedColumnCountAndWidth(count, width);
    
    bool destroyColumns = !requiresColumns(count);
    if (destroyColumns) {
        if (multiColumnFlowThread())
            destroyMultiColumnFlowThread();
    } else {
        if (!multiColumnFlowThread())
            createMultiColumnFlowThread();
        multiColumnFlowThread()->setColumnCountAndWidth(count, width);
        multiColumnFlowThread()->setProgressionIsInline(style().hasInlineColumnAxis());
        multiColumnFlowThread()->setProgressionIsReversed(style().columnProgression() == ReverseColumnProgression);
    }
}

void RenderBlockFlow::updateColumnProgressionFromStyle(RenderStyle* style)
{
    if (!document().regionBasedColumnsEnabled())
        return RenderBlock::updateColumnProgressionFromStyle(style);

    if (!multiColumnFlowThread())
        return;
    
    bool needsLayout = false;
    bool oldProgressionIsInline = multiColumnFlowThread()->progressionIsInline();
    bool newProgressionIsInline = style->hasInlineColumnAxis();
    if (oldProgressionIsInline != newProgressionIsInline) {
        multiColumnFlowThread()->setProgressionIsInline(newProgressionIsInline);
        needsLayout = true;
    }

    bool oldProgressionIsReversed = multiColumnFlowThread()->progressionIsReversed();
    bool newProgressionIsReversed = style->columnProgression() == ReverseColumnProgression;
    if (oldProgressionIsReversed != newProgressionIsReversed) {
        multiColumnFlowThread()->setProgressionIsReversed(newProgressionIsReversed);
        needsLayout = true;
    }

    if (needsLayout)
        setNeedsLayoutAndPrefWidthsRecalc();
}

LayoutUnit RenderBlockFlow::computedColumnWidth() const
{
    if (!document().regionBasedColumnsEnabled())
        return RenderBlock::computedColumnWidth();
    
    if (multiColumnFlowThread())
        return multiColumnFlowThread()->computedColumnWidth();
    return contentLogicalWidth();
}

unsigned RenderBlockFlow::computedColumnCount() const
{
    if (!document().regionBasedColumnsEnabled())
        return RenderBlock::computedColumnCount();
    
    if (multiColumnFlowThread())
        return multiColumnFlowThread()->computedColumnCount();
    
    return 1;
}

bool RenderBlockFlow::isTopLayoutOverflowAllowed() const
{
    bool hasTopOverflow = RenderBlock::isTopLayoutOverflowAllowed();
    if (!multiColumnFlowThread() || style().columnProgression() == NormalColumnProgression)
        return hasTopOverflow;
    
    if (!(isHorizontalWritingMode() ^ !style().hasInlineColumnAxis()))
        hasTopOverflow = !hasTopOverflow;

    return hasTopOverflow;
}

bool RenderBlockFlow::isLeftLayoutOverflowAllowed() const
{
    bool hasLeftOverflow = RenderBlock::isLeftLayoutOverflowAllowed();
    if (!multiColumnFlowThread() || style().columnProgression() == NormalColumnProgression)
        return hasLeftOverflow;
    
    if (isHorizontalWritingMode() ^ !style().hasInlineColumnAxis())
        hasLeftOverflow = !hasLeftOverflow;

    return hasLeftOverflow;
}

}
// namespace WebCore
