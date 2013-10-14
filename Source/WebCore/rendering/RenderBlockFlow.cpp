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

#include "FloatingObjects.h"
#include "HitTestLocation.h"
#include "LayoutRepainter.h"
#include "RenderFlowThread.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "VerticalPositionCache.h"

using namespace std;

namespace WebCore {

bool RenderBlock::s_canPropagateFloatIntoSibling = false;

struct SameSizeAsMarginInfo {
    uint32_t bitfields : 16;
    LayoutUnit margins[2];
};

COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginValues) == sizeof(LayoutUnit[4]), MarginValues_should_stay_small);
COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginInfo) == sizeof(SameSizeAsMarginInfo), MarginInfo_should_stay_small);

// Our MarginInfo state used when laying out block children.
RenderBlockFlow::MarginInfo::MarginInfo(RenderBlockFlow* block, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding)
    : m_atBeforeSideOfBlock(true)
    , m_atAfterSideOfBlock(false)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_determinedMarginBeforeQuirk(false)
    , m_discardMargin(false)
{
    RenderStyle* blockStyle = block->style();
    ASSERT(block->isRenderView() || block->parent());
    m_canCollapseWithChildren = !block->isRenderView() && !block->isRoot() && !block->isOutOfFlowPositioned()
        && !block->isFloating() && !block->isTableCell() && !block->hasOverflowClip() && !block->isInlineBlockOrInlineTable()
        && !block->isRenderFlowThread() && !block->isWritingModeRoot() && !block->parent()->isFlexibleBox()
        && blockStyle->hasAutoColumnCount() && blockStyle->hasAutoColumnWidth() && !blockStyle->columnSpan();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !beforeBorderPadding && blockStyle->marginBeforeCollapse() != MSEPARATE;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    m_canCollapseMarginAfterWithChildren = m_canCollapseWithChildren && !afterBorderPadding
        && (blockStyle->logicalHeight().isAuto() && !blockStyle->logicalHeight().value()) && blockStyle->marginAfterCollapse() != MSEPARATE;
    
    m_quirkContainer = block->isTableCell() || block->isBody();

    m_discardMargin = m_canCollapseMarginBeforeWithChildren && block->mustDiscardMarginBefore();

    m_positiveMargin = (m_canCollapseMarginBeforeWithChildren && !block->mustDiscardMarginBefore()) ? block->maxPositiveMarginBefore() : LayoutUnit();
    m_negativeMargin = (m_canCollapseMarginBeforeWithChildren && !block->mustDiscardMarginBefore()) ? block->maxNegativeMarginBefore() : LayoutUnit();
}

RenderBlockFlow::RenderBlockFlow(Element& element)
    : RenderBlock(element, RenderBlockFlowFlag)
{
}

RenderBlockFlow::RenderBlockFlow(Document& document)
    : RenderBlock(document, RenderBlockFlowFlag)
{
}

RenderBlockFlow::~RenderBlockFlow()
{
}

