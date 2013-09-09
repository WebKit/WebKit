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
#include "LayoutRepainter.h"
#include "RenderFlowThread.h"
#include "RenderLayer.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

RenderBlockFlow::RenderBlockFlow(Element* element)
    : RenderBlock(element)
{
}

RenderBlockFlow::~RenderBlockFlow()
{
}

void RenderBlockFlow::clearFloats()
{
    if (m_floatingObjects)
        m_floatingObjects->setHorizontalWritingMode(isHorizontalWritingMode());

    HashSet<RenderBox*> oldIntrudingFloatSet;
    if (!childrenInline() && m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        FloatingObjectSetIterator end = floatingObjectSet.end();
        for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
            FloatingObject* floatingObject = *it;
            if (!floatingObject->isDescendant())
                oldIntrudingFloatSet.add(floatingObject->renderer());
        }
    }

    // Inline blocks are covered by the isReplaced() check in the avoidFloats method.
    if (avoidsFloats() || isRoot() || isRenderView() || isFloatingOrOutOfFlowPositioned() || isTableCell()) {
        if (m_floatingObjects) {
            deleteAllValues(m_floatingObjects->set());
            m_floatingObjects->clear();
        }
        if (!oldIntrudingFloatSet.isEmpty())
            markAllDescendantsWithFloatsForLayout();
        return;
    }

    typedef HashMap<RenderObject*, FloatingObject*> RendererToFloatInfoMap;
    RendererToFloatInfoMap floatMap;

    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        if (childrenInline()) {
            FloatingObjectSetIterator end = floatingObjectSet.end();
            for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
                FloatingObject* f = *it;
                floatMap.add(f->renderer(), f);
            }
        } else
            deleteAllValues(floatingObjectSet);
        m_floatingObjects->clear();
    }

    // We should not process floats if the parent node is not a RenderBlock. Otherwise, we will add 
    // floats in an invalid context. This will cause a crash arising from a bad cast on the parent.
    // See <rdar://problem/8049753>, where float property is applied on a text node in a SVG.
    if (!parent() || !parent()->isRenderBlock())
        return;

    // Attempt to locate a previous sibling with overhanging floats. We skip any elements that are
    // out of flow (like floating/positioned elements), and we also skip over any objects that may have shifted
    // to avoid floats.
    RenderBlock* parentBlock = toRenderBlock(parent());
    bool parentHasFloats = false;
    RenderObject* prev = previousSibling();
    while (prev && (prev->isFloatingOrOutOfFlowPositioned() || !prev->isBox() || !prev->isRenderBlock() || toRenderBlock(prev)->avoidsFloats())) {
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
    RenderBlock* block = toRenderBlock(prev);
    if (block->m_floatingObjects && block->lowestFloatLogicalBottom() > logicalTopOffset)
        addIntrudingFloats(block, logicalLeftOffset, logicalTopOffset);

    if (childrenInline()) {
        LayoutUnit changeLogicalTop = LayoutUnit::max();
        LayoutUnit changeLogicalBottom = LayoutUnit::min();
        if (m_floatingObjects) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            FloatingObjectSetIterator end = floatingObjectSet.end();
            for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
                FloatingObject* f = *it;
                FloatingObject* oldFloatingObject = floatMap.get(f->renderer());
                LayoutUnit logicalBottom = f->logicalBottom(isHorizontalWritingMode());
                if (oldFloatingObject) {
                    LayoutUnit oldLogicalBottom = oldFloatingObject->logicalBottom(isHorizontalWritingMode());
                    if (f->logicalWidth(isHorizontalWritingMode()) != oldFloatingObject->logicalWidth(isHorizontalWritingMode()) || f->logicalLeft(isHorizontalWritingMode()) != oldFloatingObject->logicalLeft(isHorizontalWritingMode())) {
                        changeLogicalTop = 0;
                        changeLogicalBottom = max(changeLogicalBottom, max(logicalBottom, oldLogicalBottom));
                    } else {
                        if (logicalBottom != oldLogicalBottom) {
                            changeLogicalTop = min(changeLogicalTop, min(logicalBottom, oldLogicalBottom));
                            changeLogicalBottom = max(changeLogicalBottom, max(logicalBottom, oldLogicalBottom));
                        }
                        LayoutUnit logicalTop = f->logicalTop(isHorizontalWritingMode());
                        LayoutUnit oldLogicalTop = oldFloatingObject->logicalTop(isHorizontalWritingMode());
                        if (logicalTop != oldLogicalTop) {
                            changeLogicalTop = min(changeLogicalTop, min(logicalTop, oldLogicalTop));
                            changeLogicalBottom = max(changeLogicalBottom, max(logicalTop, oldLogicalTop));
                        }
                    }

                    floatMap.remove(f->renderer());
                    if (oldFloatingObject->originatingLine() && !selfNeedsLayout()) {
                        ASSERT(&oldFloatingObject->originatingLine()->renderer() == this);
                        oldFloatingObject->originatingLine()->markDirty();
                    }
                    delete oldFloatingObject;
                } else {
                    changeLogicalTop = 0;
                    changeLogicalBottom = max(changeLogicalBottom, logicalBottom);
                }
            }
        }

        RendererToFloatInfoMap::iterator end = floatMap.end();
        for (RendererToFloatInfoMap::iterator it = floatMap.begin(); it != end; ++it) {
            FloatingObject* floatingObject = (*it).value;
            if (!floatingObject->isDescendant()) {
                changeLogicalTop = 0;
                changeLogicalBottom = max(changeLogicalBottom, floatingObject->logicalBottom(isHorizontalWritingMode()));
            }
        }
        deleteAllValues(floatMap);

        markLinesDirtyInBlockRange(changeLogicalTop, changeLogicalBottom);
    } else if (!oldIntrudingFloatSet.isEmpty()) {
        // If there are previously intruding floats that no longer intrude, then children with floats
        // should also get layout because they might need their floating object lists cleared.
        if (m_floatingObjects->set().size() < oldIntrudingFloatSet.size())
            markAllDescendantsWithFloatsForLayout();
        else {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            FloatingObjectSetIterator end = floatingObjectSet.end();
            for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end && !oldIntrudingFloatSet.isEmpty(); ++it)
                oldIntrudingFloatSet.remove((*it)->renderer());
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
                    RenderBlock* block = toRenderBlock(child);
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

    setNeedsLayout(false);
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

} // namespace WebCore