void RenderBlockFlow::willBeDestroyed()
{
    if (lineGridBox())
        lineGridBox()->destroy(renderArena());

    RenderBlock::willBeDestroyed();
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
        if (m_floatingObjects) {
            m_floatingObjects->clear();
        }
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

    // First add in floats from the parent.
    LayoutUnit logicalTopOffset = logicalTop();
    if (parentHasFloats)
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
                        changeLogicalBottom = max(changeLogicalBottom, max(logicalBottom, oldLogicalBottom));
                    } else {
                        if (logicalBottom != oldLogicalBottom) {
                            changeLogicalTop = min(changeLogicalTop, min(logicalBottom, oldLogicalBottom));
                            changeLogicalBottom = max(changeLogicalBottom, max(logicalBottom, oldLogicalBottom));
                        }
                        LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                        LayoutUnit oldLogicalTop = logicalTopForFloat(oldFloatingObject.get());
                        if (logicalTop != oldLogicalTop) {
                            changeLogicalTop = min(changeLogicalTop, min(logicalTop, oldLogicalTop));
                            changeLogicalBottom = max(changeLogicalBottom, max(logicalTop, oldLogicalTop));
                        }
                    }

                    if (oldFloatingObject->originatingLine() && !selfNeedsLayout()) {
                        ASSERT(&oldFloatingObject->originatingLine()->renderer() == this);
                        oldFloatingObject->originatingLine()->markDirty();
                    }
                } else {
                    changeLogicalTop = 0;
                    changeLogicalBottom = max(changeLogicalBottom, logicalBottom);
                }
            }
        }

        auto end = floatMap.end();
        for (auto it = floatMap.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->value.get();
            if (!floatingObject->isDescendant()) {
                changeLogicalTop = 0;
                changeLogicalBottom = max(changeLogicalBottom, logicalBottomForFloat(floatingObject));
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

    RenderStyle* styleToUse = style();
    LayoutStateMaintainer statePusher(&view(), this, locationOffset(), hasColumns() || hasTransform() || hasReflection() || styleToUse->isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged, columnInfo());

    // Regions changing widths can force us to relayout our children.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (logicalWidthChangedInRegions(flowThread))
        relayoutChildren = true;
    if (updateShapesBeforeBlockLayout())
        relayoutChildren = true;

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
        
        setHasMarginBeforeQuirk(styleToUse->hasMarginBeforeQuirk());
        setHasMarginAfterQuirk(styleToUse->hasMarginAfterQuirk());
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
            for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
                if (child->isRenderBlockFlow() && !child->isFloatingOrOutOfFlowPositioned()) {
                    RenderBlockFlow* block = toRenderBlockFlow(child);
                    if (block->lowestFloatLogicalBottom() + block->logicalTop() > newHeight)
                        addOverhangingFloats(block, false);
                }
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
    if (!didFullRepaint && repaintLogicalTop != repaintLogicalBottom && (styleToUse->visibility() == VISIBLE || enclosingLayer()->hasVisibleContent())) {
        // FIXME: We could tighten up the left and right invalidation points if we let layoutInlineChildren fill them in based off the particular lines
        // it had to lay out. We wouldn't need the hasOverflowClip() hack in that case either.
        LayoutUnit repaintLogicalLeft = logicalLeftVisualOverflow();
        LayoutUnit repaintLogicalRight = logicalRightVisualOverflow();
        if (hasOverflowClip()) {
            // If we have clipped overflow, we should use layout overflow as well, since visual overflow from lines didn't propagate to our block's overflow.
            // Note the old code did this as well but even for overflow:visible. The addition of hasOverflowClip() at least tightens up the hack a bit.
            // layoutInlineChildren should be patched to compute the entire repaint rect.
            repaintLogicalLeft = min(repaintLogicalLeft, logicalLeftLayoutOverflow());
            repaintLogicalRight = max(repaintLogicalRight, logicalRightLayoutOverflow());
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
    MarginInfo marginInfo(this, beforeEdge, afterEdge);

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout. The same is true for ruby text.
    // It doesn't get included in the normal layout process but is instead skipped.
    RenderObject* childToExclude = layoutSpecialExcludedChild(relayoutChildren);

    LayoutUnit previousFloatLogicalBottom = 0;
    maxFloatLogicalBottom = 0;

    RenderBox* next = firstChildBox();

    while (next) {
        RenderBox* child = next;
        next = child->nextSiblingBox();

        if (childToExclude == child)
            continue; // Skip this child, since it will be positioned by the specialized subclass (fieldsets and ruby runs).

        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);

        if (child->isOutOfFlowPositioned()) {
            child->containingBlock()->insertPositionedObject(child);
            adjustPositionedBlock(child, marginInfo);
            continue;
        }
        if (child->isFloating()) {
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

void RenderBlockFlow::layoutBlockChild(RenderBox* child, MarginInfo& marginInfo, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom)
{
    LayoutUnit oldPosMarginBefore = maxPositiveMarginBefore();
    LayoutUnit oldNegMarginBefore = maxNegativeMarginBefore();

    // The child is a normal flow object. Compute the margins we will use for collapsing now.
    child->computeAndSetBlockDirectionMargins(this);

    // Try to guess our correct logical top position. In most cases this guess will
    // be correct. Only if we're wrong (when we compute the real logical top position)
    // will we have to potentially relayout.
    LayoutUnit estimateWithoutPagination;
    LayoutUnit logicalTopEstimate = estimateLogicalTopPosition(child, marginInfo, estimateWithoutPagination);

    // Cache our old rect so that we can dirty the proper repaint rects if the child moves.
    LayoutRect oldRect = child->frameRect();
    LayoutUnit oldLogicalTop = logicalTopForChild(child);

#if !ASSERT_DISABLED
    LayoutSize oldLayoutDelta = view().layoutDelta();
#endif
    // Go ahead and position the child as though it didn't collapse with the top.
    setLogicalTopForChild(child, logicalTopEstimate, ApplyLayoutDelta);
    estimateRegionRangeForBoxChild(child);

    RenderBlockFlow* childBlockFlow = child->isRenderBlockFlow() ? toRenderBlockFlow(child) : 0;
    bool markDescendantsWithFloats = false;
    if (logicalTopEstimate != oldLogicalTop && !child->avoidsFloats() && childBlockFlow && childBlockFlow->containsFloats())
        markDescendantsWithFloats = true;
#if ENABLE(SUBPIXEL_LAYOUT)
    else if (UNLIKELY(logicalTopEstimate.mightBeSaturated()))
        // logicalTopEstimate, returned by estimateLogicalTopPosition, might be saturated for
        // very large elements. If it does the comparison with oldLogicalTop might yield a
        // false negative as adding and removing margins, borders etc from a saturated number
        // might yield incorrect results. If this is the case always mark for layout.
        markDescendantsWithFloats = true;
#endif
    else if (!child->avoidsFloats() || child->shrinkToAvoidFloats()) {
        // If an element might be affected by the presence of floats, then always mark it for
        // layout.
        LayoutUnit fb = max(previousFloatLogicalBottom, lowestFloatLogicalBottom());
        if (fb > logicalTopEstimate)
            markDescendantsWithFloats = true;
    }

    if (childBlockFlow) {
        if (markDescendantsWithFloats)
            childBlockFlow->markAllDescendantsWithFloatsForLayout();
        if (!child->isWritingModeRoot())
            previousFloatLogicalBottom = max(previousFloatLogicalBottom, oldLogicalTop + childBlockFlow->lowestFloatLogicalBottom());
    }

    if (!child->needsLayout())
        child->markForPaginationRelayoutIfNeeded();

    bool childHadLayout = child->everHadLayout();
    bool childNeededLayout = child->needsLayout();
    if (childNeededLayout)
        child->layout();

    // Cache if we are at the top of the block right now.
    bool atBeforeSideOfBlock = marginInfo.atBeforeSideOfBlock();

    // Now determine the correct ypos based off examination of collapsing margin
    // values.
    LayoutUnit logicalTopBeforeClear = collapseMargins(child, marginInfo);

    // Now check for clear.
    LayoutUnit logicalTopAfterClear = clearFloatsIfNeeded(child, marginInfo, oldPosMarginBefore, oldNegMarginBefore, logicalTopBeforeClear);
    
    bool paginated = view().layoutState()->isPaginated();
    if (paginated)
        logicalTopAfterClear = adjustBlockChildForPagination(logicalTopAfterClear, estimateWithoutPagination, child,
            atBeforeSideOfBlock && logicalTopBeforeClear == logicalTopAfterClear);

    setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

    // Now we have a final top position. See if it really does end up being different from our estimate.
    // clearFloatsIfNeeded can also mark the child as needing a layout even though we didn't move. This happens
    // when collapseMargins dynamically adds overhanging floats because of a child with negative margins.
    if (logicalTopAfterClear != logicalTopEstimate || child->needsLayout() || (paginated && childBlockFlow && childBlockFlow->shouldBreakAtLineToAvoidWidow())) {
        if (child->shrinkToAvoidFloats()) {
            // The child's width depends on the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child->setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childBlockFlow) {
            if (!child->avoidsFloats() && childBlockFlow->containsFloats())
                childBlockFlow->markAllDescendantsWithFloatsForLayout();
            if (!child->needsLayout())
                child->markForPaginationRelayoutIfNeeded();
        }

        // Our guess was wrong. Make the child lay itself out again.
        child->layoutIfNeeded();
    }

    if (updateRegionRangeForBoxChild(child)) {
        child->setNeedsLayout(MarkOnlyThis);
        child->layoutIfNeeded();
    }

    // We are no longer at the top of the block if we encounter a non-empty child.  
    // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
    if (marginInfo.atBeforeSideOfBlock() && !child->isSelfCollapsingBlock())
        marginInfo.setAtBeforeSideOfBlock(false);

    // Now place the child in the correct left position
    determineLogicalLeftPositionForChild(child, ApplyLayoutDelta);

    LayoutSize childOffset = child->location() - oldRect.location();
#if ENABLE(CSS_SHAPES)
    relayoutShapeDescendantIfMoved(child->isRenderBlock() ? toRenderBlock(child) : 0, childOffset);
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
        maxFloatLogicalBottom = max(maxFloatLogicalBottom, addOverhangingFloats(toRenderBlockFlow(child), !childNeededLayout));

    if (childOffset.width() || childOffset.height()) {
        view().addLayoutDelta(childOffset);

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (childHadLayout && !selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldRect);
    }

    if (!childHadLayout && child->checkForRepaintDuringLayout()) {
        child->repaint();
        child->repaintOverhangingFloats(true);
    }

    if (paginated) {
        // Check for an after page/column break.
        LayoutUnit newHeight = applyAfterBreak(child, logicalHeight(), marginInfo);
        if (newHeight != height())
            setLogicalHeight(newHeight);
    }

    ASSERT(view().layoutDeltaMatches(oldLayoutDelta));
}

void RenderBlockFlow::adjustPositionedBlock(RenderBox* child, const MarginInfo& marginInfo)
{
    bool isHorizontal = isHorizontalWritingMode();
    bool hasStaticBlockPosition = child->style()->hasStaticBlockPosition(isHorizontal);
    
    LayoutUnit logicalTop = logicalHeight();
    updateStaticInlinePositionForChild(child, logicalTop);

    if (!marginInfo.canCollapseWithMarginBefore()) {
        // Positioned blocks don't collapse margins, so add the margin provided by
        // the container now. The child's own margin is added later when calculating its logical top.
        LayoutUnit collapsedBeforePos = marginInfo.positiveMargin();
        LayoutUnit collapsedBeforeNeg = marginInfo.negativeMargin();
        logicalTop += collapsedBeforePos - collapsedBeforeNeg;
    }
    
    RenderLayer* childLayer = child->layer();
    if (childLayer->staticBlockPosition() != logicalTop) {
        childLayer->setStaticBlockPosition(logicalTop);
        if (hasStaticBlockPosition)
            child->setChildNeedsLayout(MarkOnlyThis);
    }
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

RenderBlockFlow::MarginValues RenderBlockFlow::marginValuesForChild(RenderBox* child) const
{
    LayoutUnit childBeforePositive = 0;
    LayoutUnit childBeforeNegative = 0;
    LayoutUnit childAfterPositive = 0;
    LayoutUnit childAfterNegative = 0;

    LayoutUnit beforeMargin = 0;
    LayoutUnit afterMargin = 0;

    RenderBlockFlow* childRenderBlock = child->isRenderBlockFlow() ? toRenderBlockFlow(child) : 0;
    
    // If the child has the same directionality as we do, then we can just return its
    // margins in the same direction.
    if (!child->isWritingModeRoot()) {
        if (childRenderBlock) {
            childBeforePositive = childRenderBlock->maxPositiveMarginBefore();
            childBeforeNegative = childRenderBlock->maxNegativeMarginBefore();
            childAfterPositive = childRenderBlock->maxPositiveMarginAfter();
            childAfterNegative = childRenderBlock->maxNegativeMarginAfter();
        } else {
            beforeMargin = child->marginBefore();
            afterMargin = child->marginAfter();
        }
    } else if (child->isHorizontalWritingMode() == isHorizontalWritingMode()) {
        // The child has a different directionality. If the child is parallel, then it's just
        // flipped relative to us. We can use the margins for the opposite edges.
        if (childRenderBlock) {
            childBeforePositive = childRenderBlock->maxPositiveMarginAfter();
            childBeforeNegative = childRenderBlock->maxNegativeMarginAfter();
            childAfterPositive = childRenderBlock->maxPositiveMarginBefore();
            childAfterNegative = childRenderBlock->maxNegativeMarginBefore();
        } else {
            beforeMargin = child->marginAfter();
            afterMargin = child->marginBefore();
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

LayoutUnit RenderBlockFlow::collapseMargins(RenderBox* child, MarginInfo& marginInfo)
{
    bool childDiscardMarginBefore = mustDiscardMarginBeforeForChild(child);
    bool childDiscardMarginAfter = mustDiscardMarginAfterForChild(child);
    bool childIsSelfCollapsing = child->isSelfCollapsingBlock();

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
        posTop = max(posTop, childMargins.positiveMarginAfter());
        negTop = max(negTop, childMargins.negativeMarginAfter());
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
                setMaxMarginBeforeValues(max(posTop, maxPositiveMarginBefore()), max(negTop, maxNegativeMarginBefore()));

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
    if (childIsSelfCollapsing) {
        // For a self collapsing block both the before and after margins get discarded. The block doesn't contribute anything to the height of the block.
        // Also, the child's top position equals the logical height of the container.
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child has no height. We need to compute our
            // position before we collapse the child's margins together,
            // so that we can get an accurate position for the zero-height block.
            LayoutUnit collapsedBeforePos = max(marginInfo.positiveMargin(), childMargins.positiveMarginBefore());
            LayoutUnit collapsedBeforeNeg = max(marginInfo.negativeMargin(), childMargins.negativeMarginBefore());
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
            LayoutUnit separateMargin = !marginInfo.canCollapseWithMarginBefore() ? marginInfo.margin() : LayoutUnit(0);
            setLogicalHeight(logicalHeight() + separateMargin + marginBeforeForChild(child));
            logicalTop = logicalHeight();
        } else if (!marginInfo.discardMargin() && (!marginInfo.atBeforeSideOfBlock()
            || (!marginInfo.canCollapseMarginBeforeWithChildren()
            && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginBeforeQuirk())))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            setLogicalHeight(logicalHeight() + max(marginInfo.positiveMargin(), posTop) - max(marginInfo.negativeMargin(), negTop));
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
        logicalTop = min(logicalTop, nextPageLogicalTop(beforeCollapseLogicalTop));
        setLogicalHeight(logicalHeight() + (logicalTop - oldLogicalTop));
    }

    // If we have collapsed into a previous sibling and so reduced the height of the parent, ensure any floats that now
    // overhang from the previous sibling are added to our parent. If the child's previous sibling itself is a float the child will avoid
    // or clear it anyway, so don't worry about any floating children it may contain.
    LayoutUnit oldLogicalHeight = logicalHeight();
    setLogicalHeight(logicalTop);
    RenderObject* prev = child->previousSibling();
    if (prev && prev->isRenderBlockFlow() && !prev->isFloatingOrOutOfFlowPositioned()) {
        RenderBlockFlow* block = toRenderBlockFlow(prev);
        if (block->containsFloats() && !block->avoidsFloats() && (block->logicalTop() + block->lowestFloatLogicalBottom()) > logicalTop) 
            addOverhangingFloats(block, false);
    }
    setLogicalHeight(oldLogicalHeight);

    return logicalTop;
}

LayoutUnit RenderBlockFlow::clearFloatsIfNeeded(RenderBox* child, MarginInfo& marginInfo, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos)
{
    LayoutUnit heightIncrease = getClearDelta(child, yPos);
    if (!heightIncrease)
        return yPos;

    if (child->isSelfCollapsingBlock()) {
        bool childDiscardMargin = mustDiscardMarginBeforeForChild(child) || mustDiscardMarginAfterForChild(child);

        // For self-collapsing blocks that clear, they can still collapse their
        // margins with following siblings. Reset the current margins to represent
        // the self-collapsing block's margins only.
        // If DISCARD is specified for -webkit-margin-collapse, reset the margin values.
        if (!childDiscardMargin) {
            MarginValues childMargins = marginValuesForChild(child);
            marginInfo.setPositiveMargin(max(childMargins.positiveMarginBefore(), childMargins.positiveMarginAfter()));
            marginInfo.setNegativeMargin(max(childMargins.negativeMarginBefore(), childMargins.negativeMarginAfter()));
        } else
            marginInfo.clearMargin();
        marginInfo.setDiscardMargin(childDiscardMargin);

        // CSS2.1 states:
        // "If the top and bottom margins of an element with clearance are adjoining, its margins collapse with 
        // the adjoining margins of following siblings but that resulting margin does not collapse with the bottom margin of the parent block."
        // So the parent's bottom margin cannot collapse through this block or any subsequent self-collapsing blocks. Check subsequent siblings
        // for a block with height - if none is found then don't allow the margins to collapse with the parent.
        bool wouldCollapseMarginsWithParent = marginInfo.canCollapseMarginAfterWithChildren();
        for (RenderBox* curr = child->nextSiblingBox(); curr && wouldCollapseMarginsWithParent; curr = curr->nextSiblingBox()) {
            if (!curr->isFloatingOrOutOfFlowPositioned() && !curr->isSelfCollapsingBlock())
                wouldCollapseMarginsWithParent = false;
        }
        if (wouldCollapseMarginsWithParent)
            marginInfo.setCanCollapseMarginAfterWithChildren(false);

        // CSS2.1: "the amount of clearance is set so that clearance + margin-top = [height of float], i.e., clearance = [height of float] - margin-top"
        // Move the top of the child box to the bottom of the float ignoring the child's top margin.
        LayoutUnit collapsedMargin = collapsedMarginBeforeForChild(child);
        setLogicalHeight(child->logicalTop() - collapsedMargin);
        // A negative collapsed margin-top value cancels itself out as it has already been factored into |yPos| above.
        heightIncrease -= max(LayoutUnit(), collapsedMargin);
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
        setMustDiscardMarginBefore(style()->marginBeforeCollapse() == MDISCARD);
    }

    LayoutUnit logicalTop = yPos + heightIncrease;
    // After margin collapsing, one of our floats may now intrude into the child. If the child doesn't contain floats of its own it
    // won't get picked up for relayout even though the logical top estimate was wrong - so add the newly intruding float now.
    if (containsFloats() && child->isRenderBlockFlow() && !toRenderBlockFlow(child)->containsFloats() && !child->avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
        toRenderBlockFlow(child)->addIntrudingFloats(this, logicalLeftOffsetForContent(), logicalTop);

    return logicalTop;
}

void RenderBlockFlow::marginBeforeEstimateForChild(RenderBox* child, LayoutUnit& positiveMarginBefore, LayoutUnit& negativeMarginBefore, bool& discardMarginBefore) const
{
    // Give up if in quirks mode and we're a body/table cell and the top margin of the child box is quirky.
    // Give up if the child specified -webkit-margin-collapse: separate that prevents collapsing.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if ((document().inQuirksMode() && hasMarginAfterQuirk(child) && (isTableCell() || isBody())) || child->style()->marginBeforeCollapse() == MSEPARATE)
        return;

    // The margins are discarded by a child that specified -webkit-margin-collapse: discard.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if (child->style()->marginBeforeCollapse() == MDISCARD) {
        positiveMarginBefore = 0;
        negativeMarginBefore = 0;
        discardMarginBefore = true;
        return;
    }

    LayoutUnit beforeChildMargin = marginBeforeForChild(child);
    positiveMarginBefore = max(positiveMarginBefore, beforeChildMargin);
    negativeMarginBefore = max(negativeMarginBefore, -beforeChildMargin);

    if (!child->isRenderBlockFlow())
        return;
    
    RenderBlockFlow* childBlock = toRenderBlockFlow(child);
    if (childBlock->childrenInline() || childBlock->isWritingModeRoot())
        return;

    MarginInfo childMarginInfo(childBlock, childBlock->borderAndPaddingBefore(), childBlock->borderAndPaddingAfter());
    if (!childMarginInfo.canCollapseMarginBeforeWithChildren())
        return;

    RenderBox* grandchildBox = childBlock->firstChildBox();
    for ( ; grandchildBox; grandchildBox = grandchildBox->nextSiblingBox()) {
        if (!grandchildBox->isFloatingOrOutOfFlowPositioned())
            break;
    }
    
    // Give up if there is clearance on the box, since it probably won't collapse into us.
    if (!grandchildBox || grandchildBox->style()->clear() != CNONE)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(this);
        if (grandchildBox->isRenderBlock()) {
            RenderBlock* grandchildBlock = toRenderBlock(grandchildBox);
            grandchildBlock->setHasMarginBeforeQuirk(grandchildBox->style()->hasMarginBeforeQuirk());
            grandchildBlock->setHasMarginAfterQuirk(grandchildBox->style()->hasMarginAfterQuirk());
        }
    }

    // Collapse the margin of the grandchild box with our own to produce an estimate.
    childBlock->marginBeforeEstimateForChild(grandchildBox, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
}

LayoutUnit RenderBlockFlow::estimateLogicalTopPosition(RenderBox* child, const MarginInfo& marginInfo, LayoutUnit& estimateWithoutPagination)
{
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    LayoutUnit logicalTopEstimate = logicalHeight();
    if (!marginInfo.canCollapseWithMarginBefore()) {
        LayoutUnit positiveMarginBefore = 0;
        LayoutUnit negativeMarginBefore = 0;
        bool discardMarginBefore = false;
        if (child->selfNeedsLayout()) {
            // Try to do a basic estimation of how the collapse is going to go.
            marginBeforeEstimateForChild(child, positiveMarginBefore, negativeMarginBefore, discardMarginBefore);
        } else {
            // Use the cached collapsed margin values from a previous layout. Most of the time they
            // will be right.
            MarginValues marginValues = marginValuesForChild(child);
            positiveMarginBefore = max(positiveMarginBefore, marginValues.positiveMarginBefore());
            negativeMarginBefore = max(negativeMarginBefore, marginValues.negativeMarginBefore());
            discardMarginBefore = mustDiscardMarginBeforeForChild(child);
        }

        // Collapse the result with our current margins.
        if (!discardMarginBefore)
            logicalTopEstimate += max(marginInfo.positiveMargin(), positiveMarginBefore) - max(marginInfo.negativeMargin(), negativeMarginBefore);
    }

    // Adjust logicalTopEstimate down to the next page if the margins are so large that we don't fit on the current
    // page.
    LayoutState* layoutState = view().layoutState();
    if (layoutState->isPaginated() && layoutState->pageLogicalHeight() && logicalTopEstimate > logicalHeight()
        && hasNextPage(logicalHeight()))
        logicalTopEstimate = min(logicalTopEstimate, nextPageLogicalTop(logicalHeight()));

    logicalTopEstimate += getClearDelta(child, logicalTopEstimate);
    
    estimateWithoutPagination = logicalTopEstimate;

    if (layoutState->isPaginated()) {
        // If the object has a page or column break value of "before", then we should shift to the top of the next page.
        logicalTopEstimate = applyBeforeBreak(child, logicalTopEstimate);
    
        // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
        logicalTopEstimate = adjustForUnsplittableChild(child, logicalTopEstimate);
        
        if (!child->selfNeedsLayout() && child->isRenderBlock())
            logicalTopEstimate += toRenderBlock(child)->paginationStrut();
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
        setMaxMarginAfterValues(max(maxPositiveMarginAfter(), marginInfo.positiveMargin()), max(maxNegativeMarginAfter(), marginInfo.negativeMargin()));

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

    // If we can't collapse with children then go ahead and add in the bottom margin.
    if (!marginInfo.discardMargin() && (!marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()
        && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginAfterQuirk())))
        setLogicalHeight(logicalHeight() + marginInfo.margin());
        
    // Now add in our bottom border/padding.
    setLogicalHeight(logicalHeight() + afterSide);

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    setLogicalHeight(max(logicalHeight(), beforeSide + afterSide));

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);
}

void RenderBlockFlow::setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!m_rareData) {
        if (pos == RenderBlockFlowRareData::positiveMarginBeforeDefault(this) && neg == RenderBlockFlowRareData::negativeMarginBeforeDefault(this))
            return;
        m_rareData = adoptPtr(new RenderBlockFlowRareData(this));
    }
    m_rareData->m_margins.setPositiveMarginBefore(pos);
    m_rareData->m_margins.setNegativeMarginBefore(neg);
}

void RenderBlockFlow::setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg)
{
    if (!m_rareData) {
        if (pos == RenderBlockFlowRareData::positiveMarginAfterDefault(this) && neg == RenderBlockFlowRareData::negativeMarginAfterDefault(this))
            return;
        m_rareData = adoptPtr(new RenderBlockFlowRareData(this));
    }
    m_rareData->m_margins.setPositiveMarginAfter(pos);
    m_rareData->m_margins.setNegativeMarginAfter(neg);
}

void RenderBlockFlow::setMustDiscardMarginBefore(bool value)
{
    if (style()->marginBeforeCollapse() == MDISCARD) {
        ASSERT(value);
        return;
    }
    
    if (!m_rareData && !value)
        return;

    if (!m_rareData)
        m_rareData = adoptPtr(new RenderBlockFlowRareData(this));

    m_rareData->m_discardMarginBefore = value;
}

void RenderBlockFlow::setMustDiscardMarginAfter(bool value)
{
    if (style()->marginAfterCollapse() == MDISCARD) {
        ASSERT(value);
        return;
    }

    if (!m_rareData && !value)
        return;

    if (!m_rareData)
        m_rareData = adoptPtr(new RenderBlockFlowRareData(this));

    m_rareData->m_discardMarginAfter = value;
}

bool RenderBlockFlow::mustDiscardMarginBefore() const
{
    return style()->marginBeforeCollapse() == MDISCARD || (m_rareData && m_rareData->m_discardMarginBefore);
}

bool RenderBlockFlow::mustDiscardMarginAfter() const
{
    return style()->marginAfterCollapse() == MDISCARD || (m_rareData && m_rareData->m_discardMarginAfter);
}

bool RenderBlockFlow::mustDiscardMarginBeforeForChild(const RenderBox* child) const
{
    ASSERT(!child->selfNeedsLayout());
    if (!child->isWritingModeRoot())
        return child->isRenderBlockFlow() ? toRenderBlockFlow(child)->mustDiscardMarginBefore() : (child->style()->marginBeforeCollapse() == MDISCARD);
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isRenderBlockFlow() ? toRenderBlockFlow(child)->mustDiscardMarginAfter() : (child->style()->marginAfterCollapse() == MDISCARD);

    // FIXME: We return false here because the implementation is not geometrically complete. We have values only for before/after, not start/end.
    // In case the boxes are perpendicular we assume the property is not specified.
    return false;
}

bool RenderBlockFlow::mustDiscardMarginAfterForChild(const RenderBox* child) const
{
    ASSERT(!child->selfNeedsLayout());
    if (!child->isWritingModeRoot())
        return child->isRenderBlockFlow() ? toRenderBlockFlow(child)->mustDiscardMarginAfter() : (child->style()->marginAfterCollapse() == MDISCARD);
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return child->isRenderBlockFlow() ? toRenderBlockFlow(child)->mustDiscardMarginBefore() : (child->style()->marginBeforeCollapse() == MDISCARD);

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginBeforeForChild(const RenderBox* child) const
{
    ASSERT(!child->selfNeedsLayout());
    const RenderStyle* childStyle = child->style();
    if (!child->isWritingModeRoot())
        return childStyle->marginBeforeCollapse() == MSEPARATE;
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle->marginAfterCollapse() == MSEPARATE;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginAfterForChild(const RenderBox* child) const
{
    ASSERT(!child->selfNeedsLayout());
    const RenderStyle* childStyle = child->style();
    if (!child->isWritingModeRoot())
        return childStyle->marginAfterCollapse() == MSEPARATE;
    if (child->isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle->marginBeforeCollapse() == MSEPARATE;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

static bool inNormalFlow(RenderBox* child)
{
    RenderBlock* curr = child->containingBlock();
    while (curr && curr != &child->view()) {
        if (curr->hasColumns() || curr->isRenderFlowThread())
            return true;
        if (curr->isFloatingOrOutOfFlowPositioned())
            return false;
        curr = curr->containingBlock();
    }
    return true;
}

LayoutUnit RenderBlockFlow::applyBeforeBreak(RenderBox* child, LayoutUnit logicalOffset)
{
    // FIXME: Add page break checking here when we support printing.
    bool checkColumnBreaks = view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight; // FIXME: Once columns can print we have to check this.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    bool checkBeforeAlways = (checkColumnBreaks && child->style()->columnBreakBefore() == PBALWAYS) || (checkPageBreaks && child->style()->pageBreakBefore() == PBALWAYS)
        || (checkRegionBreaks && child->style()->regionBreakBefore() == PBALWAYS);
    if (checkBeforeAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        if (checkColumnBreaks)
            view().layoutState()->addForcedColumnBreak(child, logicalOffset);
        if (checkRegionBreaks) {
            LayoutUnit offsetBreakAdjustment = 0;
            if (flowThread->addForcedRegionBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset, child, true, &offsetBreakAdjustment))
                return logicalOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::applyAfterBreak(RenderBox* child, LayoutUnit logicalOffset, MarginInfo& marginInfo)
{
    // FIXME: Add page break checking here when we support printing.
    bool checkColumnBreaks = view().layoutState()->isPaginatingColumns();
    bool checkPageBreaks = !checkColumnBreaks && view().layoutState()->m_pageLogicalHeight; // FIXME: Once columns can print we have to check this.
    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool checkRegionBreaks = flowThread && flowThread->isRenderNamedFlowThread();
    bool checkAfterAlways = (checkColumnBreaks && child->style()->columnBreakAfter() == PBALWAYS) || (checkPageBreaks && child->style()->pageBreakAfter() == PBALWAYS)
        || (checkRegionBreaks && child->style()->regionBreakAfter() == PBALWAYS);
    if (checkAfterAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? LayoutUnit() : marginInfo.margin();

        // So our margin doesn't participate in the next collapsing steps.
        marginInfo.clearMargin();

        if (checkColumnBreaks)
            view().layoutState()->addForcedColumnBreak(child, logicalOffset);
        if (checkRegionBreaks) {
            LayoutUnit offsetBreakAdjustment = 0;
            if (flowThread->addForcedRegionBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset + marginOffset, child, false, &offsetBreakAdjustment))
                return logicalOffset + marginOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox* child, bool atBeforeSideOfBlock)
{
    RenderBlock* childRenderBlock = child->isRenderBlock() ? toRenderBlock(child) : 0;

    if (estimateWithoutPagination != logicalTopAfterClear) {
        // Our guess prior to pagination movement was wrong. Before we attempt to paginate, let's try again at the new
        // position.
        setLogicalHeight(logicalTopAfterClear);
        setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

        if (child->shrinkToAvoidFloats()) {
            // The child's width depends on the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child->setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childRenderBlock) {
            if (!child->avoidsFloats() && childRenderBlock->containsFloats())
                toRenderBlockFlow(childRenderBlock)->markAllDescendantsWithFloatsForLayout();
            if (!child->needsLayout())
                child->markForPaginationRelayoutIfNeeded();
        }

        // Our guess was wrong. Make the child lay itself out again.
        child->layoutIfNeeded();
    }

    LayoutUnit oldTop = logicalTopAfterClear;

    // If the object has a page or column break value of "before", then we should shift to the top of the next page.
    LayoutUnit result = applyBeforeBreak(child, logicalTopAfterClear);

    if (pageLogicalHeightForOffset(result)) {
        LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(result, ExcludePageBoundary);
        LayoutUnit spaceShortage = child->logicalHeight() - remainingLogicalHeight;
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
    unsigned lineCount = max<unsigned>(renderStyle->hasAutoOrphans() ? 1 : renderStyle->orphans(), renderStyle->hasAutoWidows() ? 1 : renderStyle->widows());
    if (lineCount > 1) {
        RootInlineBox* line = lastLine;
        for (unsigned i = 1; i < lineCount && line->prevRootBox(); i++)
            line = line->prevRootBox();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        LayoutRect overflow = line->logicalVisualOverflowRect(line->lineTop(), line->lineBottom());
        lineTop = min(line->lineTopWithLeading(), overflow.y());
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
    LayoutUnit logicalOffset = min(lineBox->lineTopWithLeading(), logicalVisualOverflow.y());
    LayoutUnit logicalBottom = max(lineBox->lineBottomWithLeading(), logicalVisualOverflow.maxY());
    LayoutUnit lineHeight = logicalBottom - logicalOffset;
    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(style(), lineBox, logicalOffset, logicalBottom));
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
            remainingLogicalHeight -= min(lineHeight - pageLogicalHeight, max<LayoutUnit>(0, logicalVisualOverflow.y() - lineBox->lineTopWithLeading()));
        }
        LayoutUnit totalLogicalHeight = lineHeight + max<LayoutUnit>(0, logicalOffset);
        LayoutUnit pageLogicalHeightAtNewOffset = hasUniformPageLogicalHeight ? pageLogicalHeight : pageLogicalHeightForOffset(logicalOffset + remainingLogicalHeight);
        setPageBreak(logicalOffset, lineHeight - remainingLogicalHeight);
        if (((lineBox == firstRootBox() && totalLogicalHeight < pageLogicalHeightAtNewOffset) || (!style()->hasAutoOrphans() && style()->orphans() >= lineIndex))
            && !isOutOfFlowPositioned() && !isTableCell())
            setPaginationStrut(remainingLogicalHeight + max<LayoutUnit>(0, logicalOffset));
        else {
            delta += remainingLogicalHeight;
            lineBox->setPaginationStrut(remainingLogicalHeight);
            lineBox->setIsFirstAfterPageBreak(true);
        }
    } else if (remainingLogicalHeight == pageLogicalHeight && lineBox != firstRootBox())
        lineBox->setIsFirstAfterPageBreak(true);
}

void RenderBlockFlow::setBreakAtLineToAvoidWidow(int lineToBreak)
{
    ASSERT(lineToBreak >= 0);
    if (!m_rareData)
        m_rareData = adoptPtr(new RenderBlockFlowRareData(this));

    ASSERT(!m_rareData->m_didBreakAtLineToAvoidWidow);
    m_rareData->m_lineBreakToAvoidWidow = lineToBreak;
}

void RenderBlockFlow::setDidBreakAtLineToAvoidWidow()
{
    ASSERT(!shouldBreakAtLineToAvoidWidow());
    if (!m_rareData)
        return;

    m_rareData->m_didBreakAtLineToAvoidWidow = true;
}

void RenderBlockFlow::clearDidBreakAtLineToAvoidWidow()
{
    if (!m_rareData)
        return;

    m_rareData->m_didBreakAtLineToAvoidWidow = false;
}

void RenderBlockFlow::clearShouldBreakAtLineToAvoidWidow() const
{
    ASSERT(shouldBreakAtLineToAvoidWidow());
    if (!m_rareData)
        return;

    m_rareData->m_lineBreakToAvoidWidow = -1;
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

void RenderBlockFlow::layoutLineGridBox()
{
    if (style()->lineGrid() == RenderStyle::initialLineGrid()) {
        setLineGridBox(0);
        return;
    }
    
    setLineGridBox(0);

    RootInlineBox* lineGridBox = new (renderArena()) RootInlineBox(*this);
    lineGridBox->setHasTextChildren(); // Needed to make the line ascent/descent actually be honored in quirks mode.
    lineGridBox->setConstructed();
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    VerticalPositionCache verticalPositionCache;
    lineGridBox->alignBoxesInBlockDirection(logicalHeight(), textBoxDataMap, verticalPositionCache);
    
    setLineGridBox(lineGridBox);
    
    // FIXME: If any of the characteristics of the box change compared to the old one, then we need to do a deep dirtying
    // (similar to what happens when the page height changes). Ideally, though, we only do this if someone is actually snapping
    // to this grid.
}

bool RenderBlockFlow::containsFloat(RenderBox* renderer) const
{
    return m_floatingObjects && m_floatingObjects->set().contains<RenderBox&, FloatingObjectHashTranslator>(*renderer);
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
        auto end = floatingObjectSet.end();

        for (auto curr = parent(); curr && !curr->isRenderView(); curr = curr->parent()) {
            if (curr->isRenderBlockFlow()) {
                RenderBlockFlow* currBlock = toRenderBlockFlow(curr);

                if (currBlock->hasOverhangingFloats()) {
                    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
                        RenderBox& renderer = (*it)->renderer();
                        if (currBlock->hasOverhangingFloat(&renderer)) {
                            parentBlock = currBlock;
                            break;
                        }
                    }
                }
            }
        }

        parentBlock->markAllDescendantsWithFloatsForLayout();
        parentBlock->markSiblingsWithFloatsForLayout();
    }
}

void RenderBlockFlow::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    RenderStyle* oldStyle = style();
    s_canPropagateFloatIntoSibling = oldStyle ? !isFloatingOrOutOfFlowPositioned() && !avoidsFloats() : false;

    if (oldStyle && parent() && diff == StyleDifferenceLayout && oldStyle->position() != newStyle->position()) {
        if (containsFloats() && !isFloating() && !isOutOfFlowPositioned() && newStyle->hasOutOfFlowPosition())
            markAllDescendantsWithFloatsForLayout();
    }

    RenderBlock::styleWillChange(diff, newStyle);
}

void RenderBlockFlow::deleteLineBoxTree()
{
    if (containsFloats())
        m_floatingObjects->clearLineBoxTreePointers();
    RenderBlock::deleteLineBoxTree();
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
            if (toBlockFlow->containsFloat(&floatingObject->renderer()))
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


void RenderBlockFlow::clipOutFloatingObjects(RenderBlock* rootBlock, const PaintInfo* paintInfo, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto end = floatingObjectSet.end();
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* floatingObject = it->get();
            LayoutRect floatBox(offsetFromRootBlock.width() + xPositionForFloatIncludingMargin(floatingObject),
                offsetFromRootBlock.height() + yPositionForFloatIncludingMargin(floatingObject),
                floatingObject->renderer().width(), floatingObject->renderer().height());
            rootBlock->flipForWritingMode(floatBox);
            floatBox.move(rootBlockPhysicalPosition.x(), rootBlockPhysicalPosition.y());
            paintInfo->context->clipOut(pixelSnappedIntRect(floatBox));
        }
    }
}

void RenderBlockFlow::createFloatingObjects()
{
    m_floatingObjects = adoptPtr(new FloatingObjects(this, isHorizontalWritingMode()));
}

void RenderBlockFlow::removeFloatingObjects()
{
    if (!m_floatingObjects)
        return;

    m_floatingObjects->clear();
}

FloatingObject* RenderBlockFlow::insertFloatingObject(RenderBox* floatBox)
{
    ASSERT(floatBox->isFloating());

    // Create the list of special objects if we don't aleady have one
    if (!m_floatingObjects)
        createFloatingObjects();
    else {
        // Don't insert the floatingObject again if it's already in the list
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(*floatBox);
        if (it != floatingObjectSet.end())
            return it->get();
    }

    // Create the special floatingObject entry & append it to the list

    std::unique_ptr<FloatingObject> floatingObject = FloatingObject::create(*floatBox);
    
    // Our location is irrelevant if we're unsplittable or no pagination is in effect.
    // Just go ahead and lay out the float.
    bool isChildRenderBlock = floatBox->isRenderBlock();
    if (isChildRenderBlock && !floatBox->needsLayout() && view().layoutState()->pageLogicalHeightChanged())
        floatBox->setChildNeedsLayout(MarkOnlyThis);
            
    bool needsBlockDirectionLocationSetBeforeLayout = isChildRenderBlock && view().layoutState()->needsBlockDirectionLocationSetBeforeLayout();
    if (!needsBlockDirectionLocationSetBeforeLayout || isWritingModeRoot()) // We are unsplittable if we're a block flow root.
        floatBox->layoutIfNeeded();
    else {
        floatBox->updateLogicalWidth();
        floatBox->computeAndSetBlockDirectionMargins(this);
    }

    setLogicalWidthForFloat(floatingObject.get(), logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));

#if ENABLE(CSS_SHAPES)
    if (ShapeOutsideInfo* shapeOutside = floatBox->shapeOutsideInfo())
        shapeOutside->setShapeSize(logicalWidthForChild(floatBox), logicalHeightForChild(floatBox));
#endif

    return m_floatingObjects->add(std::move(floatingObject));
}

void RenderBlockFlow::removeFloatingObject(RenderBox* floatBox)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(*floatBox);
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
                    logicalBottom = max(logicalBottom, logicalTop + 1);
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

LayoutPoint RenderBlockFlow::computeLogicalLocationForFloat(const FloatingObject* floatingObject, LayoutUnit logicalTopOffset) const
{
    RenderBox* childBox = &floatingObject->renderer();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
    LayoutUnit logicalRightOffset; // Constant part of right offset.
#if ENABLE(CSS_SHAPES)
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

    LayoutUnit floatLogicalWidth = min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    bool insideFlowThread = flowThreadContainingBlock();

    if (childBox->style()->floating() == LeftFloat) {
        LayoutUnit heightRemainingLeft = 1;
        LayoutUnit heightRemainingRight = 1;
        floatLogicalLeft = logicalLeftOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        while (logicalRightOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight) - floatLogicalLeft < floatLogicalWidth) {
            logicalTopOffset += min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        floatLogicalLeft = max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
    } else {
        LayoutUnit heightRemainingLeft = 1;
        LayoutUnit heightRemainingRight = 1;
        floatLogicalLeft = logicalRightOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
        while (floatLogicalLeft - logicalLeftOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft) < floatLogicalWidth) {
            logicalTopOffset += min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForLineIgnoringShapeOutside(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
            if (insideFlowThread) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
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
        logicalTop = max(logicalTopForFloat(lastPlacedFloatingObject), logicalTop);

    auto end = floatingObjectSet.end();
    // Now walk through the set of unpositioned floats and place them.
    for (; it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        if (floatingObject->renderer().containingBlock() != this)
            continue;

        RenderBox* childBox = &floatingObject->renderer();

        LayoutUnit childLogicalLeftMargin = style()->isLeftToRightDirection() ? marginStartForChild(childBox) : marginEndForChild(childBox);

        LayoutRect oldRect = childBox->frameRect();

        if (childBox->style()->clear() & CLEFT)
            logicalTop = max(lowestFloatLogicalBottom(FloatingObject::FloatLeft), logicalTop);
        if (childBox->style()->clear() & CRIGHT)
            logicalTop = max(lowestFloatLogicalBottom(FloatingObject::FloatRight), logicalTop);

        LayoutPoint floatLogicalLocation = computeLogicalLocationForFloat(floatingObject, logicalTop);

        setLogicalLeftForFloat(floatingObject, floatLogicalLocation.x());

        setLogicalLeftForChild(childBox, floatLogicalLocation.x() + childLogicalLeftMargin);
        setLogicalTopForChild(childBox, floatLogicalLocation.y() + marginBeforeForChild(childBox));

        estimateRegionRangeForBoxChild(childBox);

        LayoutState* layoutState = view().layoutState();
        bool isPaginated = layoutState->isPaginated();
        if (isPaginated && !childBox->needsLayout())
            childBox->markForPaginationRelayoutIfNeeded();
        
        childBox->layoutIfNeeded();

        if (isPaginated) {
            // If we are unsplittable and don't fit, then we need to move down.
            // We include our margins as part of the unsplittable area.
            LayoutUnit newLogicalTop = adjustForUnsplittableChild(childBox, floatLogicalLocation.y(), true);
            
            // See if we have a pagination strut that is making us move down further.
            // Note that an unsplittable child can't also have a pagination strut, so this is
            // exclusive with the case above.
            RenderBlock* childBlock = childBox->isRenderBlock() ? toRenderBlock(childBox) : 0;
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
                childBox->layoutIfNeeded();
            }

            if (updateRegionRangeForBoxChild(childBox)) {
                childBox->setNeedsLayout(MarkOnlyThis);
                childBox->layoutIfNeeded();
            }
        }

        setLogicalTopForFloat(floatingObject, floatLogicalLocation.y());

        setLogicalHeightForFloat(floatingObject, logicalHeightForChild(childBox) + marginBeforeForChild(childBox) + marginAfterForChild(childBox));

        m_floatingObjects->addPlacedObject(floatingObject);

        // If the child moved, we have to repaint it.
        if (childBox->checkForRepaintDuringLayout())
            childBox->repaintDuringLayoutIfMoved(oldRect);
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
    default:
        break;
    }
    if (height() < newY)
        setLogicalHeight(newY);
}

LayoutUnit RenderBlockFlow::logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode offsetMode) const
{
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        return m_floatingObjects->logicalLeftOffset(fixedOffset, logicalTop, logicalHeight, offsetMode, heightRemaining);

    return fixedOffset;
}

LayoutUnit RenderBlockFlow::logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode offsetMode) const
{
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        return m_floatingObjects->logicalRightOffset(fixedOffset, logicalTop, logicalHeight, offsetMode, heightRemaining);

    return fixedOffset;
}

LayoutUnit RenderBlockFlow::nextFloatLogicalBottomBelow(LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode offsetMode) const
{
    if (!m_floatingObjects)
        return logicalHeight;

    LayoutUnit bottom = LayoutUnit::max();
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        LayoutUnit floatBottom = logicalBottomForFloat(floatingObject);
#if ENABLE(CSS_SHAPES)
        ShapeOutsideInfo* shapeOutside = floatingObject->renderer().shapeOutsideInfo();
        if (offsetMode == ShapeOutsideFloatShapeOffset && shapeOutside) {
            LayoutUnit shapeBottom = logicalTopForFloat(floatingObject) + marginBeforeForChild(&(floatingObject->renderer())) + shapeOutside->shapeLogicalBottom();
            // Use the shapeBottom unless it extends outside of the margin box, in which case it is clipped.
            if (shapeBottom < floatBottom)
                floatBottom = shapeBottom;
        }
#endif
        if (floatBottom > logicalHeight)
            bottom = min(floatBottom, bottom);
    }

    return bottom == LayoutUnit::max() ? LayoutUnit() : bottom;
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
            lowestFloatBottom = max(lowestFloatBottom, logicalBottomForFloat(floatingObject));
    }
    return lowestFloatBottom;
}

LayoutUnit RenderBlockFlow::addOverhangingFloats(RenderBlockFlow* child, bool makeChildPaintOtherFloats)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (child->hasOverflowClip() || !child->containsFloats() || child->isRoot() || child->hasColumns() || child->isWritingModeRoot())
        return 0;

    LayoutUnit childLogicalTop = child->logicalTop();
    LayoutUnit childLogicalLeft = child->logicalLeft();
    LayoutUnit lowestFloatLogicalBottom = 0;

    // Floats that will remain the child's responsibility to paint should factor into its
    // overflow.
    auto childEnd = child->m_floatingObjects->set().end();
    for (auto childIt = child->m_floatingObjects->set().begin(); childIt != childEnd; ++childIt) {
        FloatingObject* floatingObject = childIt->get();
        LayoutUnit floatLogicalBottom = min(logicalBottomForFloat(floatingObject), LayoutUnit::max() - childLogicalTop);
        LayoutUnit logicalBottom = childLogicalTop + floatLogicalBottom;
        lowestFloatLogicalBottom = max(lowestFloatLogicalBottom, logicalBottom);

        if (logicalBottom > logicalHeight()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(&floatingObject->renderer())) {
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
                && floatingObject->renderer().isDescendantOf(child) && floatingObject->renderer().enclosingFloatPaintingLayer() == child->enclosingFloatPaintingLayer()) {
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
                child->addOverflowFromChild(&floatingObject->renderer(), LayoutSize(xPositionForFloatIncludingMargin(floatingObject), yPositionForFloatIncludingMargin(floatingObject)));
        }
    }
    return lowestFloatLogicalBottom;
}

bool RenderBlockFlow::hasOverhangingFloat(RenderBox* renderer)
{
    if (!m_floatingObjects || hasColumns() || !parent())
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(*renderer);
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
        removeFloatingObject(floatToRemove);

    // Iterate over our children and mark them as needed.
    if (!childrenInline()) {
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if ((!floatToRemove && child->isFloatingOrOutOfFlowPositioned()) || !child->isRenderBlock())
                continue;
            if (!child->isRenderBlockFlow()) {
                RenderBlock* childBlock = toRenderBlock(child);
                if (childBlock->shrinkToAvoidFloats() && childBlock->everHadLayout())
                    childBlock->setChildNeedsLayout(markParents);
                continue;
            }
            RenderBlockFlow* childBlock = toRenderBlockFlow(child);
            if ((floatToRemove ? childBlock->containsFloat(floatToRemove) : childBlock->containsFloats()) || childBlock->shrinkToAvoidFloats())
                childBlock->markAllDescendantsWithFloatsForLayout(floatToRemove, inLayout);
        }
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
            RenderBox* floatingBox = &(*it)->renderer();
            if (floatToRemove && floatingBox != floatToRemove)
                continue;
            if (nextBlock->containsFloat(floatingBox))
                nextBlock->markAllDescendantsWithFloatsForLayout(floatingBox);
        }
    }
}

LayoutUnit RenderBlockFlow::getClearDelta(RenderBox* child, LayoutUnit logicalTop)
{
    // There is no need to compute clearance if we have no floats.
    if (!containsFloats())
        return 0;
    
    // At least one float is present. We need to perform the clearance computation.
    bool clearSet = child->style()->clear() != CNONE;
    LayoutUnit logicalBottom = 0;
    switch (child->style()->clear()) {
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
    LayoutUnit result = clearSet ? max<LayoutUnit>(0, logicalBottom - logicalTop) : LayoutUnit();
    if (!result && child->avoidsFloats()) {
        LayoutUnit newLogicalTop = logicalTop;
        while (true) {
            LayoutUnit availableLogicalWidthAtNewLogicalTopOffset = availableLogicalWidthForLine(newLogicalTop, false, logicalHeightForChild(child));
            if (availableLogicalWidthAtNewLogicalTopOffset == availableLogicalWidthForContent(newLogicalTop))
                return newLogicalTop - logicalTop;

            RenderRegion* region = regionAtBlockOffset(logicalTopForChild(child));
            LayoutRect borderBox = child->borderBoxRectInRegion(region, DoNotCacheRenderBoxRegionInfo);
            LayoutUnit childLogicalWidthAtOldLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            // FIXME: None of this is right for perpendicular writing-mode children.
            LayoutUnit childOldLogicalWidth = child->logicalWidth();
            LayoutUnit childOldMarginLeft = child->marginLeft();
            LayoutUnit childOldMarginRight = child->marginRight();
            LayoutUnit childOldLogicalTop = child->logicalTop();

            child->setLogicalTop(newLogicalTop);
            child->updateLogicalWidth();
            region = regionAtBlockOffset(logicalTopForChild(child));
            borderBox = child->borderBoxRectInRegion(region, DoNotCacheRenderBoxRegionInfo);
            LayoutUnit childLogicalWidthAtNewLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            child->setLogicalTop(childOldLogicalTop);
            child->setLogicalWidth(childOldLogicalWidth);
            child->setMarginLeft(childOldMarginLeft);
            child->setMarginRight(childOldMarginRight);
            
            if (childLogicalWidthAtNewLogicalTopOffset <= availableLogicalWidthAtNewLogicalTopOffset) {
                // Even though we may not be moving, if the logical width did shrink because of the presence of new floats, then
                // we need to force a relayout as though we shifted. This happens because of the dynamic addition of overhanging floats
                // from previous siblings when negative margins exist on a child (see the addOverhangingFloats call at the end of collapseMargins).
                if (childLogicalWidthAtOldLogicalTopOffset != childLogicalWidthAtNewLogicalTopOffset)
                    child->setChildNeedsLayout(MarkOnlyThis);
                return newLogicalTop - logicalTop;
            }

            newLogicalTop = nextFloatLogicalBottomBelow(newLogicalTop);
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

void RenderBlockFlow::adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const
{
    RenderBlock::adjustForBorderFit(x, left, right);
        
    if (style()->visibility() == VISIBLE) {
        if (m_floatingObjects) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            auto end = floatingObjectSet.end();
            for (auto it = floatingObjectSet.begin(); it != end; ++it) {
                FloatingObject* r = it->get();
                // Only examine the object if our m_shouldPaint flag is set.
                if (r->shouldPaint()) {
                    LayoutUnit floatLeft = xPositionForFloatIncludingMargin(r) - r->renderer().x();
                    LayoutUnit floatRight = floatLeft + r->renderer().width();
                    left = min(left, floatLeft);
                    right = max(right, floatRight);
                }
            }
        }
    }
}


} // namespace WebCore
