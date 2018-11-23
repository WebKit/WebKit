/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2015 Apple Inc. All rights reserved.
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
#include "FrameSelection.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "HitTestLocation.h"
#include "InlineTextBox.h"
#include "LayoutRepainter.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RenderCombineText.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderMarquee.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFunctions.h"
#include "SimpleLineLayoutPagination.h"
#include "SimpleLineLayoutResolver.h"
#include "TextAutoSizing.h"
#include "VerticalPositionCache.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderBlockFlow);

bool RenderBlock::s_canPropagateFloatIntoSibling = false;

struct SameSizeAsMarginInfo {
    uint32_t bitfields : 16;
    LayoutUnit margins[2];
};

COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginValues) == sizeof(LayoutUnit[4]), MarginValues_should_stay_small);
COMPILE_ASSERT(sizeof(RenderBlockFlow::MarginInfo) == sizeof(SameSizeAsMarginInfo), MarginInfo_should_stay_small);

// Our MarginInfo state used when laying out block children.
RenderBlockFlow::MarginInfo::MarginInfo(const RenderBlockFlow& block, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding)
    : m_atBeforeSideOfBlock(true)
    , m_atAfterSideOfBlock(false)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_determinedMarginBeforeQuirk(false)
    , m_discardMargin(false)
{
    const RenderStyle& blockStyle = block.style();
    ASSERT(block.isRenderView() || block.parent());
    m_canCollapseWithChildren = !block.createsNewFormattingContext() && !block.isRenderView();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !beforeBorderPadding && blockStyle.marginBeforeCollapse() != MarginCollapse::Separate;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    m_canCollapseMarginAfterWithChildren = m_canCollapseWithChildren && !afterBorderPadding
        && (blockStyle.logicalHeight().isAuto() && !blockStyle.logicalHeight().value()) && blockStyle.marginAfterCollapse() != MarginCollapse::Separate;
    
    m_quirkContainer = block.isTableCell() || block.isBody();

    m_discardMargin = m_canCollapseMarginBeforeWithChildren && block.mustDiscardMarginBefore();

    m_positiveMargin = (m_canCollapseMarginBeforeWithChildren && !block.mustDiscardMarginBefore()) ? block.maxPositiveMarginBefore() : 0_lu;
    m_negativeMargin = (m_canCollapseMarginBeforeWithChildren && !block.mustDiscardMarginBefore()) ? block.maxNegativeMarginBefore() : 0_lu;
}

RenderBlockFlow::RenderBlockFlow(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), RenderBlockFlowFlag)
#if ENABLE(TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    setChildrenInline(true);
}

RenderBlockFlow::RenderBlockFlow(Document& document, RenderStyle&& style)
    : RenderBlock(document, WTFMove(style), RenderBlockFlowFlag)
#if ENABLE(TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    setChildrenInline(true);
}

RenderBlockFlow::~RenderBlockFlow()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderBlockFlow::willBeDestroyed()
{
    if (!renderTreeBeingDestroyed()) {
        if (firstRootBox()) {
            // We can't wait for RenderBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            if (isSelectionBorder())
                frame().selection().setNeedsSelectionUpdate();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (auto* box = firstRootBox(); box; box = box->nextRootBox()) {
                    while (auto childBox = box->firstChild())
                        childBox->removeFromParent();
                }
            }
        } else if (parent())
            parent()->dirtyLinesFromChangedChild(*this);
    }

    m_lineBoxes.deleteLineBoxes();

    blockWillBeDestroyed();

    // NOTE: This jumps down to RenderBox, bypassing RenderBlock since it would do duplicate work.
    RenderBox::willBeDestroyed();
}

RenderBlockFlow* RenderBlockFlow::previousSiblingWithOverhangingFloats(bool& parentHasFloats) const
{
    // Attempt to locate a previous sibling with overhanging floats. We skip any elements that are
    // out of flow (like floating/positioned elements), and we also skip over any objects that may have shifted
    // to avoid floats.
    parentHasFloats = false;
    for (RenderObject* sibling = previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (is<RenderBlockFlow>(*sibling)) {
            auto& siblingBlock = downcast<RenderBlockFlow>(*sibling);
            if (!siblingBlock.avoidsFloats())
                return &siblingBlock;
        }
        if (sibling->isFloating())
            parentHasFloats = true;
    }
    return nullptr;
}

void RenderBlockFlow::rebuildFloatingObjectSetFromIntrudingFloats()
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
    if (avoidsFloats() || isDocumentElementRenderer() || isRenderView() || isFloatingOrOutOfFlowPositioned() || isTableCell()) {
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
    if (!is<RenderBlockFlow>(parent()))
        return;

    // First add in floats from the parent. Self-collapsing blocks let their parent track any floats that intrude into
    // them (as opposed to floats they contain themselves) so check for those here too.
    auto& parentBlock = downcast<RenderBlockFlow>(*parent());
    bool parentHasFloats = false;
    RenderBlockFlow* previousBlock = previousSiblingWithOverhangingFloats(parentHasFloats);
    LayoutUnit logicalTopOffset = logicalTop();
    if (parentHasFloats || (parentBlock.lowestFloatLogicalBottom() > logicalTopOffset && previousBlock && previousBlock->isSelfCollapsingBlock()))
        addIntrudingFloats(&parentBlock, &parentBlock, parentBlock.logicalLeftOffsetForContent(), logicalTopOffset);
    
    LayoutUnit logicalLeftOffset;
    if (previousBlock)
        logicalTopOffset -= previousBlock->logicalTop();
    else {
        previousBlock = &parentBlock;
        logicalLeftOffset += parentBlock.logicalLeftOffsetForContent();
    }

    // Add overhanging floats from the previous RenderBlock, but only if it has a float that intrudes into our space.    
    if (previousBlock->m_floatingObjects && previousBlock->lowestFloatLogicalBottom() > logicalTopOffset)
        addIntrudingFloats(previousBlock, &parentBlock, logicalLeftOffset, logicalTopOffset);

    if (childrenInline()) {
        LayoutUnit changeLogicalTop = LayoutUnit::max();
        LayoutUnit changeLogicalBottom = LayoutUnit::min();
        if (m_floatingObjects) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            auto end = floatingObjectSet.end();
            for (auto it = floatingObjectSet.begin(); it != end; ++it) {
                const auto& floatingObject = *it->get();
                std::unique_ptr<FloatingObject> oldFloatingObject = floatMap.take(&floatingObject.renderer());
                LayoutUnit logicalBottom = logicalBottomForFloat(floatingObject);
                if (oldFloatingObject) {
                    LayoutUnit oldLogicalBottom = logicalBottomForFloat(*oldFloatingObject);
                    if (logicalWidthForFloat(floatingObject) != logicalWidthForFloat(*oldFloatingObject) || logicalLeftForFloat(floatingObject) != logicalLeftForFloat(*oldFloatingObject)) {
                        changeLogicalTop = 0;
                        changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                    } else {
                        if (logicalBottom != oldLogicalBottom) {
                            changeLogicalTop = std::min(changeLogicalTop, std::min(logicalBottom, oldLogicalBottom));
                            changeLogicalBottom = std::max(changeLogicalBottom, std::max(logicalBottom, oldLogicalBottom));
                        }
                        LayoutUnit logicalTop = logicalTopForFloat(floatingObject);
                        LayoutUnit oldLogicalTop = logicalTopForFloat(*oldFloatingObject);
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
            const auto& floatingObject = *it->value.get();
            if (!floatingObject.isDescendant()) {
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

void RenderBlockFlow::adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!style().hasAutoColumnCount() || !style().hasAutoColumnWidth()) {
        // The min/max intrinsic widths calculated really tell how much space elements need when
        // laid out inside the columns. In order to eventually end up with the desired column width,
        // we need to convert them to values pertaining to the multicol container.
        int columnCount = style().hasAutoColumnCount() ? 1 : style().columnCount();
        LayoutUnit columnWidth;
        LayoutUnit colGap = columnGap();
        LayoutUnit gapExtra = (columnCount - 1) * colGap;
        if (style().hasAutoColumnWidth())
            minLogicalWidth = minLogicalWidth * columnCount + gapExtra;
        else {
            columnWidth = style().columnWidth();
            minLogicalWidth = std::min(minLogicalWidth, columnWidth);
        }
        // FIXME: If column-count is auto here, we should resolve it to calculate the maximum
        // intrinsic width, instead of pretending that it's 1. The only way to do that is by
        // performing a layout pass, but this is not an appropriate time or place for layout. The
        // good news is that if height is unconstrained and there are no explicit breaks, the
        // resolved column-count really should be 1.
        maxLogicalWidth = std::max(maxLogicalWidth, columnWidth) * columnCount + gapExtra;
    }
}

void RenderBlockFlow::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (childrenInline())
        computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    else
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    adjustIntrinsicLogicalWidthsForColumns(minLogicalWidth, maxLogicalWidth);

    if (!style().autoWrap() && childrenInline()) {
        // A horizontal marquee with inline children has no minimum width.
        if (layer() && layer()->marquee() && layer()->marquee()->isHorizontal())
            minLogicalWidth = 0;
    }

    if (is<RenderTableCell>(*this)) {
        Length tableCellWidth = downcast<RenderTableCell>(*this).styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(tableCellWidth.value()));
    }

    int scrollbarWidth = intrinsicScrollbarLogicalWidth();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

bool RenderBlockFlow::recomputeLogicalWidthAndColumnWidth()
{
    bool changed = recomputeLogicalWidth();

    LayoutUnit oldColumnWidth = computedColumnWidth();
    computeColumnCountAndWidth();
    
    return changed || oldColumnWidth != computedColumnWidth();
}

LayoutUnit RenderBlockFlow::columnGap() const
{
    if (style().columnGap().isNormal())
        return style().fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return valueForLength(style().columnGap().length(), availableLogicalWidth());
}

void RenderBlockFlow::computeColumnCountAndWidth()
{
    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned desiredColumnCount = 1;
    LayoutUnit desiredColumnWidth = contentLogicalWidth();

    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document().paginated() || (style().hasAutoColumnCount() && style().hasAutoColumnWidth()) || !style().hasInlineColumnAxis()) {
        setComputedColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }

    LayoutUnit availWidth = desiredColumnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = std::max<LayoutUnit>(1, style().columnWidth());
    unsigned colCount = std::max<unsigned>(1, style().columnCount());

    if (style().hasAutoColumnWidth() && !style().hasAutoColumnCount()) {
        desiredColumnCount = colCount;
        desiredColumnWidth = std::max<LayoutUnit>(0, (availWidth - ((desiredColumnCount - 1) * colGap)) / desiredColumnCount);
    } else if (!style().hasAutoColumnWidth() && style().hasAutoColumnCount()) {
        desiredColumnCount = std::max<unsigned>(1, ((availWidth + colGap) / (colWidth + colGap)).toUnsigned());
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    } else {
        desiredColumnCount = std::max<unsigned>(std::min(colCount, ((availWidth + colGap) / (colWidth + colGap)).toUnsigned()), 1);
        desiredColumnWidth = ((availWidth + colGap) / desiredColumnCount) - colGap;
    }
    setComputedColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
}

bool RenderBlockFlow::willCreateColumns(std::optional<unsigned> desiredColumnCount) const
{
    // The following types are not supposed to create multicol context.
    if (isFileUploadControl() || isTextControl() || isListBox())
        return false;
    if (isRenderSVGBlock() || isRubyRun())
        return false;
#if ENABLE(MATHML)
    if (isRenderMathMLBlock())
        return false;
#endif // ENABLE(MATHML)

    if (!firstChild())
        return false;

    if (style().styleType() != PseudoId::None)
        return false;

    // If overflow-y is set to paged-x or paged-y on the body or html element, we'll handle the paginating in the RenderView instead.
    if ((style().overflowY() == Overflow::PagedX || style().overflowY() == Overflow::PagedY) && !(isDocumentElementRenderer() || isBody()))
        return true;

    if (!style().specifiesColumns())
        return false;

    // column-axis with opposite writing direction initiates MultiColumnFlow.
    if (!style().hasInlineColumnAxis())
        return true;

    // Non-auto column-width always initiates MultiColumnFlow.
    if (!style().hasAutoColumnWidth())
        return true;

    if (desiredColumnCount)
        return desiredColumnCount.value() > 1;

    // column-count > 1 always initiates MultiColumnFlow.
    if (!style().hasAutoColumnCount())
        return style().columnCount() > 1;

    ASSERT_NOT_REACHED();
    return false;
}

void RenderBlockFlow::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    if (recomputeLogicalWidthAndColumnWidth())
        relayoutChildren = true;

    rebuildFloatingObjectSetFromIntrudingFloats();

    LayoutUnit previousHeight = logicalHeight();
    // FIXME: should this start out as borderAndPaddingLogicalHeight() + scrollbarLogicalHeight(),
    // for consistency with other render classes?
    setLogicalHeight(0);

    bool pageLogicalHeightChanged = false;
    checkForPaginationLogicalHeightChange(relayoutChildren, pageLogicalHeight, pageLogicalHeightChanged);

    LayoutUnit repaintLogicalTop;
    LayoutUnit repaintLogicalBottom;
    LayoutUnit maxFloatLogicalBottom;
    const RenderStyle& styleToUse = style();
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || styleToUse.isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged);

        preparePaginationBeforeBlockLayout(relayoutChildren);

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
        if (!firstChild() && !isAnonymousBlock())
            setChildrenInline(true);
        if (childrenInline())
            layoutInlineChildren(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
        else
            layoutBlockChildren(relayoutChildren, maxFloatLogicalBottom);
    }

    // Expand our intrinsic height to encompass floats.
    LayoutUnit toAdd = borderAndPaddingAfter() + scrollbarLogicalHeight();
    if (lowestFloatLogicalBottom() > (logicalHeight() - toAdd) && createsNewFormattingContext())
        setLogicalHeight(lowestFloatLogicalBottom() + toAdd);
    if (relayoutForPagination() || relayoutToAvoidWidows()) {
        ASSERT(!shouldBreakAtLineToAvoidWidow());
        return;
    }

    // Calculate our new height.
    LayoutUnit oldHeight = logicalHeight();
    LayoutUnit oldClientAfterEdge = clientLogicalBottom();

    // Before updating the final size of the flow thread make sure a forced break is applied after the content.
    // This ensures the size information is correctly computed for the last auto-height fragment receiving content.
    if (is<RenderFragmentedFlow>(*this))
        downcast<RenderFragmentedFlow>(*this).applyBreakAfterContent(oldClientAfterEdge);

    updateLogicalHeight();
    LayoutUnit newHeight = logicalHeight();
    {
        // FIXME: This could be removed once relayoutForPagination()/relayoutToAvoidWidows() either stop recursing or we manage to
        // re-order them.
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || styleToUse.isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged);

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
        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());
    }
    // Add overflow from children (unless we're multi-column, since in that case all our child overflow is clipped anyway).
    computeOverflow(oldClientAfterEdge);

    fitBorderToLinesIfNeeded();

    auto* state = view().frameView().layoutContext().layoutState();
    if (state && state->pageLogicalHeight())
        setPageLogicalOffset(state->pageLogicalOffset(this, logicalTop()));

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    // FIXME: This repaint logic should be moved into a separate helper function!
    // Repaint with our new bounds if they are different from our old bounds.
    bool didFullRepaint = repainter.repaintAfterLayout();
    if (!didFullRepaint && repaintLogicalTop != repaintLogicalBottom && (styleToUse.visibility() == Visibility::Visible || enclosingLayer()->hasVisibleContent())) {
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

        if (hasOverflowClip()) {
            // Adjust repaint rect for scroll offset
            repaintRect.moveBy(-scrollPosition());

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
    if (view().frameView().layoutContext().layoutState()->lineGrid() == this)
        layoutLineGridBox();

    // The margin struct caches all our current margin collapsing state.
    MarginInfo marginInfo(*this, beforeEdge, afterEdge);

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout. The same is true for ruby text.
    // It doesn't get included in the normal layout process but is instead skipped.
    layoutExcludedChildren(relayoutChildren);

    LayoutUnit previousFloatLogicalBottom;
    maxFloatLogicalBottom = 0;

    RenderBox* next = firstChildBox();

    while (next) {
        RenderBox& child = *next;
        next = child.nextSiblingBox();

        if (child.isExcludedFromNormalLayout())
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
    if (lineLayoutPath() == UndeterminedPath)
        setLineLayoutPath(SimpleLineLayout::canUseFor(*this) ? SimpleLinesPath : LineBoxesPath);

    if (lineLayoutPath() == SimpleLinesPath) {
        layoutSimpleLines(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
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
    child.computeAndSetBlockDirectionMargins(*this);

    // Try to guess our correct logical top position. In most cases this guess will
    // be correct. Only if we're wrong (when we compute the real logical top position)
    // will we have to potentially relayout.
    LayoutUnit estimateWithoutPagination;
    LayoutUnit logicalTopEstimate = estimateLogicalTopPosition(child, marginInfo, estimateWithoutPagination);

    // Cache our old rect so that we can dirty the proper repaint rects if the child moves.
    LayoutRect oldRect = child.frameRect();
    LayoutUnit oldLogicalTop = logicalTopForChild(child);

#if !ASSERT_DISABLED
    LayoutSize oldLayoutDelta = view().frameView().layoutContext().layoutDelta();
#endif
    // Position the child as though it didn't collapse with the top.
    setLogicalTopForChild(child, logicalTopEstimate, ApplyLayoutDelta);
    estimateFragmentRangeForBoxChild(child);

    RenderBlockFlow* childBlockFlow = is<RenderBlockFlow>(child) ? &downcast<RenderBlockFlow>(child) : nullptr;
    bool markDescendantsWithFloats = false;
    if (logicalTopEstimate != oldLogicalTop && !child.avoidsFloats() && childBlockFlow && childBlockFlow->containsFloats())
        markDescendantsWithFloats = true;
    else if (UNLIKELY(logicalTopEstimate.mightBeSaturated()))
        // logicalTopEstimate, returned by estimateLogicalTopPosition, might be saturated for
        // very large elements. If it does the comparison with oldLogicalTop might yield a
        // false negative as adding and removing margins, borders etc from a saturated number
        // might yield incorrect results. If this is the case always mark for layout.
        markDescendantsWithFloats = true;
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
    
    bool paginated = view().frameView().layoutContext().layoutState()->isPaginated();
    if (paginated)
        logicalTopAfterClear = adjustBlockChildForPagination(logicalTopAfterClear, estimateWithoutPagination, child, atBeforeSideOfBlock && logicalTopBeforeClear == logicalTopAfterClear);

    setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

    // Now we have a final top position. See if it really does end up being different from our estimate.
    // clearFloatsIfNeeded can also mark the child as needing a layout even though we didn't move. This happens
    // when collapseMargins dynamically adds overhanging floats because of a child with negative margins.
    if (logicalTopAfterClear != logicalTopEstimate || child.needsLayout() || (paginated && childBlockFlow && childBlockFlow->shouldBreakAtLineToAvoidWidow())) {
        if (child.shrinkToAvoidFloats()) {
            // The child's width depends on the line width. When the child shifts to clear an item, its width can
            // change (because it has more available line width). So mark the item as dirty.
            child.setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childBlockFlow) {
            if (!child.avoidsFloats() && childBlockFlow->containsFloats())
                childBlockFlow->markAllDescendantsWithFloatsForLayout();
            child.markForPaginationRelayoutIfNeeded();
        }
    }

    if (updateFragmentRangeForBoxChild(child))
        child.setNeedsLayout(MarkOnlyThis);

    // In case our guess was wrong, relayout the child.
    child.layoutIfNeeded();

    // We are no longer at the top of the block if we encounter a non-empty child.  
    // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
    if (marginInfo.atBeforeSideOfBlock() && !child.isSelfCollapsingBlock())
        marginInfo.setAtBeforeSideOfBlock(false);

    // Now place the child in the correct left position
    determineLogicalLeftPositionForChild(child, ApplyLayoutDelta);

    // Update our height now that the child has been placed in the correct position.
    setLogicalHeight(logicalHeight() + logicalHeightForChildForFragmentation(child));
    if (mustSeparateMarginAfterForChild(child)) {
        setLogicalHeight(logicalHeight() + marginAfterForChild(child));
        marginInfo.clearMargin();
    }
    // If the child has overhanging floats that intrude into following siblings (or possibly out
    // of this block), then the parent gets notified of the floats now.
    if (childBlockFlow && childBlockFlow->containsFloats())
        maxFloatLogicalBottom = std::max(maxFloatLogicalBottom, addOverhangingFloats(*childBlockFlow, !childNeededLayout));

    LayoutSize childOffset = child.location() - oldRect.location();
    if (childOffset.width() || childOffset.height()) {
        view().frameView().layoutContext().addLayoutDelta(childOffset);

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
        if (RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow())
            fragmentedFlow->fragmentedFlowDescendantBoxLaidOut(&child);
        // Check for an after page/column break.
        LayoutUnit newHeight = applyAfterBreak(child, logicalHeight(), marginInfo);
        if (newHeight != height())
            setLogicalHeight(newHeight);
    }

    ASSERT(view().frameView().layoutContext().layoutDeltaMatches(oldLayoutDelta));
}

void RenderBlockFlow::adjustPositionedBlock(RenderBox& child, const MarginInfo& marginInfo)
{
    bool isHorizontal = isHorizontalWritingMode();
    bool hasStaticBlockPosition = child.style().hasStaticBlockPosition(isHorizontal);
    
    LayoutUnit logicalTop = logicalHeight();
    updateStaticInlinePositionForChild(child, logicalTop, DoNotIndentText);

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
    RenderBlockFlow* parentBlock = downcast<RenderBlockFlow>(parent());
    if (parentBlock && style().clear() != Clear::None && parentBlock->getClearDelta(*this, logicalHeight()))
        return marginValuesForChild(*this).positiveMarginBefore();
    return 0_lu;
}

void RenderBlockFlow::determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode applyDelta)
{
    LayoutUnit startPosition = borderStart() + paddingStart();
    if (shouldPlaceBlockDirectionScrollbarOnLeft())
        startPosition += (style().isLeftToRightDirection() ? 1 : -1) * verticalScrollbarWidth();
    LayoutUnit totalAvailableLogicalWidth = borderAndPaddingLogicalWidth() + availableLogicalWidth();

    // Add in our start margin.
    LayoutUnit childMarginStart = marginStartForChild(child);
    LayoutUnit newPosition = startPosition + childMarginStart;
        
    // Some objects (e.g., tables, horizontal rules, overflow:auto blocks) avoid floats. They need
    // to shift over as necessary to dodge any floats that might get in the way.
    if (child.avoidsFloats() && containsFloats())
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
    LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? 0_lu : marginInfo.margin();
    setLogicalHeight(logicalHeight() + marginOffset);
    positionNewFloats();
    setLogicalHeight(logicalHeight() - marginOffset);
}

void RenderBlockFlow::updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop, IndentTextOrNot shouldIndentText)
{
    if (child.style().isOriginalDisplayInlineType())
        setStaticInlinePositionForChild(child, logicalTop, startAlignedOffsetForLine(logicalTop, shouldIndentText));
    else
        setStaticInlinePositionForChild(child, logicalTop, startOffsetForContent(logicalTop));
}

void RenderBlockFlow::setStaticInlinePositionForChild(RenderBox& child, LayoutUnit blockOffset, LayoutUnit inlinePosition)
{
    if (enclosingFragmentedFlow()) {
        // Shift the inline position to exclude the fragment offset.
        inlinePosition += startOffsetForContent() - startOffsetForContent(blockOffset);
    }
    child.layer()->setStaticInlinePosition(inlinePosition);
}

RenderBlockFlow::MarginValues RenderBlockFlow::marginValuesForChild(RenderBox& child) const
{
    LayoutUnit childBeforePositive;
    LayoutUnit childBeforeNegative;
    LayoutUnit childAfterPositive;
    LayoutUnit childAfterNegative;

    LayoutUnit beforeMargin;
    LayoutUnit afterMargin;

    RenderBlockFlow* childRenderBlock = is<RenderBlockFlow>(child) ? &downcast<RenderBlockFlow>(child) : nullptr;
    
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

bool RenderBlockFlow::childrenPreventSelfCollapsing() const
{
    if (!childrenInline())
        return RenderBlock::childrenPreventSelfCollapsing();

    return hasLines();
}

LayoutUnit RenderBlockFlow::collapseMargins(RenderBox& child, MarginInfo& marginInfo)
{
    return collapseMarginsWithChildInfo(&child, child.previousSibling(), marginInfo);
}

LayoutUnit RenderBlockFlow::collapseMarginsWithChildInfo(RenderBox* child, RenderObject* prevSibling, MarginInfo& marginInfo)
{
    bool childDiscardMarginBefore = child ? mustDiscardMarginBeforeForChild(*child) : false;
    bool childDiscardMarginAfter = child ? mustDiscardMarginAfterForChild(*child) : false;
    bool childIsSelfCollapsing = child ? child->isSelfCollapsingBlock() : false;
    bool beforeQuirk = child ? hasMarginBeforeQuirk(*child) : false;
    bool afterQuirk = child ? hasMarginAfterQuirk(*child) : false;
    
    // The child discards the before margin when the after margin has discarded in the case of a self collapsing block.
    childDiscardMarginBefore = childDiscardMarginBefore || (childDiscardMarginAfter && childIsSelfCollapsing);
    
    // Get the four margin values for the child and cache them.
    const MarginValues childMargins = child ? marginValuesForChild(*child) : MarginValues(0, 0, 0, 0);

    // Get our max pos and neg top margins.
    LayoutUnit posTop = childMargins.positiveMarginBefore();
    LayoutUnit negTop = childMargins.negativeMarginBefore();

    // For self-collapsing blocks, collapse our bottom margins into our
    // top to get new posTop and negTop values.
    if (childIsSelfCollapsing) {
        posTop = std::max(posTop, childMargins.positiveMarginAfter());
        negTop = std::max(negTop, childMargins.negativeMarginAfter());
    }
    
    if (marginInfo.canCollapseWithMarginBefore()) {
        if (!childDiscardMarginBefore && !marginInfo.discardMargin()) {
            // This child is collapsing with the top of the
            // block. If it has larger margin values, then we need to update
            // our own maximal values.
            if (!document().inQuirksMode() || !marginInfo.quirkContainer() || !beforeQuirk)
                setMaxMarginBeforeValues(std::max(posTop, maxPositiveMarginBefore()), std::max(negTop, maxNegativeMarginBefore()));

            // The minute any of the margins involved isn't a quirk, don't
            // collapse it away, even if the margin is smaller (www.webreference.com
            // has an example of this, a <dt> with 0.8em author-specified inside
            // a <dl> inside a <td>.
            if (!marginInfo.determinedMarginBeforeQuirk() && !beforeQuirk && (posTop - negTop)) {
                setHasMarginBeforeQuirk(false);
                marginInfo.setDeterminedMarginBeforeQuirk(true);
            }

            if (!marginInfo.determinedMarginBeforeQuirk() && beforeQuirk && !marginBefore()) {
                // We have no top margin and our top child has a quirky margin.
                // We will pick up this quirky margin and pass it through.
                // This deals with the <td><div><p> case.
                // Don't do this for a block that split two inlines though. You do
                // still apply margins in this case.
                setHasMarginBeforeQuirk(true);
            }
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
        marginInfo.setHasMarginBeforeQuirk(beforeQuirk);

    LayoutUnit beforeCollapseLogicalTop = logicalHeight();
    LayoutUnit logicalTop = beforeCollapseLogicalTop;

    LayoutUnit clearanceForSelfCollapsingBlock;
    
    // If the child's previous sibling is a self-collapsing block that cleared a float then its top border edge has been set at the bottom border edge
    // of the float. Since we want to collapse the child's top margin with the self-collapsing block's top and bottom margins we need to adjust our parent's height to match the 
    // margin top of the self-collapsing block. If the resulting collapsed margin leaves the child still intruding into the float then we will want to clear it.
    if (!marginInfo.canCollapseWithMarginBefore() && is<RenderBlockFlow>(prevSibling) && downcast<RenderBlockFlow>(*prevSibling).isSelfCollapsingBlock()) {
        clearanceForSelfCollapsingBlock = downcast<RenderBlockFlow>(*prevSibling).marginOffsetForSelfCollapsingBlock();
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
        if (child && mustSeparateMarginBeforeForChild(*child)) {
            ASSERT(!marginInfo.discardMargin() || (marginInfo.discardMargin() && !marginInfo.margin()));
            // If we are at the before side of the block and we collapse, ignore the computed margin
            // and just add the child margin to the container height. This will correctly position
            // the child inside the container.
            LayoutUnit separateMargin = !marginInfo.canCollapseWithMarginBefore() ? marginInfo.margin() : 0_lu;
            setLogicalHeight(logicalHeight() + separateMargin + marginBeforeForChild(*child));
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
            marginInfo.setHasMarginAfterQuirk(afterQuirk);
    }
    
    // If margins would pull us past the top of the next page, then we need to pull back and pretend like the margins
    // collapsed into the page edge.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (layoutState->isPaginated() && layoutState->pageLogicalHeight() && logicalTop > beforeCollapseLogicalTop
        && hasNextPage(beforeCollapseLogicalTop)) {
        LayoutUnit oldLogicalTop = logicalTop;
        logicalTop = std::min(logicalTop, nextPageLogicalTop(beforeCollapseLogicalTop));
        setLogicalHeight(logicalHeight() + (logicalTop - oldLogicalTop));
    }

    if (is<RenderBlockFlow>(prevSibling) && !prevSibling->isFloatingOrOutOfFlowPositioned()) {
        // If |child| is a self-collapsing block it may have collapsed into a previous sibling and although it hasn't reduced the height of the parent yet
        // any floats from the parent will now overhang.
        RenderBlockFlow& block = downcast<RenderBlockFlow>(*prevSibling);
        LayoutUnit oldLogicalHeight = logicalHeight();
        setLogicalHeight(logicalTop);
        if (block.containsFloats() && !block.avoidsFloats() && (block.logicalTop() + block.lowestFloatLogicalBottom()) > logicalTop)
            addOverhangingFloats(block, false);
        setLogicalHeight(oldLogicalHeight);

        // If |child|'s previous sibling is a self-collapsing block that cleared a float and margin collapsing resulted in |child| moving up
        // into the margin area of the self-collapsing block then the float it clears is now intruding into |child|. Layout again so that we can look for
        // floats in the parent that overhang |child|'s new logical top.
        bool logicalTopIntrudesIntoFloat = clearanceForSelfCollapsingBlock > 0 && logicalTop < beforeCollapseLogicalTop;
        if (child && logicalTopIntrudesIntoFloat && containsFloats() && !child->avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
            child->setNeedsLayout();
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
        setMustDiscardMarginBefore(style().marginBeforeCollapse() == MarginCollapse::Discard);
    }

    return yPos + heightIncrease;
}

void RenderBlockFlow::marginBeforeEstimateForChild(RenderBox& child, LayoutUnit& positiveMarginBefore, LayoutUnit& negativeMarginBefore, bool& discardMarginBefore) const
{
    // Give up if in quirks mode and we're a body/table cell and the top margin of the child box is quirky.
    // Give up if the child specified -webkit-margin-collapse: separate that prevents collapsing.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if ((document().inQuirksMode() && hasMarginAfterQuirk(child) && (isTableCell() || isBody())) || child.style().marginBeforeCollapse() == MarginCollapse::Separate)
        return;

    // The margins are discarded by a child that specified -webkit-margin-collapse: discard.
    // FIXME: Use writing mode independent accessor for marginBeforeCollapse.
    if (child.style().marginBeforeCollapse() == MarginCollapse::Discard) {
        positiveMarginBefore = 0;
        negativeMarginBefore = 0;
        discardMarginBefore = true;
        return;
    }

    LayoutUnit beforeChildMargin = marginBeforeForChild(child);
    positiveMarginBefore = std::max(positiveMarginBefore, beforeChildMargin);
    negativeMarginBefore = std::max(negativeMarginBefore, -beforeChildMargin);

    if (!is<RenderBlockFlow>(child))
        return;
    
    RenderBlockFlow& childBlock = downcast<RenderBlockFlow>(child);
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
    if (!grandchildBox || grandchildBox->style().clear() != Clear::None)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(*this);
        if (is<RenderBlock>(*grandchildBox)) {
            RenderBlock& grandchildBlock = downcast<RenderBlock>(*grandchildBox);
            grandchildBlock.setHasMarginBeforeQuirk(grandchildBox->style().hasMarginBeforeQuirk());
            grandchildBlock.setHasMarginAfterQuirk(grandchildBox->style().hasMarginAfterQuirk());
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
        LayoutUnit positiveMarginBefore;
        LayoutUnit negativeMarginBefore;
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
    auto* layoutState = view().frameView().layoutContext().layoutState();
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
        
        if (!child.selfNeedsLayout() && is<RenderBlock>(child))
            logicalTopEstimate += downcast<RenderBlock>(child).paginationStrut();
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
    if (is<RenderBlockFlow>(lastBlock) && downcast<RenderBlockFlow>(*lastBlock).isSelfCollapsingBlock())
        setLogicalHeight(logicalHeight() - downcast<RenderBlockFlow>(*lastBlock).marginOffsetForSelfCollapsingBlock());

    // If we can't collapse with children then add in the bottom margin.
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
    if (style().marginBeforeCollapse() == MarginCollapse::Discard) {
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
    if (style().marginAfterCollapse() == MarginCollapse::Discard) {
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
    return style().marginBeforeCollapse() == MarginCollapse::Discard || (hasRareBlockFlowData() && rareBlockFlowData()->m_discardMarginBefore);
}

bool RenderBlockFlow::mustDiscardMarginAfter() const
{
    return style().marginAfterCollapse() == MarginCollapse::Discard || (hasRareBlockFlowData() && rareBlockFlowData()->m_discardMarginAfter);
}

bool RenderBlockFlow::mustDiscardMarginBeforeForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return is<RenderBlockFlow>(child) ? downcast<RenderBlockFlow>(child).mustDiscardMarginBefore() : (child.style().marginBeforeCollapse() == MarginCollapse::Discard);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return is<RenderBlockFlow>(child) ? downcast<RenderBlockFlow>(child).mustDiscardMarginAfter() : (child.style().marginAfterCollapse() == MarginCollapse::Discard);

    // FIXME: We return false here because the implementation is not geometrically complete. We have values only for before/after, not start/end.
    // In case the boxes are perpendicular we assume the property is not specified.
    return false;
}

bool RenderBlockFlow::mustDiscardMarginAfterForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    if (!child.isWritingModeRoot())
        return is<RenderBlockFlow>(child) ? downcast<RenderBlockFlow>(child).mustDiscardMarginAfter() : (child.style().marginAfterCollapse() == MarginCollapse::Discard);
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return is<RenderBlockFlow>(child) ? downcast<RenderBlockFlow>(child).mustDiscardMarginBefore() : (child.style().marginBeforeCollapse() == MarginCollapse::Discard);

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginBeforeForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const RenderStyle& childStyle = child.style();
    if (!child.isWritingModeRoot())
        return childStyle.marginBeforeCollapse() == MarginCollapse::Separate;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginAfterCollapse() == MarginCollapse::Separate;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

bool RenderBlockFlow::mustSeparateMarginAfterForChild(const RenderBox& child) const
{
    ASSERT(!child.selfNeedsLayout());
    const RenderStyle& childStyle = child.style();
    if (!child.isWritingModeRoot())
        return childStyle.marginAfterCollapse() == MarginCollapse::Separate;
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return childStyle.marginBeforeCollapse() == MarginCollapse::Separate;

    // FIXME: See |mustDiscardMarginBeforeForChild| above.
    return false;
}

static bool inNormalFlow(RenderBox& child)
{
    RenderBlock* curr = child.containingBlock();
    while (curr && curr != &child.view()) {
        if (curr->isRenderFragmentedFlow())
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
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    bool isInsideMulticolFlow = fragmentedFlow;
    bool checkColumnBreaks = fragmentedFlow && fragmentedFlow->shouldCheckColumnBreaks();
    bool checkPageBreaks = !checkColumnBreaks && view().frameView().layoutContext().layoutState()->pageLogicalHeight(); // FIXME: Once columns can print we have to check this.
    bool checkFragmentBreaks = false;
    bool checkBeforeAlways = (checkColumnBreaks && child.style().breakBefore() == BreakBetween::Column)
        || (checkPageBreaks && alwaysPageBreak(child.style().breakBefore()));
    if (checkBeforeAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        if (checkColumnBreaks) {
            if (isInsideMulticolFlow)
                checkFragmentBreaks = true;
        }
        if (checkFragmentBreaks) {
            LayoutUnit offsetBreakAdjustment;
            if (fragmentedFlow->addForcedFragmentBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset, &child, true, &offsetBreakAdjustment))
                return logicalOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::applyAfterBreak(RenderBox& child, LayoutUnit logicalOffset, MarginInfo& marginInfo)
{
    // FIXME: Add page break checking here when we support printing.
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    bool isInsideMulticolFlow = fragmentedFlow;
    bool checkColumnBreaks = fragmentedFlow && fragmentedFlow->shouldCheckColumnBreaks();
    bool checkPageBreaks = !checkColumnBreaks && view().frameView().layoutContext().layoutState()->pageLogicalHeight(); // FIXME: Once columns can print we have to check this.
    bool checkFragmentBreaks = false;
    bool checkAfterAlways = (checkColumnBreaks && child.style().breakAfter() == BreakBetween::Column)
        || (checkPageBreaks && alwaysPageBreak(child.style().breakAfter()));
    if (checkAfterAlways && inNormalFlow(child) && hasNextPage(logicalOffset, IncludePageBoundary)) {
        LayoutUnit marginOffset = marginInfo.canCollapseWithMarginBefore() ? 0_lu : marginInfo.margin();

        // So our margin doesn't participate in the next collapsing steps.
        marginInfo.clearMargin();

        if (checkColumnBreaks) {
            if (isInsideMulticolFlow)
                checkFragmentBreaks = true;
        }
        if (checkFragmentBreaks) {
            LayoutUnit offsetBreakAdjustment;
            if (fragmentedFlow->addForcedFragmentBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset + marginOffset, &child, false, &offsetBreakAdjustment))
                return logicalOffset + marginOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox& child, bool atBeforeSideOfBlock)
{
    RenderBlock* childRenderBlock = is<RenderBlock>(child) ? &downcast<RenderBlock>(child) : nullptr;

    if (estimateWithoutPagination != logicalTopAfterClear) {
        // Our guess prior to pagination movement was wrong. Before we attempt to paginate, let's try again at the new
        // position.
        setLogicalHeight(logicalTopAfterClear);
        setLogicalTopForChild(child, logicalTopAfterClear, ApplyLayoutDelta);

        if (child.shrinkToAvoidFloats()) {
            // The child's width depends on the line width. When the child shifts to clear an item, its width can
            // change (because it has more available line width). So mark the item as dirty.
            child.setChildNeedsLayout(MarkOnlyThis);
        }
        
        if (childRenderBlock) {
            if (!child.avoidsFloats() && childRenderBlock->containsFloats())
                downcast<RenderBlockFlow>(*childRenderBlock).markAllDescendantsWithFloatsForLayout();
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
    
    LayoutUnit paginationStrut;
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

    // Similar to how we apply clearance. Boost height() to be the place where we're going to position the child.
    setLogicalHeight(logicalHeight() + (result - oldTop));
    
    // Return the final adjusted logical top.
    return result;
}

static inline LayoutUnit calculateMinimumPageHeight(const RenderStyle& renderStyle, RootInlineBox& lastLine, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // We may require a certain minimum number of lines per page in order to satisfy
    // orphans and widows, and that may affect the minimum page height.
    unsigned lineCount = std::max<unsigned>(renderStyle.hasAutoOrphans() ? 1 : renderStyle.orphans(), renderStyle.hasAutoWidows() ? 1 : renderStyle.widows());
    if (lineCount > 1) {
        RootInlineBox* line = &lastLine;
        for (unsigned i = 1; i < lineCount && line->prevRootBox(); i++)
            line = line->prevRootBox();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        LayoutRect overflow = line->logicalVisualOverflowRect(line->lineTop(), line->lineBottom());
        lineTop = std::min(line->lineTopWithLeading(), overflow.y());
    }
    return lineBottom - lineTop;
}

static inline bool needsAppleMailPaginationQuirk(RootInlineBox& lineBox)
{
    auto& renderer = lineBox.renderer();

    if (!renderer.settings().appleMailPaginationQuirkEnabled())
        return false;

    if (renderer.element() && renderer.element()->idForStyleResolution() == "messageContentContainer")
        return true;

    return false;
}

static void clearShouldBreakAtLineToAvoidWidowIfNeeded(RenderBlockFlow& blockFlow)
{
    if (!blockFlow.shouldBreakAtLineToAvoidWidow())
        return;
    blockFlow.clearShouldBreakAtLineToAvoidWidow();
    blockFlow.setDidBreakAtLineToAvoidWidow();
}

void RenderBlockFlow::adjustLinePositionForPagination(RootInlineBox* lineBox, LayoutUnit& delta, bool& overflowsFragment, RenderFragmentedFlow* fragmentedFlow)
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
    overflowsFragment = false;
    LayoutRect logicalVisualOverflow = lineBox->logicalVisualOverflowRect(lineBox->lineTop(), lineBox->lineBottom());
    LayoutUnit logicalOffset = std::min(lineBox->lineTopWithLeading(), logicalVisualOverflow.y());
    LayoutUnit logicalBottom = std::max(lineBox->lineBottomWithLeading(), logicalVisualOverflow.maxY());
    LayoutUnit lineHeight = logicalBottom - logicalOffset;
    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(style(), *lineBox, logicalOffset, logicalBottom));
    logicalOffset += delta;
    lineBox->setPaginationStrut(0);
    lineBox->setIsFirstAfterPageBreak(false);
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    bool hasUniformPageLogicalHeight = !fragmentedFlow || fragmentedFlow->fragmentsHaveUniformLogicalHeight();
    // If lineHeight is greater than pageLogicalHeight, but logicalVisualOverflow.height() still fits, we are
    // still going to add a strut, so that the visible overflow fits on a single page.
    if (!pageLogicalHeight || !hasNextPage(logicalOffset)) {
        // FIXME: In case the line aligns with the top of the page (or it's slightly shifted downwards) it will not be marked as the first line in the page.
        // From here, the fix is not straightforward because it's not easy to always determine when the current line is the first in the page.
        return;
    }

    if (hasUniformPageLogicalHeight && logicalVisualOverflow.height() > pageLogicalHeight) {
        // We are so tall that we are bigger than a page. Before we give up and just leave the line where it is, try drilling into the
        // line and computing a new height that excludes anything we consider "blank space". We will discard margins, descent, and even overflow. If we are
        // able to fit with the blank space and overflow excluded, we will give the line its own page with the highest non-blank element being aligned with the
        // top of the page.
        // FIXME: We are still honoring gigantic margins, which does leave open the possibility of blank pages caused by this heuristic. It remains to be seen whether or not
        // this will be a real-world issue. For now we don't try to deal with this problem.
        logicalOffset = intMaxForLayoutUnit;
        logicalBottom = intMinForLayoutUnit;
        lineBox->computeReplacedAndTextLineTopAndBottom(logicalOffset, logicalBottom);
        lineHeight = logicalBottom - logicalOffset;
        if (logicalOffset == intMaxForLayoutUnit || lineHeight > pageLogicalHeight) {
            // Give up. We're genuinely too big even after excluding blank space and overflow.
            clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
            return;
        }
        pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    }
    
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);
    overflowsFragment = (lineHeight > remainingLogicalHeight);

    int lineIndex = lineCount(lineBox);
    if (remainingLogicalHeight < lineHeight || (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineIndex)) {
        if (lineBreakToAvoidWidow() == lineIndex)
            clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
        // If we have a non-uniform page height, then we have to shift further possibly.
        if (!hasUniformPageLogicalHeight && !pushToNextPageWithMinimumLogicalHeight(remainingLogicalHeight, logicalOffset, lineHeight))
            return;
        if (lineHeight > pageLogicalHeight) {
            // Split the top margin in order to avoid splitting the visible part of the line.
            remainingLogicalHeight -= std::min(lineHeight - pageLogicalHeight, std::max<LayoutUnit>(0, logicalVisualOverflow.y() - lineBox->lineTopWithLeading()));
        }
        LayoutUnit remainingLogicalHeightAtNewOffset = pageRemainingLogicalHeightForOffset(logicalOffset + remainingLogicalHeight, ExcludePageBoundary);
        overflowsFragment = (lineHeight > remainingLogicalHeightAtNewOffset);
        LayoutUnit totalLogicalHeight = lineHeight + std::max<LayoutUnit>(0, logicalOffset);
        LayoutUnit pageLogicalHeightAtNewOffset = hasUniformPageLogicalHeight ? pageLogicalHeight : pageLogicalHeightForOffset(logicalOffset + remainingLogicalHeight);
        setPageBreak(logicalOffset, lineHeight - remainingLogicalHeight);
        if (((lineBox == firstRootBox() && totalLogicalHeight < pageLogicalHeightAtNewOffset) || (!style().hasAutoOrphans() && style().orphans() >= lineIndex))
            && !isOutOfFlowPositioned() && !isTableCell()) {
            auto firstRootBox = this->firstRootBox();
            auto firstRootBoxOverflowRect = firstRootBox->logicalVisualOverflowRect(firstRootBox->lineTop(), firstRootBox->lineBottom());
            auto firstLineUpperOverhang = std::max(-firstRootBoxOverflowRect.y(), 0_lu);
            if (needsAppleMailPaginationQuirk(*lineBox))
                return;
            setPaginationStrut(remainingLogicalHeight + logicalOffset + firstLineUpperOverhang);
        } else {
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

bool RenderBlockFlow::relayoutToAvoidWidows()
{
    if (!shouldBreakAtLineToAvoidWidow())
        return false;

    setEverHadLayout(true);
    layoutBlock(false);
    return true;
}

bool RenderBlockFlow::hasNextPage(LayoutUnit logicalOffset, PageBoundaryRule pageBoundaryRule) const
{
    ASSERT(view().frameView().layoutContext().layoutState() && view().frameView().layoutContext().layoutState()->isPaginated());

    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow)
        return true; // Printing and multi-column both make new pages to accommodate content.

    // See if we're in the last fragment.
    LayoutUnit pageOffset = offsetFromLogicalTopOfFirstPage() + logicalOffset;
    RenderFragmentContainer* fragment = fragmentedFlow->fragmentAtBlockOffset(this, pageOffset, true);
    if (!fragment)
        return false;

    if (fragment->isLastFragment())
        return fragment->isRenderFragmentContainerSet() || (pageBoundaryRule == IncludePageBoundary && pageOffset == fragment->logicalTopForFragmentedFlowContent());

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    fragmentedFlow->getFragmentRangeForBox(this, startFragment, endFragment);
    return (endFragment && fragment != endFragment);
}

LayoutUnit RenderBlockFlow::adjustForUnsplittableChild(RenderBox& child, LayoutUnit logicalOffset, LayoutUnit childBeforeMargin, LayoutUnit childAfterMargin)
{
    // When flexboxes are embedded inside a block flow, they don't perform any adjustments for unsplittable
    // children. We'll treat flexboxes themselves as unsplittable just to get them to paginate properly inside
    // a block flow.
    bool isUnsplittable = childBoxIsUnsplittableForFragmentation(child);
    if (!isUnsplittable && !(child.isFlexibleBox() && !downcast<RenderFlexibleBox>(child).isFlexibleBoxImpl()))
        return logicalOffset;
    
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    LayoutUnit childLogicalHeight = logicalHeightForChild(child) + childBeforeMargin + childAfterMargin;
    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    bool hasUniformPageLogicalHeight = !fragmentedFlow || fragmentedFlow->fragmentsHaveUniformLogicalHeight();
    if (isUnsplittable)
        updateMinimumPageHeight(logicalOffset, childLogicalHeight);
    if (!pageLogicalHeight || (hasUniformPageLogicalHeight && childLogicalHeight > pageLogicalHeight)
        || !hasNextPage(logicalOffset))
        return logicalOffset;
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);
    if (remainingLogicalHeight < childLogicalHeight) {
        if (!hasUniformPageLogicalHeight && !pushToNextPageWithMinimumLogicalHeight(remainingLogicalHeight, logicalOffset, childLogicalHeight))
            return logicalOffset;
        auto result = logicalOffset + remainingLogicalHeight;
        bool isInitialLetter = child.isFloating() && child.style().styleType() == PseudoId::FirstLetter && child.style().initialLetterDrop() > 0;
        if (isInitialLetter) {
            // Increase our logical height to ensure that lines all get pushed along with the letter.
            setLogicalHeight(logicalOffset + remainingLogicalHeight);
        }
        return result;
    }

    return logicalOffset;
}

bool RenderBlockFlow::pushToNextPageWithMinimumLogicalHeight(LayoutUnit& adjustment, LayoutUnit logicalOffset, LayoutUnit minimumLogicalHeight) const
{
    bool checkFragment = false;
    for (LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment); pageLogicalHeight;
        pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment)) {
        if (minimumLogicalHeight <= pageLogicalHeight)
            return true;
        if (!hasNextPage(logicalOffset + adjustment))
            return false;
        adjustment += pageLogicalHeight;
        checkFragment = true;
    }
    return !checkFragment;
}

void RenderBlockFlow::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->setPageBreak(this, offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void RenderBlockFlow::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->updateMinimumPageHeight(this, offsetFromLogicalTopOfFirstPage() + offset, minHeight);
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
    // Unsplittable objects clear out the pageLogicalHeight in the layout state as a way of signaling that no
    // pagination should occur. Therefore we have to check this first and bail if the value has been set to 0.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    LayoutUnit pageLogicalHeight = layoutState->pageLogicalHeight();
    if (!pageLogicalHeight)
        return 0;

    LayoutUnit firstPageLogicalTop = isHorizontalWritingMode() ? layoutState->pageOffset().height() : layoutState->pageOffset().width();
    LayoutUnit blockLogicalTop = isHorizontalWritingMode() ? layoutState->layoutOffset().height() : layoutState->layoutOffset().width();

    LayoutUnit cumulativeOffset = offset + blockLogicalTop;
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow)
        return cumulativeOffset - roundToInt(cumulativeOffset - firstPageLogicalTop) % roundToInt(pageLogicalHeight);
    return firstPageLogicalTop + fragmentedFlow->pageLogicalTopForOffset(cumulativeOffset - firstPageLogicalTop);
}

LayoutUnit RenderBlockFlow::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    // Unsplittable objects clear out the pageLogicalHeight in the layout state as a way of signaling that no
    // pagination should occur. Therefore we have to check this first and bail if the value has been set to 0.
    LayoutUnit pageLogicalHeight = view().frameView().layoutContext().layoutState()->pageLogicalHeight();
    if (!pageLogicalHeight)
        return 0;
    
    // Now check for a flow thread.
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow)
        return pageLogicalHeight;
    return fragmentedFlow->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
}

LayoutUnit RenderBlockFlow::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    offset += offsetFromLogicalTopOfFirstPage();
    
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow) {
        LayoutUnit pageLogicalHeight = view().frameView().layoutContext().layoutState()->pageLogicalHeight();
        LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
        if (pageBoundaryRule == IncludePageBoundary) {
            // If includeBoundaryPoint is true the line exactly on the top edge of a
            // column will act as being part of the previous column.
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }
    
    return fragmentedFlow->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}

LayoutUnit RenderBlockFlow::logicalHeightForChildForFragmentation(const RenderBox& child) const
{
    return logicalHeightForChild(child);
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
    
    setLineGridBox(WTFMove(lineGridBox));

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
    if (diff == StyleDifference::Layout && s_canPropagateFloatIntoSibling && !canPropagateFloatIntoSibling && hasOverhangingFloats()) {
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

    if (diff >= StyleDifference::Repaint) {
        // FIXME: This could use a cheaper style-only test instead of SimpleLineLayout::canUseFor.
        if (selfNeedsLayout() || !m_simpleLineLayout || !SimpleLineLayout::canUseFor(*this))
            invalidateLineLayoutPath();
    }

    if (multiColumnFlow())
        updateStylesForColumnChildren();
}

void RenderBlockFlow::updateStylesForColumnChildren()
{
    for (auto* child = firstChildBox(); child && (child->isInFlowRenderFragmentedFlow() || child->isRenderMultiColumnSet()); child = child->nextSiblingBox())
        child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), DisplayType::Block));
}

void RenderBlockFlow::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    s_canPropagateFloatIntoSibling = oldStyle ? !isFloatingOrOutOfFlowPositioned() && !avoidsFloats() : false;

    if (oldStyle) {
        auto oldPosition = oldStyle->position();
        auto newPosition = newStyle.position();

        if (parent() && diff == StyleDifference::Layout && oldPosition != newPosition) {
            if (containsFloats() && !isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
                markAllDescendantsWithFloatsForLayout();
        }
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

void RenderBlockFlow::addFloatsToNewParent(RenderBlockFlow& toBlockFlow) const
{
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
    if (!m_floatingObjects)
        return;

    if (!toBlockFlow.m_floatingObjects)
        toBlockFlow.createFloatingObjects();

    for (auto& floatingObject : m_floatingObjects->set()) {
        if (toBlockFlow.containsFloat(floatingObject->renderer()))
            continue;
        toBlockFlow.m_floatingObjects->add(floatingObject->cloneForNewParent());
    }
}

void RenderBlockFlow::addOverflowFromFloats()
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        if (floatingObject.isDescendant())
            addOverflowFromChild(&floatingObject.renderer(), floatingObject.locationOffsetOfBorderBox());
    }
}

void RenderBlockFlow::computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats)
{
    RenderBlock::computeOverflow(oldClientAfterEdge, recomputeFloats);

    if (!multiColumnFlow() && (recomputeFloats || createsNewFormattingContext() || hasSelfPaintingLayer()))
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
    LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        // Only repaint the object if it is overhanging, is not in its own layer, and
        // is our responsibility to paint (m_shouldPaint is set). When paintAllDescendants is true, the latter
        // condition is replaced with being a descendant of us.
        auto& renderer = floatingObject.renderer();
        if (logicalBottomForFloat(floatingObject) > logicalHeight()
            && !renderer.hasSelfPaintingLayer()
            && (floatingObject.shouldPaint() || (paintAllDescendants && renderer.isDescendantOf(this)))) {
            renderer.repaint();
            renderer.repaintOverhangingFloats(false);
        }
    }
}

void RenderBlockFlow::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& point)
{
    RenderBlock::paintColumnRules(paintInfo, point);
    
    if (!multiColumnFlow() || paintInfo.context().paintingDisabled())
        return;

    // Iterate over our children and paint the column rules as needed.
    for (auto& columnSet : childrenOfType<RenderMultiColumnSet>(*this)) {
        LayoutPoint childPoint = columnSet.location() + flipForWritingModeForChild(&columnSet, point);
        columnSet.paintColumnRules(paintInfo, childPoint);
    }
}

void RenderBlockFlow::paintFloats(PaintInfo& paintInfo, const LayoutPoint& paintOffset, bool preservePhase)
{
    if (!m_floatingObjects)
        return;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        auto& renderer = floatingObject.renderer();
        // Only paint the object if our m_shouldPaint flag is set.
        if (floatingObject.shouldPaint() && !renderer.hasSelfPaintingLayer()) {
            PaintInfo currentPaintInfo(paintInfo);
            currentPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhase::BlockBackground;
            LayoutPoint childPoint = flipFloatForWritingModeForChild(floatingObject, paintOffset + floatingObject.translationOffsetToAncestor());
            renderer.paint(currentPaintInfo, childPoint);
            if (!preservePhase) {
                currentPaintInfo.phase = PaintPhase::ChildBlockBackgrounds;
                renderer.paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhase::Float;
                renderer.paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhase::Foreground;
                renderer.paint(currentPaintInfo, childPoint);
                currentPaintInfo.phase = PaintPhase::Outline;
                renderer.paint(currentPaintInfo, childPoint);
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
            const auto& floatingObject = *it->get();
            LayoutRect floatBox(offsetFromRootBlock.width(), offsetFromRootBlock.height(), floatingObject.renderer().width(), floatingObject.renderer().height());
            floatBox.move(floatingObject.locationOffsetOfBorderBox());
            rootBlock.flipForWritingMode(floatBox);
            floatBox.move(rootBlockPhysicalPosition.x(), rootBlockPhysicalPosition.y());
            paintInfo->context().clipOut(snappedIntRect(floatBox));
        }
    }
}

void RenderBlockFlow::createFloatingObjects()
{
    m_floatingObjects = std::make_unique<FloatingObjects>(*this);
}

void RenderBlockFlow::removeFloatingObjects()
{
    if (!m_floatingObjects)
        return;

    markSiblingsWithFloatsForLayout();

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
    
    // Our location is irrelevant if we're unsplittable or no pagination is in effect. Just lay out the float.
    bool isChildRenderBlock = floatBox.isRenderBlock();
    if (isChildRenderBlock && !floatBox.needsLayout() && view().frameView().layoutContext().layoutState()->pageLogicalHeightChanged())
        floatBox.setChildNeedsLayout(MarkOnlyThis);

    bool needsBlockDirectionLocationSetBeforeLayout = isChildRenderBlock && view().frameView().layoutContext().layoutState()->needsBlockDirectionLocationSetBeforeLayout();
    if (!needsBlockDirectionLocationSetBeforeLayout || isWritingModeRoot()) {
        // We are unsplittable if we're a block flow root.
        floatBox.layoutIfNeeded();
        floatingObject->setShouldPaint(!floatBox.hasSelfPaintingLayer());
    }
    else {
        floatBox.updateLogicalWidth();
        floatBox.computeAndSetBlockDirectionMargins(*this);
    }

    setLogicalWidthForFloat(*floatingObject, logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));

    return m_floatingObjects->add(WTFMove(floatingObject));
}

void RenderBlockFlow::removeFloatingObject(RenderBox& floatBox)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(floatBox);
        if (it != floatingObjectSet.end()) {
            auto& floatingObject = *it->get();
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
                if (floatingObject.originatingLine()) {
                    floatingObject.originatingLine()->removeFloat(floatBox);
                    if (!selfNeedsLayout()) {
                        ASSERT(&floatingObject.originatingLine()->renderer() == this);
                        floatingObject.originatingLine()->markDirty();
                    }
#if !ASSERT_DISABLED
                    floatingObject.clearOriginatingLine();
#endif
                }
                markLinesDirtyInBlockRange(0, logicalBottom);
            }
            m_floatingObjects->remove(&floatingObject);
        }
    }
}

void RenderBlockFlow::removeFloatingObjectsBelow(FloatingObject* lastFloat, int logicalOffset)
{
    if (!containsFloats())
        return;
    
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObject* curr = floatingObjectSet.last().get();
    while (curr != lastFloat && (!curr->isPlaced() || logicalTopForFloat(*curr) >= logicalOffset)) {
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

void RenderBlockFlow::computeLogicalLocationForFloat(FloatingObject& floatingObject, LayoutUnit& logicalTopOffset)
{
    auto& childBox = floatingObject.renderer();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
    LayoutUnit logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.

    LayoutUnit floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    bool insideFragmentedFlow = enclosingFragmentedFlow();
    bool isInitialLetter = childBox.style().styleType() == PseudoId::FirstLetter && childBox.style().initialLetterDrop() > 0;
    
    if (isInitialLetter) {
        int letterClearance = lowestInitialLetterLogicalBottom() - logicalTopOffset;
        if (letterClearance > 0) {
            logicalTopOffset += letterClearance;
            setLogicalHeight(logicalHeight() + letterClearance);
        }
    }
    
    if (childBox.style().floating() == Float::Left) {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        while (logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight) - floatLogicalLeft < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
            if (insideFragmentedFlow) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        floatLogicalLeft = std::max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
    } else {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
        while (floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft) < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
            if (insideFragmentedFlow) {
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
    
    LayoutUnit childLogicalLeftMargin = style().isLeftToRightDirection() ? marginStartForChild(childBox) : marginEndForChild(childBox);
    LayoutUnit childBeforeMargin = marginBeforeForChild(childBox);
    
    if (isInitialLetter)
        adjustInitialLetterPosition(childBox, logicalTopOffset, childBeforeMargin);
    
    setLogicalLeftForFloat(floatingObject, floatLogicalLeft);
    setLogicalLeftForChild(childBox, floatLogicalLeft + childLogicalLeftMargin);
    
    setLogicalTopForFloat(floatingObject, logicalTopOffset);
    setLogicalTopForChild(childBox, logicalTopOffset + childBeforeMargin);
    
    setLogicalMarginsForFloat(floatingObject, childLogicalLeftMargin, childBeforeMargin);
}

void RenderBlockFlow::adjustInitialLetterPosition(RenderBox& childBox, LayoutUnit& logicalTopOffset, LayoutUnit& marginBeforeOffset)
{
    const RenderStyle& style = firstLineStyle();
    const FontMetrics& fontMetrics = style.fontMetrics();
    if (!fontMetrics.hasCapHeight())
        return;

    LayoutUnit heightOfLine = lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit beforeMarginBorderPadding = childBox.borderAndPaddingBefore() + childBox.marginBefore();
    
    // Make an adjustment to align with the cap height of a theoretical block line.
    LayoutUnit adjustment = fontMetrics.ascent() + (heightOfLine - fontMetrics.height()) / 2 - fontMetrics.capHeight() - beforeMarginBorderPadding;
    logicalTopOffset += adjustment;

    // For sunken and raised caps, we have to make some adjustments. Test if we're sunken or raised (dropHeightDelta will be
    // positive for raised and negative for sunken).
    int dropHeightDelta = childBox.style().initialLetterHeight() - childBox.style().initialLetterDrop();
    
    // If we're sunken, the float needs to shift down but lines still need to avoid it. In order to do that we increase the float's margin.
    if (dropHeightDelta < 0)
        marginBeforeOffset += -dropHeightDelta * heightOfLine;
    
    // If we're raised, then we actually have to grow the height of the block, since the lines have to be pushed down as though we're placing
    // empty lines beside the first letter.
    if (dropHeightDelta > 0)
        setLogicalHeight(logicalHeight() + dropHeightDelta * heightOfLine);
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
        logicalTop = std::max(logicalTopForFloat(*lastPlacedFloatingObject), logicalTop);

    auto end = floatingObjectSet.end();
    // Now walk through the set of unpositioned floats and place them.
    for (; it != end; ++it) {
        auto& floatingObject = *it->get();
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        auto& childBox = floatingObject.renderer();
        if (childBox.containingBlock() != this)
            continue;

        LayoutRect oldRect = childBox.frameRect();

        if (childBox.style().clear() == Clear::Left || childBox.style().clear() == Clear::Both)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatLeft), logicalTop);
        if (childBox.style().clear() == Clear::Right || childBox.style().clear() == Clear::Both)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatRight), logicalTop);

        computeLogicalLocationForFloat(floatingObject, logicalTop);
        LayoutUnit childLogicalTop = logicalTopForChild(childBox);

        estimateFragmentRangeForBoxChild(childBox);

        childBox.markForPaginationRelayoutIfNeeded();
        childBox.layoutIfNeeded();

        auto* layoutState = view().frameView().layoutContext().layoutState();
        bool isPaginated = layoutState->isPaginated();
        if (isPaginated) {
            // If we are unsplittable and don't fit, then we need to move down.
            // We include our margins as part of the unsplittable area.
            LayoutUnit newLogicalTop = adjustForUnsplittableChild(childBox, logicalTop, childLogicalTop - logicalTop, marginAfterForChild(childBox));
            
            // See if we have a pagination strut that is making us move down further.
            // Note that an unsplittable child can't also have a pagination strut, so this
            // is exclusive with the case above.
            RenderBlock* childBlock = is<RenderBlock>(childBox) ? &downcast<RenderBlock>(childBox) : nullptr;
            if (childBlock && childBlock->paginationStrut()) {
                newLogicalTop += childBlock->paginationStrut();
                childBlock->setPaginationStrut(0);
            }
            
            if (newLogicalTop != logicalTop) {
                floatingObject.setPaginationStrut(newLogicalTop - logicalTop);
                computeLogicalLocationForFloat(floatingObject, newLogicalTop);
                if (childBlock)
                    childBlock->setChildNeedsLayout(MarkOnlyThis);
                childBox.layoutIfNeeded();
                logicalTop = newLogicalTop;
            }

            if (updateFragmentRangeForBoxChild(childBox)) {
                childBox.setNeedsLayout(MarkOnlyThis);
                childBox.layoutIfNeeded();
            }
        }

        setLogicalHeightForFloat(floatingObject, logicalHeightForChildForFragmentation(childBox) + (logicalTopForChild(childBox) - logicalTop) + marginAfterForChild(childBox));

        m_floatingObjects->addPlacedObject(&floatingObject);

        if (ShapeOutsideInfo* shapeOutside = childBox.shapeOutsideInfo())
            shapeOutside->setReferenceBoxLogicalSize(logicalSizeForChild(childBox));
        // If the child moved, we have to repaint it.
        if (childBox.checkForRepaintDuringLayout())
            childBox.repaintDuringLayoutIfMoved(oldRect);
    }
    return true;
}

void RenderBlockFlow::clearFloats(Clear clear)
{
    positionNewFloats();
    // set y position
    LayoutUnit newY;
    switch (clear) {
    case Clear::Left:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case Clear::Right:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case Clear::Both:
        newY = lowestFloatLogicalBottom();
        break;
    case Clear::None:
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
    LayoutUnit lowestFloatBottom;
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        if (floatingObject.isPlaced() && floatingObject.type() & floatType)
            lowestFloatBottom = std::max(lowestFloatBottom, logicalBottomForFloat(floatingObject));
    }
    return lowestFloatBottom;
}

LayoutUnit RenderBlockFlow::lowestInitialLetterLogicalBottom() const
{
    if (!m_floatingObjects)
        return 0;
    LayoutUnit lowestFloatBottom;
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        if (floatingObject.isPlaced() && floatingObject.renderer().style().styleType() == PseudoId::FirstLetter && floatingObject.renderer().style().initialLetterDrop() > 0)
            lowestFloatBottom = std::max(lowestFloatBottom, logicalBottomForFloat(floatingObject));
    }
    return lowestFloatBottom;
}

LayoutUnit RenderBlockFlow::addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (!child.containsFloats() || child.createsNewFormattingContext())
        return 0;

    LayoutUnit childLogicalTop = child.logicalTop();
    LayoutUnit childLogicalLeft = child.logicalLeft();
    LayoutUnit lowestFloatLogicalBottom;

    // Floats that will remain the child's responsibility to paint should factor into its
    // overflow.
    auto childEnd = child.m_floatingObjects->set().end();
    for (auto childIt = child.m_floatingObjects->set().begin(); childIt != childEnd; ++childIt) {
        auto& floatingObject = *childIt->get();
        LayoutUnit floatLogicalBottom = std::min(logicalBottomForFloat(floatingObject), LayoutUnit::max() - childLogicalTop);
        LayoutUnit logicalBottom = childLogicalTop + floatLogicalBottom;
        lowestFloatLogicalBottom = std::max(lowestFloatLogicalBottom, logicalBottom);

        if (logicalBottom > logicalHeight()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(floatingObject.renderer())) {
                LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(-childLogicalLeft, -childLogicalTop) : LayoutSize(-childLogicalTop, -childLogicalLeft);
                bool shouldPaint = false;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly). We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a self-painting layer boundary.
                if (floatingObject.renderer().enclosingFloatPaintingLayer() == enclosingFloatPaintingLayer()) {
                    floatingObject.setShouldPaint(false);
                    shouldPaint = true;
                }
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                m_floatingObjects->add(floatingObject.copyToNewContainer(offset, shouldPaint, true));
            }
        } else {
            const auto& renderer = floatingObject.renderer();
            if (makeChildPaintOtherFloats && !floatingObject.shouldPaint() && !renderer.hasSelfPaintingLayer()
                && renderer.isDescendantOf(&child) && renderer.enclosingFloatPaintingLayer() == child.enclosingFloatPaintingLayer()) {
                // The float is not overhanging from this block, so if it is a descendant of the child, the child should
                // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
                // layer.
                // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
                // it should paint.
                floatingObject.setShouldPaint(true);
            }
            
            // Since the float doesn't overhang, it didn't get put into our list. We need to add its overflow in to the child now.
            if (floatingObject.isDescendant())
                child.addOverflowFromChild(&renderer, floatingObject.locationOffsetOfBorderBox());
        }
    }
    return lowestFloatLogicalBottom;
}

bool RenderBlockFlow::hasOverhangingFloat(RenderBox& renderer)
{
    if (!m_floatingObjects || !parent())
        return false;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    const auto it = floatingObjectSet.find<RenderBox&, FloatingObjectHashTranslator>(renderer);
    if (it == floatingObjectSet.end())
        return false;

    return logicalBottomForFloat(*it->get()) > logicalHeight();
}

void RenderBlockFlow::addIntrudingFloats(RenderBlockFlow* prev, RenderBlockFlow* container, LayoutUnit logicalLeftOffset, LayoutUnit logicalTopOffset)
{
    ASSERT(!avoidsFloats());

    // If we create our own block formatting context then our contents don't interact with floats outside it, even those from our parent.
    if (createsNewFormattingContext())
        return;

    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!prev->m_floatingObjects)
        return;

    logicalLeftOffset += marginLogicalLeft();

    const FloatingObjectSet& prevSet = prev->m_floatingObjects->set();
    auto prevEnd = prevSet.end();
    for (auto prevIt = prevSet.begin(); prevIt != prevEnd; ++prevIt) {
        auto& floatingObject = *prevIt->get();
        if (logicalBottomForFloat(floatingObject) > logicalTopOffset) {
            if (!m_floatingObjects || !m_floatingObjects->set().contains<FloatingObject&, FloatingObjectHashTranslator>(floatingObject)) {
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since this margin was added already through the modification of the |logicalLeftOffset| variable
                // above. |logicalLeftOffset| will equal the margin in this case, so it's already been taken
                // into account. Only apply this code if prev is the parent, since otherwise the left margin
                // will get applied twice.
                LayoutSize offset = isHorizontalWritingMode()
                    ? LayoutSize(logicalLeftOffset - (prev != container ? prev->marginLeft() : 0_lu), logicalTopOffset)
                    : LayoutSize(logicalTopOffset, logicalLeftOffset - (prev != container ? prev->marginTop() : 0_lu));

                m_floatingObjects->add(floatingObject.copyToNewContainer(offset));
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
    else if (childrenInline())
        return;

    // Iterate over our block children and mark them as needed.
    for (auto& block : childrenOfType<RenderBlock>(*this)) {
        if (!floatToRemove && block.isFloatingOrOutOfFlowPositioned())
            continue;
        if (!is<RenderBlockFlow>(block)) {
            if (block.shrinkToAvoidFloats() && block.everHadLayout())
                block.setChildNeedsLayout(markParents);
            continue;
        }
        auto& blockFlow = downcast<RenderBlockFlow>(block);
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
        if (!is<RenderBlockFlow>(*next) || next->isFloatingOrOutOfFlowPositioned())
            continue;

        RenderBlockFlow& nextBlock = downcast<RenderBlockFlow>(*next);
        for (auto it = floatingObjectSet.begin(); it != end; ++it) {
            RenderBox& floatingBox = (*it)->renderer();
            if (floatToRemove && &floatingBox != floatToRemove)
                continue;
            if (nextBlock.containsFloat(floatingBox))
                nextBlock.markAllDescendantsWithFloatsForLayout(&floatingBox);
        }
    }
}

LayoutPoint RenderBlockFlow::flipFloatForWritingModeForChild(const FloatingObject& child, const LayoutPoint& point) const
{
    if (!style().isFlippedBlocksWritingMode())
        return point;
    
    // This is similar to RenderBox::flipForWritingModeForChild. We have to subtract out our left/top offsets twice, since
    // it's going to get added back in. We hide this complication here so that the calling code looks normal for the unflipped
    // case.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child.renderer().height() - 2 * child.locationOffsetOfBorderBox().height());
    return LayoutPoint(point.x() + width() - child.renderer().width() - 2 * child.locationOffsetOfBorderBox().width(), point.y());
}

LayoutUnit RenderBlockFlow::getClearDelta(RenderBox& child, LayoutUnit logicalTop)
{
    // There is no need to compute clearance if we have no floats.
    if (!containsFloats())
        return 0;
    
    // At least one float is present. We need to perform the clearance computation.
    bool clearSet = child.style().clear() != Clear::None;
    LayoutUnit logicalBottom;
    switch (child.style().clear()) {
    case Clear::None:
        break;
    case Clear::Left:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case Clear::Right:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case Clear::Both:
        logicalBottom = lowestFloatLogicalBottom();
        break;
    }

    // We also clear floats if we are too big to sit on the same line as a float (and wish to avoid floats by default).
    LayoutUnit result = clearSet ? std::max<LayoutUnit>(0, logicalBottom - logicalTop) : 0_lu;
    if (!result && child.avoidsFloats()) {
        LayoutUnit newLogicalTop = logicalTop;
        while (true) {
            LayoutUnit availableLogicalWidthAtNewLogicalTopOffset = availableLogicalWidthForLine(newLogicalTop, DoNotIndentText, logicalHeightForChild(child));
            if (availableLogicalWidthAtNewLogicalTopOffset == availableLogicalWidthForContent(newLogicalTop))
                return newLogicalTop - logicalTop;

            RenderFragmentContainer* fragment = fragmentAtBlockOffset(logicalTopForChild(child));
            LayoutRect borderBox = child.borderBoxRectInFragment(fragment, DoNotCacheRenderBoxFragmentInfo);
            LayoutUnit childLogicalWidthAtOldLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            // FIXME: None of this is right for perpendicular writing-mode children.
            LayoutUnit childOldLogicalWidth = child.logicalWidth();
            LayoutUnit childOldMarginLeft = child.marginLeft();
            LayoutUnit childOldMarginRight = child.marginRight();
            LayoutUnit childOldLogicalTop = child.logicalTop();

            child.setLogicalTop(newLogicalTop);
            child.updateLogicalWidth();
            fragment = fragmentAtBlockOffset(logicalTopForChild(child));
            borderBox = child.borderBoxRectInFragment(fragment, DoNotCacheRenderBoxFragmentInfo);
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
    if (is<RenderView>(*this))
        adjustedLocation += toLayoutSize(downcast<RenderView>(*this).frameView().scrollPosition());

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto begin = floatingObjectSet.begin();
    for (auto it = floatingObjectSet.end(); it != begin;) {
        --it;
        const auto& floatingObject = *it->get();
        auto& renderer = floatingObject.renderer();
        if (floatingObject.shouldPaint() && !renderer.hasSelfPaintingLayer()) {
            LayoutPoint childPoint = flipFloatForWritingModeForChild(floatingObject, adjustedLocation + floatingObject.translationOffsetToAncestor());
            if (renderer.hitTest(request, result, locationInContainer, childPoint)) {
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

    if (auto simpleLineLayout = this->simpleLineLayout())
        return SimpleLineLayout::hitTestFlow(*this, *simpleLineLayout, request, result, locationInContainer, accumulatedOffset, hitTestAction);

    return m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

void RenderBlockFlow::adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const
{
    if (style().visibility() != Visibility::Visible)
        return;

    // We don't deal with relative positioning.  Our assumption is that you shrink to fit the lines without accounting
    // for either overflow or translations via relative positioning.
    if (childrenInline()) {
        const_cast<RenderBlockFlow&>(*this).ensureLineBoxes();

        for (auto* box = firstRootBox(); box; box = box->nextRootBox()) {
            if (box->firstChild())
                left = std::min(left, x + LayoutUnit(box->firstChild()->x()));
            if (box->lastChild())
                right = std::max(right, x + LayoutUnit(ceilf(box->lastChild()->logicalRight())));
        }
    } else {
        for (RenderBox* obj = firstChildBox(); obj; obj = obj->nextSiblingBox()) {
            if (!obj->isFloatingOrOutOfFlowPositioned()) {
                if (is<RenderBlockFlow>(*obj) && !obj->hasOverflowClip())
                    downcast<RenderBlockFlow>(*obj).adjustForBorderFit(x + obj->x(), left, right);
                else if (obj->style().visibility() == Visibility::Visible) {
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
            const auto& floatingObject = *it->get();
            // Only examine the object if our m_shouldPaint flag is set.
            if (floatingObject.shouldPaint()) {
                LayoutUnit floatLeft = floatingObject.translationOffsetToAncestor().width();
                LayoutUnit floatRight = floatLeft + floatingObject.renderer().width();
                left = std::min(left, floatLeft);
                right = std::max(right, floatRight);
            }
        }
    }
}

void RenderBlockFlow::fitBorderToLinesIfNeeded()
{
    if (style().borderFit() == BorderFit::Border || hasOverrideContentLogicalWidth())
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
    
    setOverrideContentLogicalWidth(newContentWidth);
    layoutBlock(false);
    clearOverrideContentLogicalWidth();
}

void RenderBlockFlow::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    // Floats currently affect the choice whether to use simple line layout path.
    if (m_simpleLineLayout) {
        invalidateLineLayoutPath();
        return;
    }

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

std::optional<int> RenderBlockFlow::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun() && !isGridItem())
        return std::nullopt;

    if (!childrenInline())
        return RenderBlock::firstLineBaseline();

    if (!hasLines())
        return std::nullopt;

    if (auto simpleLineLayout = this->simpleLineLayout())
        return std::optional<int>(SimpleLineLayout::computeFlowFirstLineBaseline(*this, *simpleLineLayout));

    ASSERT(firstRootBox());
    if (style().isFlippedLinesWritingMode())
        return firstRootBox()->logicalTop() + firstLineStyle().fontMetrics().descent(firstRootBox()->baselineType());
    return firstRootBox()->logicalTop() + firstLineStyle().fontMetrics().ascent(firstRootBox()->baselineType());
}

std::optional<int> RenderBlockFlow::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return std::nullopt;

    // Note that here we only take the left and bottom into consideration. Our caller takes the right and top into consideration.
    float boxHeight = lineDirection == HorizontalLine ? height() + m_marginBox.bottom() : width() + m_marginBox.left();
    float lastBaseline;
    if (!childrenInline()) {
        std::optional<int> inlineBlockBaseline = RenderBlock::inlineBlockBaseline(lineDirection);
        if (!inlineBlockBaseline)
            return inlineBlockBaseline;
        lastBaseline = inlineBlockBaseline.value();
    } else {
        if (!hasLines()) {
            if (!hasLineIfEmpty())
                return std::nullopt;
            const auto& fontMetrics = firstLineStyle().fontMetrics();
            return std::optional<int>(fontMetrics.ascent()
                + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight()));
        }

        if (auto simpleLineLayout = this->simpleLineLayout())
            lastBaseline = SimpleLineLayout::computeFlowLastLineBaseline(*this, *simpleLineLayout);
        else {
            bool isFirstLine = lastRootBox() == firstRootBox();
            const auto& style = isFirstLine ? firstLineStyle() : this->style();
            // InlineFlowBox::placeBoxesInBlockDirection will flip lines in case of verticalLR mode, so we can assume verticalRL for now.
            lastBaseline = style.fontMetrics().ascent(lastRootBox()->baselineType())
                + (style.isFlippedLinesWritingMode() ? logicalHeight() - lastRootBox()->logicalBottom() : lastRootBox()->logicalTop());
        }
    }
    // According to the CSS spec http://www.w3.org/TR/CSS21/visudet.html, we shouldn't be performing this min, but should
    // instead be returning boxHeight directly. However, we feel that a min here is better behavior (and is consistent
    // enough with the spec to not cause tons of breakages).
    return style().overflowY() == Overflow::Visible ? lastBaseline : std::min(boxHeight, lastBaseline);
}

void RenderBlockFlow::setSelectionState(SelectionState state)
{
    if (state != SelectionNone)
        ensureLineBoxes();
    RenderBoxModelObject::setSelectionState(state);
}

GapRects RenderBlockFlow::inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    ASSERT(!m_simpleLineLayout);

    GapRects result;

    bool containsStart = selectionState() == SelectionStart || selectionState() == SelectionBoth;

    if (!hasLines()) {
        if (containsStart) {
            // Update our lastLogicalTop to be the bottom of the block. <hr>s or empty blocks with height can trip this case.
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
            selectionState() != SelectionStart && selectionState() != SelectionBoth && !isRubyBase())
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
        // Update our lastY to be the bottom of the last selected line.
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + lastSelectedLine->selectionBottom();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, lastSelectedLine->selectionBottom(), cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, lastSelectedLine->selectionBottom(), cache);
    }
    return result;
}

bool RenderBlockFlow::needsLayoutAfterFragmentRangeChange() const
{
    // A block without floats or that expands to enclose them won't need a relayout
    // after a fragment range change. There is no overflow content needing relayout
    // in the fragment chain because the fragment range can only shrink after the estimation.
    if (!containsFloats() || createsNewFormattingContext())
        return false;

    return true;
}

void RenderBlockFlow::setMultiColumnFlow(RenderMultiColumnFlow& fragmentedFlow)
{
    ASSERT(!hasRareBlockFlowData() || !rareBlockFlowData()->m_multiColumnFlow);
    ensureRareBlockFlowData().m_multiColumnFlow = makeWeakPtr(fragmentedFlow);
}

void RenderBlockFlow::clearMultiColumnFlow()
{
    ASSERT(hasRareBlockFlowData());
    ASSERT(rareBlockFlowData()->m_multiColumnFlow);
    rareBlockFlowData()->m_multiColumnFlow.clear();
}

static bool shouldCheckLines(const RenderBlockFlow& blockFlow)
{
    return !blockFlow.isFloatingOrOutOfFlowPositioned() && blockFlow.style().height().isAuto();
}

RootInlineBox* RenderBlockFlow::lineAtIndex(int i) const
{
    ASSERT(i >= 0);

    if (style().visibility() != Visibility::Visible)
        return nullptr;

    if (childrenInline()) {
        for (auto* box = firstRootBox(); box; box = box->nextRootBox()) {
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
    if (style().visibility() != Visibility::Visible)
        return 0;

    int count = 0;

    if (childrenInline()) {
        if (auto simpleLineLayout = this->simpleLineLayout()) {
            ASSERT(!stopRootInlineBox);
            return simpleLineLayout->lineCount();
        }
        for (auto* box = firstRootBox(); box; box = box->nextRootBox()) {
            ++count;
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
    if (block.style().visibility() != Visibility::Visible)
        return -1;

    if (block.childrenInline()) {
        for (auto* box = block.firstRootBox(); box; box = box->nextRootBox()) {
            if (++count == lineCount)
                return box->lineBottom() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : 0_lu);
        }
    } else {
        RenderBox* normalFlowChildWithoutLines = nullptr;
        for (auto* obj = block.firstChildBox(); obj; obj = obj->nextSiblingBox()) {
            if (is<RenderBlockFlow>(*obj) && shouldCheckLines(downcast<RenderBlockFlow>(*obj))) {
                int result = getHeightForLineCount(downcast<RenderBlockFlow>(*obj), lineCount, false, count);
                if (result != -1)
                    return result + obj->y() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : 0_lu);
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
    if (style().visibility() != Visibility::Visible)
        return;

    if (childrenInline() && hasMarkupTruncation()) {
        ensureLineBoxes();

        setHasMarkupTruncation(false);
        for (auto* box = firstRootBox(); box; box = box->nextRootBox())
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
    for (auto* root = firstRootBox(); root; root = root->nextRootBox()) {
        for (auto* box = root->firstLeafChild(); box; box = box->nextLeafChild()) {
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

    if (!is<InlineTextBox>(*box))
        return createLegacyEditingPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    auto& textBox = downcast<InlineTextBox>(*box);
    return createLegacyEditingPosition(textBox.renderer().nonPseudoNode(), start ? textBox.start() : textBox.start() + textBox.len());
}

RenderText* RenderBlockFlow::findClosestTextAtAbsolutePoint(const FloatPoint& point)
{
    // A light, non-recursive version of RenderBlock::positionForCoordinates that looks at
    // whether a point lies within the gaps between its root line boxes, to be called against
    // a node returned from elementAtPoint. We make the assumption that either the node or one
    // of its immediate children contains the root line boxes in question.
    // See <rdar://problem/6824650> for context.
    
    RenderBlock* block = this;
    
    FloatPoint localPoint = block->absoluteToLocal(point);
    
    if (!block->childrenInline()) {
        // Look among our immediate children for an alternate box that contains the point.
        for (RenderBox* child = block->firstChildBox(); child; child = child->nextSiblingBox()) {
            if (!child->height() || child->style().visibility() != WebCore::Visibility::Visible || child->isFloatingOrOutOfFlowPositioned())
                continue;
            float top = child->y();
            
            RenderBox* nextChild = child->nextSiblingBox();
            while (nextChild && nextChild->isFloatingOrOutOfFlowPositioned())
                nextChild = nextChild->nextSiblingBox();
            if (!nextChild) {
                if (localPoint.y() >= top) {
                    block = downcast<RenderBlock>(child);
                    break;
                }
                continue;
            }
            
            float bottom = nextChild->y();
            
            if (localPoint.y() >= top && localPoint.y() < bottom && is<RenderBlock>(*child)) {
                block = downcast<RenderBlock>(child);
                break;
            }
        }
        
        if (!block->childrenInline())
            return nullptr;
        
        localPoint = block->absoluteToLocal(point);
    }
    
    RenderBlockFlow& blockFlow = downcast<RenderBlockFlow>(*block);
    
    // Only check the gaps between the root line boxes. We deliberately ignore overflow because
    // experience has shown that hit tests on an exploded text node can fail when within the
    // overflow fragment.
    for (RootInlineBox* current = blockFlow.firstRootBox(); current && current != blockFlow.lastRootBox(); current = current->nextRootBox()) {
        float currentBottom = current->y() + current->logicalHeight();
        if (localPoint.y() < currentBottom)
            return nullptr;
        
        RootInlineBox* next = current->nextRootBox();
        float nextTop = next->y();
        if (localPoint.y() < nextTop) {
            InlineBox* inlineBox = current->closestLeafChildForLogicalLeftPosition(localPoint.x());
            if (inlineBox && inlineBox->behavesLikeText() && is<RenderText>(inlineBox->renderer()))
                return &downcast<RenderText>(inlineBox->renderer());
        }
    }
    return nullptr;
}

VisiblePosition RenderBlockFlow::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents, const RenderFragmentContainer* fragment)
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
        if (fragment && root->containingFragment() != fragment)
            continue;

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
            return positionForPointRespectingEditingBoundaries(*this, downcast<RenderBox>(closestBox->renderer()), point);
        return closestBox->renderer().positionForPoint(point, nullptr);
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

Position RenderBlockFlow::positionForPoint(const LayoutPoint& point)
{
    // FIXME: It supports single text child only (which is the majority of simple line layout supported content at this point).
    if (!simpleLineLayout() || firstChild() != lastChild() || !is<RenderText>(firstChild()))
        return positionForPoint(point, nullptr).deepEquivalent();
    return downcast<RenderText>(*firstChild()).positionForPoint(point);
}

VisiblePosition RenderBlockFlow::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer*)
{
    return RenderBlock::positionForPoint(point, nullptr);
}

void RenderBlockFlow::addFocusRingRectsForInlineChildren(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    ASSERT(childrenInline());
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        LayoutUnit top = std::max<LayoutUnit>(curr->lineTop(), curr->top());
        LayoutUnit bottom = std::min<LayoutUnit>(curr->lineBottom(), curr->top() + curr->height());
        LayoutRect rect(additionalOffset.x() + curr->x(), additionalOffset.y() + top, curr->width(), bottom - top);
        if (!rect.isEmpty())
            rects.append(rect);
    }
}

void RenderBlockFlow::paintInlineChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(childrenInline());

    if (auto simpleLineLayout = this->simpleLineLayout()) {
        SimpleLineLayout::paintFlow(*this, *simpleLineLayout, paintInfo, paintOffset);
        return;
    }
    m_lineBoxes.paint(this, paintInfo, paintOffset);
}

bool RenderBlockFlow::relayoutForPagination()
{
    if (!multiColumnFlow() || !multiColumnFlow()->shouldRelayoutForPagination())
        return false;
    
    multiColumnFlow()->setNeedsHeightsRecalculation(false);
    multiColumnFlow()->setInBalancingPass(true); // Prevent re-entering this method (and recursion into layout).

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
        for (RenderMultiColumnSet* multicolSet = multiColumnFlow()->firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
            if (multicolSet->recalculateColumnHeight(firstPass))
                needsRelayout = true;
            if (needsRelayout) {
                // Once a column set gets a new column height, that column set and all successive column
                // sets need to be laid out over again, since their logical top will be affected by
                // this, and therefore their column heights may change as well, at least if the multicol
                // height is constrained.
                multicolSet->setChildNeedsLayout(MarkOnlyThis);
            }
        }
        if (needsRelayout) {
            // Layout again. Column balancing resulted in a new height.
            neededRelayout = true;
            multiColumnFlow()->setChildNeedsLayout(MarkOnlyThis);
            setChildNeedsLayout(MarkOnlyThis);
            layoutBlock(false);
        }
        firstPass = false;
    } while (needsRelayout);
    
    multiColumnFlow()->setInBalancingPass(false);
    
    return neededRelayout;
}

bool RenderBlockFlow::hasLines() const
{
    if (!childrenInline())
        return false;

    if (auto simpleLineLayout = this->simpleLineLayout())
        return simpleLineLayout->lineCount();

    return lineBoxes().firstLineBox();
}

void RenderBlockFlow::invalidateLineLayoutPath()
{
    switch (lineLayoutPath()) {
    case UndeterminedPath:
    case ForceLineBoxesPath:
        ASSERT(!m_simpleLineLayout);
        return;
    case LineBoxesPath:
        ASSERT(!m_simpleLineLayout);
        setLineLayoutPath(UndeterminedPath);
        return;
    case SimpleLinesPath:
        // The simple line layout may have become invalid.
        m_simpleLineLayout = nullptr;
        setLineLayoutPath(UndeterminedPath);
        if (needsLayout())
            return;
        // FIXME: We should just kick off a subtree layout here (if needed at all) see webkit.org/b/172947.
        setNeedsLayout();
        return;
    }
    ASSERT_NOT_REACHED();
}

void RenderBlockFlow::layoutSimpleLines(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    bool needsLayout = selfNeedsLayout() || relayoutChildren || !m_simpleLineLayout;
    if (needsLayout) {
        deleteLineBoxesBeforeSimpleLineLayout();
        m_simpleLineLayout = SimpleLineLayout::create(*this);
    }
    if (view().frameView().layoutContext().layoutState() && view().frameView().layoutContext().layoutState()->isPaginated()) {
        m_simpleLineLayout->setIsPaginated();
        SimpleLineLayout::adjustLinePositionsForPagination(*m_simpleLineLayout, *this);
    }
    for (auto& renderer : childrenOfType<RenderObject>(*this))
        renderer.clearNeedsLayout();
    ASSERT(!m_lineBoxes.firstLineBox());
    LayoutUnit lineLayoutHeight = SimpleLineLayout::computeFlowHeight(*this, *m_simpleLineLayout);
    LayoutUnit lineLayoutTop = borderAndPaddingBefore();
    repaintLogicalTop = lineLayoutTop;
    repaintLogicalBottom = needsLayout ? repaintLogicalTop + lineLayoutHeight + borderAndPaddingAfter() : repaintLogicalTop;
    setLogicalHeight(lineLayoutTop + lineLayoutHeight + borderAndPaddingAfter());
}

void RenderBlockFlow::deleteLineBoxesBeforeSimpleLineLayout()
{
    ASSERT(lineLayoutPath() == SimpleLinesPath);
    lineBoxes().deleteLineBoxes();
    for (auto& renderer : childrenOfType<RenderObject>(*this)) {
        if (is<RenderText>(renderer))
            downcast<RenderText>(renderer).deleteLineBoxesBeforeSimpleLineLayout();
        else if (is<RenderLineBreak>(renderer))
            downcast<RenderLineBreak>(renderer).deleteLineBoxesBeforeSimpleLineLayout();
        else
            ASSERT_NOT_REACHED();
    }
}

void RenderBlockFlow::ensureLineBoxes()
{
    setLineLayoutPath(ForceLineBoxesPath);
    if (!m_simpleLineLayout)
        return;

    if (SimpleLineLayout::canUseForLineBoxTree(*this, *m_simpleLineLayout)) {
        SimpleLineLayout::generateLineBoxTree(*this, *m_simpleLineLayout);
        m_simpleLineLayout = nullptr;
        return;
    }
    bool isPaginated = m_simpleLineLayout->isPaginated();
    m_simpleLineLayout = nullptr;

#if !ASSERT_DISABLED
    LayoutUnit oldHeight = logicalHeight();
#endif
    bool didNeedLayout = needsLayout();

    bool relayoutChildren = false;
    LayoutUnit repaintLogicalTop;
    LayoutUnit repaintLogicalBottom;
    if (isPaginated) {
        PaginatedLayoutStateMaintainer state(*this);
        layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
        // This matches relayoutToAvoidWidows.
        if (shouldBreakAtLineToAvoidWidow())
            layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
        // FIXME: This is needed as long as simple and normal line layout produce different line breakings.
        repaint();
    } else
        layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);

    updateLogicalHeight();
    ASSERT(didNeedLayout || logicalHeight() == oldHeight);

    if (!didNeedLayout)
        clearNeedsLayout();
}

#if ENABLE(TREE_DEBUGGING)
void RenderBlockFlow::outputLineTreeAndMark(WTF::TextStream& stream, const InlineBox* markedBox, int depth) const
{
    for (const RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->outputLineTreeAndMark(stream, markedBox, depth);

    if (auto simpleLineLayout = this->simpleLineLayout())
        SimpleLineLayout::outputLineLayoutForFlow(stream, *this, *simpleLineLayout, depth);
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

#if ENABLE(TEXT_AUTOSIZING)

static inline bool isVisibleRenderText(const RenderObject& renderer)
{
    if (!is<RenderText>(renderer))
        return false;

    auto& renderText = downcast<RenderText>(renderer);
    return !renderText.linesBoundingBox().isEmpty() && !renderText.text().isAllSpecialCharacters<isHTMLSpace>();
}

static inline bool resizeTextPermitted(const RenderObject& renderer)
{
    // We disallow resizing for text input fields and textarea to address <rdar://problem/5792987> and <rdar://problem/8021123>
    for (auto* ancestor = renderer.parent(); ancestor; ancestor = ancestor->parent()) {
        // Get the first non-shadow HTMLElement and see if it's an input.
        if (is<HTMLElement>(ancestor->element()) && !ancestor->element()->isInShadowTree()) {
            auto& element = downcast<HTMLElement>(*ancestor->element());
            return !is<HTMLInputElement>(element) && !is<HTMLTextAreaElement>(element);
        }
    }
    return true;
}

int RenderBlockFlow::lineCountForTextAutosizing()
{
    if (style().visibility() != Visibility::Visible)
        return 0;
    if (childrenInline())
        return lineCount();
    // Only descend into list items.
    int count = 0;
    for (auto& listItem : childrenOfType<RenderListItem>(*this))
        count += listItem.lineCount();
    return count;
}

static bool isNonBlocksOrNonFixedHeightListItems(const RenderObject& renderer)
{
    if (!renderer.isRenderBlock())
        return true;
    if (renderer.isListItem())
        return renderer.style().height().type() != Fixed;
    return false;
}

// For now, we auto size single lines of text the same as multiple lines.
// We've been experimenting with low values for single lines of text.
static inline float oneLineTextMultiplier(RenderObject& renderer, float specifiedSize)
{
    const float coefficient = renderer.settings().oneLineTextMultiplierCoefficient();
    return std::max((1.0f / log10f(specifiedSize) * coefficient), 1.0f);
}

static inline float textMultiplier(RenderObject& renderer, float specifiedSize)
{
    const float coefficient = renderer.settings().multiLineTextMultiplierCoefficient();
    return std::max((1.0f / log10f(specifiedSize) * coefficient), 1.0f);
}

void RenderBlockFlow::adjustComputedFontSizes(float size, float visibleWidth)
{
    LOG(TextAutosizing, "RenderBlockFlow %p adjustComputedFontSizes, size=%f visibleWidth=%f, width()=%f. Bailing: %d", this, size, visibleWidth, width().toFloat(), visibleWidth >= width());

    // Don't do any work if the block is smaller than the visible area.
    if (visibleWidth >= width())
        return;
    
    unsigned lineCount;
    if (m_lineCountForTextAutosizing == NOT_SET) {
        int count = lineCountForTextAutosizing();
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

    for (auto* descendant = RenderObjectTraversal::firstChild(*this); descendant; ) {
        if (!isNonBlocksOrNonFixedHeightListItems(*descendant)) {
            descendant = RenderObjectTraversal::nextSkippingChildren(*descendant, this);
            continue;
        }
        if (!isVisibleRenderText(*descendant) || !resizeTextPermitted(*descendant)) {
            descendant = RenderObjectTraversal::next(*descendant, this);
            continue;
        }

        auto& text = downcast<RenderText>(*descendant);
        auto& oldStyle = text.style();
        auto& fontDescription = oldStyle.fontDescription();
        float specifiedSize = fontDescription.specifiedSize();
        float scaledSize = roundf(specifiedSize * scale);
        if (scaledSize > 0 && scaledSize < minFontSize) {
            // Record the width of the block and the line count the first time we resize text and use it from then on for text resizing.
            // This makes text resizing consistent even if the block's width or line count changes (which can be caused by text resizing itself 5159915).
            if (m_lineCountForTextAutosizing == NOT_SET)
                m_lineCountForTextAutosizing = lineCount;
            if (m_widthForTextAutosizing == -1)
                m_widthForTextAutosizing = actualWidth;

            float lineTextMultiplier = lineCount == ONE_LINE ? oneLineTextMultiplier(text, specifiedSize) : textMultiplier(text, specifiedSize);
            float candidateNewSize = roundf(std::min(minFontSize, specifiedSize * lineTextMultiplier));
            if (candidateNewSize > specifiedSize && candidateNewSize != fontDescription.computedSize() && text.textNode() && oldStyle.textSizeAdjust().isAuto())
                document().textAutoSizing().addTextNode(*text.textNode(), candidateNewSize);
        }

        descendant = RenderObjectTraversal::nextSkippingChildren(text, this);
    }
}

#endif // ENABLE(TEXT_AUTOSIZING)

void RenderBlockFlow::layoutExcludedChildren(bool relayoutChildren)
{
    RenderBlock::layoutExcludedChildren(relayoutChildren);

    auto* fragmentedFlow = multiColumnFlow();
    if (!fragmentedFlow)
        return;

    fragmentedFlow->setIsExcludedFromNormalLayout(true);

    setLogicalTopForChild(*fragmentedFlow, borderAndPaddingBefore());

    if (relayoutChildren)
        fragmentedFlow->setChildNeedsLayout(MarkOnlyThis);

    if (fragmentedFlow->needsLayout()) {
        for (RenderMultiColumnSet* columnSet = fragmentedFlow->firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
            columnSet->prepareForLayout(!fragmentedFlow->inBalancingPass());

        fragmentedFlow->invalidateFragments(MarkOnlyThis);
        fragmentedFlow->setNeedsHeightsRecalculation(true);
        fragmentedFlow->layout();
    } else {
        // At the end of multicol layout, relayoutForPagination() is called unconditionally, but if
        // no children are to be laid out (e.g. fixed width with layout already being up-to-date),
        // we want to prevent it from doing any work, so that the column balancing machinery doesn't
        // kick in and trigger additional unnecessary layout passes. Actually, it's not just a good
        // idea in general to not waste time on balancing content that hasn't been re-laid out; we
        // are actually required to guarantee this. The calculation of implicit breaks needs to be
        // preceded by a proper layout pass, since it's layout that sets up content runs, and the
        // runs get deleted right after every pass.
        fragmentedFlow->setNeedsHeightsRecalculation(false);
    }
    determineLogicalLeftPositionForChild(*fragmentedFlow);
}

void RenderBlockFlow::checkForPaginationLogicalHeightChange(bool& relayoutChildren, LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged)
{
    // If we don't use columns or flow threads, then bail.
    if (!isRenderFragmentedFlow() && !multiColumnFlow())
        return;
    
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    if (RenderMultiColumnFlow* fragmentedFlow = multiColumnFlow()) {
        LayoutUnit newColumnHeight;
        if (hasDefiniteLogicalHeight() || view().frameView().pagination().mode != Pagination::Unpaginated) {
            auto computedValues = computeLogicalHeight(0_lu, logicalTop());
            newColumnHeight = std::max<LayoutUnit>(computedValues.m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight(), 0);
            if (fragmentedFlow->columnHeightAvailable() != newColumnHeight)
                relayoutChildren = true;
        }
        fragmentedFlow->setColumnHeightAvailable(newColumnHeight);
    } else if (is<RenderFragmentedFlow>(*this)) {
        RenderFragmentedFlow& fragmentedFlow = downcast<RenderFragmentedFlow>(*this);

        // FIXME: This is a hack to always make sure we have a page logical height, if said height
        // is known. The page logical height thing in LayoutState is meaningless for flow
        // thread-based pagination (page height isn't necessarily uniform throughout the flow
        // thread), but as long as it is used universally as a means to determine whether page
        // height is known or not, we need this. Page height is unknown when column balancing is
        // enabled and flow thread height is still unknown (i.e. during the first layout pass). When
        // it's unknown, we need to prevent the pagination code from assuming page breaks everywhere
        // and thereby eating every top margin. It should be trivial to clean up and get rid of this
        // hack once the old multicol implementation is gone (see also RenderView::pushLayoutStateForPagination).
        pageLogicalHeight = fragmentedFlow.isPageLogicalHeightKnown() ? 1_lu : 0_lu;

        pageLogicalHeightChanged = fragmentedFlow.pageLogicalSizeChanged();
    }
}

bool RenderBlockFlow::requiresColumns(int desiredColumnCount) const
{    
    return willCreateColumns(desiredColumnCount);
}

void RenderBlockFlow::setComputedColumnCountAndWidth(int count, LayoutUnit width)
{
    ASSERT(!!multiColumnFlow() == requiresColumns(count));
    if (!multiColumnFlow())
        return;
    multiColumnFlow()->setColumnCountAndWidth(count, width);
    multiColumnFlow()->setProgressionIsInline(style().hasInlineColumnAxis());
    multiColumnFlow()->setProgressionIsReversed(style().columnProgression() == ColumnProgression::Reverse);
}

void RenderBlockFlow::updateColumnProgressionFromStyle(RenderStyle& style)
{
    if (!multiColumnFlow())
        return;
    
    bool needsLayout = false;
    bool oldProgressionIsInline = multiColumnFlow()->progressionIsInline();
    bool newProgressionIsInline = style.hasInlineColumnAxis();
    if (oldProgressionIsInline != newProgressionIsInline) {
        multiColumnFlow()->setProgressionIsInline(newProgressionIsInline);
        needsLayout = true;
    }

    bool oldProgressionIsReversed = multiColumnFlow()->progressionIsReversed();
    bool newProgressionIsReversed = style.columnProgression() == ColumnProgression::Reverse;
    if (oldProgressionIsReversed != newProgressionIsReversed) {
        multiColumnFlow()->setProgressionIsReversed(newProgressionIsReversed);
        needsLayout = true;
    }

    if (needsLayout)
        setNeedsLayoutAndPrefWidthsRecalc();
}

LayoutUnit RenderBlockFlow::computedColumnWidth() const
{
    if (multiColumnFlow())
        return multiColumnFlow()->computedColumnWidth();
    return contentLogicalWidth();
}

unsigned RenderBlockFlow::computedColumnCount() const
{
    if (multiColumnFlow())
        return multiColumnFlow()->computedColumnCount();
    
    return 1;
}

bool RenderBlockFlow::isTopLayoutOverflowAllowed() const
{
    bool hasTopOverflow = RenderBlock::isTopLayoutOverflowAllowed();
    if (!multiColumnFlow() || style().columnProgression() == ColumnProgression::Normal)
        return hasTopOverflow;
    
    if (!(isHorizontalWritingMode() ^ !style().hasInlineColumnAxis()))
        hasTopOverflow = !hasTopOverflow;

    return hasTopOverflow;
}

bool RenderBlockFlow::isLeftLayoutOverflowAllowed() const
{
    bool hasLeftOverflow = RenderBlock::isLeftLayoutOverflowAllowed();
    if (!multiColumnFlow() || style().columnProgression() == ColumnProgression::Normal)
        return hasLeftOverflow;
    
    if (isHorizontalWritingMode() ^ !style().hasInlineColumnAxis())
        hasLeftOverflow = !hasLeftOverflow;

    return hasLeftOverflow;
}

struct InlineMinMaxIterator {
/* InlineMinMaxIterator is a class that will iterate over all render objects that contribute to
   inline min/max width calculations.  Note the following about the way it walks:
   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
   (2) We do not drill into the children of floats or replaced elements, since you can't break
       in the middle of such an element.
   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
       distinct borders/margin/padding that contribute to the min/max width.
*/
    const RenderBlockFlow& parent;
    RenderObject* current;
    bool endOfInline;
    bool initial;

    InlineMinMaxIterator(const RenderBlockFlow& p)
        : parent(p)
        , current(nullptr)
        , endOfInline(false)
        , initial(true)
        { }

    RenderObject* next();
};

RenderObject* InlineMinMaxIterator::next()
{
    RenderObject* result = nullptr;
    bool oldEndOfInline = endOfInline;
    endOfInline = false;
    do {
        if (!oldEndOfInline && (current && !current->isFloating() && !current->isReplaced() && !current->isOutOfFlowPositioned()))
            result = current->firstChildSlow();
        else if (initial) {
            result = parent.firstChild();
            initial = false;
        }

        if (!result) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && current && current->isRenderInline()) {
                result = current;
                endOfInline = true;
                break;
            }

            while (current && current != &parent) {
                result = current->nextSibling();
                if (result)
                    break;
                current = current->parent();
                if (current && current != &parent && current->isRenderInline()) {
                    result = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!result)
            break;

        if (!result->isOutOfFlowPositioned() && (result->isTextOrLineBreak() || result->isFloating() || result->isReplaced() || result->isRenderInline()))
            break;

        current = result;
        result = nullptr;
    } while (current || current == &parent);
    // Update our position.
    current = result;
    return result;
}

static LayoutUnit getBPMWidth(LayoutUnit childValue, Length cssUnit)
{
    if (cssUnit.type() != Auto)
        return (cssUnit.isFixed() ? LayoutUnit(cssUnit.value()) : childValue);
    return 0;
}

static LayoutUnit getBorderPaddingMargin(const RenderBoxModelObject& child, bool endOfInline)
{
    const RenderStyle& childStyle = child.style();
    if (endOfInline) {
        return getBPMWidth(child.marginEnd(), childStyle.marginEnd()) +
               getBPMWidth(child.paddingEnd(), childStyle.paddingEnd()) +
               child.borderEnd();
    }
    return getBPMWidth(child.marginStart(), childStyle.marginStart()) +
               getBPMWidth(child.paddingStart(), childStyle.paddingStart()) +
               child.borderStart();
}

static inline void stripTrailingSpace(float& inlineMax, float& inlineMin, RenderObject* trailingSpaceChild)
{
    if (is<RenderText>(trailingSpaceChild)) {
        // Collapse away the trailing space at the end of a block.
        RenderText& renderText = downcast<RenderText>(*trailingSpaceChild);
        const UChar space = ' ';
        const FontCascade& font = renderText.style().fontCascade(); // FIXME: This ignores first-line.
        float spaceWidth = font.width(RenderBlock::constructTextRun(&space, 1, renderText.style()));
        inlineMax -= spaceWidth + font.wordSpacing();
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

static inline LayoutUnit preferredWidth(LayoutUnit preferredWidth, float result)
{
    return std::max(preferredWidth, LayoutUnit::fromFloatCeil(result));
}

void RenderBlockFlow::computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    float inlineMax = 0;
    float inlineMin = 0;

    const RenderStyle& styleToUse = style();
    RenderBlock* containingBlock = this->containingBlock();
    LayoutUnit cw = containingBlock ? containingBlock->contentLogicalWidth() : 0_lu;

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = nullptr;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific cirucumstances (in order to match common WinIE renderings). 
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.) 
    bool allowImagesToBreak = !document().inQuirksMode() || !isTableCell() || !styleToUse.logicalWidth().isIntrinsicOrAuto();

    bool oldAutoWrap = styleToUse.autoWrap();

    InlineMinMaxIterator childIterator(*this);

    // Only gets added to the max preffered width once.
    bool addedTextIndent = false;
    // Signals the text indent was more negative than the min preferred width
    bool hasRemainingNegativeTextIndent = false;

    LayoutUnit textIndent = minimumValueForLength(styleToUse.textIndent(), cw);
    RenderObject* prevFloat = 0;
    bool isPrevChildInlineFlow = false;
    bool shouldBreakLineAfterText = false;
    bool canHangPunctuationAtStart = styleToUse.hangingPunctuation().contains(HangingPunctuation::First);
    bool canHangPunctuationAtEnd = styleToUse.hangingPunctuation().contains(HangingPunctuation::Last);
    RenderText* lastText = nullptr;

    bool addedStartPunctuationHang = false;
    
    while (RenderObject* child = childIterator.next()) {
        bool autoWrap = child->isReplaced() ? child->parent()->style().autoWrap() :
            child->style().autoWrap();
        if (!child->isBR()) {
            // Step One: determine whether or not we need to terminate our current line.
            // Each discrete chunk can become the new min-width, if it is the widest chunk
            // seen so far, and it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object. These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.
            //
            // (2) An inline non-text non-flow object, e.g., an inline replaced element.
            // These objects can always be on a line by themselves, so in this situation
            // we need to break the current line, and then add in our own margins and min/max
            // width on its own line, and then terminate the line.
            //
            // (3) A text object. Text runs can have breakable characters at the start,
            // the middle or the end. They may also lose whitespace off the front if
            // we're already ignoring whitespace. In order to compute accurate min-width
            // information, we need three pieces of information.
            // (a) the min-width of the first non-breakable run. Should be 0 if the text string
            // starts with whitespace.
            // (b) the min-width of the last non-breakable run. Should be 0 if the text string
            // ends with whitespace.
            // (c) the min/max width of the string (trimmed for whitespace).
            //
            // If the text string starts with whitespace, then we need to terminate our current line
            // (unless we're already in a whitespace stripping mode.
            //
            // If the text string has a breakable character in the middle, but didn't start
            // with whitespace, then we add the width of the first non-breakable run and
            // then end the current line. We then need to use the intermediate min/max width
            // values (if any of them are larger than our current min/max). We then look at
            // the width of the last non-breakable run and use that to start a new line
            // (unless we end in whitespace).
            const RenderStyle& childStyle = child->style();
            float childMin = 0;
            float childMax = 0;

            if (!child->isText()) {
                if (child->isLineBreakOpportunity()) {
                    minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                    continue;
                }
                // Case (1) and (2). Inline replaced and inline flow elements.
                if (is<RenderInline>(*child)) {
                    // Add in padding/border/margin from the appropriate side of
                    // the element.
                    float bpm = getBorderPaddingMargin(downcast<RenderInline>(*child), childIterator.endOfInline);
                    childMin += bpm;
                    childMax += bpm;

                    inlineMin += childMin;
                    inlineMax += childMax;

                    child->setPreferredLogicalWidthsDirty(false);
                } else {
                    // Inline replaced elts add in their margins to their min/max values.
                    if (!child->isFloating())
                        lastText = nullptr;
                    LayoutUnit margins;
                    Length startMargin = childStyle.marginStart();
                    Length endMargin = childStyle.marginEnd();
                    if (startMargin.isFixed())
                        margins += LayoutUnit::fromFloatCeil(startMargin.value());
                    if (endMargin.isFixed())
                        margins += LayoutUnit::fromFloatCeil(endMargin.value());
                    childMin += margins.ceilToFloat();
                    childMax += margins.ceilToFloat();
                }
            }

            if (!is<RenderInline>(*child) && !is<RenderText>(*child)) {
                // Case (2). Inline replaced elements and floats.
                // Terminate the current line as far as minwidth is concerned.
                LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
                computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);
                childMin += childMinPreferredLogicalWidth.ceilToFloat();
                childMax += childMaxPreferredLogicalWidth.ceilToFloat();

                bool clearPreviousFloat;
                if (child->isFloating()) {
                    clearPreviousFloat = (prevFloat
                        && ((prevFloat->style().floating() == Float::Left && (childStyle.clear() == Clear::Left || childStyle.clear() == Clear::Both))
                            || (prevFloat->style().floating() == Float::Right && (childStyle.clear() == Clear::Right || childStyle.clear() == Clear::Both))));
                    prevFloat = child;
                } else
                    clearPreviousFloat = false;

                bool canBreakReplacedElement = !child->isImage() || allowImagesToBreak;
                if (((canBreakReplacedElement && (autoWrap || oldAutoWrap) && (!isPrevChildInlineFlow || shouldBreakLineAfterText)) || clearPreviousFloat)) {
                    minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                }

                // If we're supposed to clear the previous float, then terminate maxwidth as well.
                if (clearPreviousFloat) {
                    maxLogicalWidth = preferredWidth(maxLogicalWidth, inlineMax);
                    inlineMax = 0;
                }

                // Add in text-indent. This is added in only once.
                if (!addedTextIndent && !child->isFloating()) {
                    LayoutUnit ceiledIndent = textIndent.ceilToFloat();
                    childMin += ceiledIndent;
                    childMax += ceiledIndent;

                    if (childMin < 0)
                        textIndent = LayoutUnit::fromFloatCeil(childMin);
                    else
                        addedTextIndent = true;
                }
                
                if (canHangPunctuationAtStart && !addedStartPunctuationHang && !child->isFloating())
                    addedStartPunctuationHang = true;

                // Add our width to the max.
                inlineMax += std::max<float>(0, childMax);

                if ((!autoWrap || !canBreakReplacedElement || (isPrevChildInlineFlow && !shouldBreakLineAfterText))) {
                    if (child->isFloating())
                        minLogicalWidth = preferredWidth(minLogicalWidth, childMin);
                    else
                        inlineMin += childMin;
                } else {
                    // Now check our line.
                    minLogicalWidth = preferredWidth(minLogicalWidth, childMin);

                    // Now start a new line.
                    inlineMin = 0;                    
                }

                if (autoWrap && canBreakReplacedElement && isPrevChildInlineFlow) {
                    minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                }

                // We are no longer stripping whitespace at the start of a line.
                if (!child->isFloating()) {
                    stripFrontSpaces = false;
                    trailingSpaceChild = nullptr;
                    lastText = nullptr;
                }
            } else if (is<RenderText>(*child)) {
                // Case (3). Text.
                RenderText& renderText = downcast<RenderText>(*child);

                if (renderText.style().hasTextCombine() && renderText.isCombineText())
                    downcast<RenderCombineText>(renderText).combineTextIfNeeded();

                // Determine if we have a breakable character. Pass in
                // whether or not we should ignore any spaces at the front
                // of the string. If those are going to be stripped out,
                // then they shouldn't be considered in the breakable char
                // check.
                bool strippingBeginWS = stripFrontSpaces;
                auto widths = renderText.trimmedPreferredWidths(inlineMax, stripFrontSpaces);

                childMin = widths.min;
                childMax = widths.max;

                // This text object will not be rendered, but it may still provide a breaking opportunity.
                if (!widths.hasBreak && !childMax) {
                    if (autoWrap && (widths.beginWS || widths.endWS)) {
                        minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = 0;
                    }
                    continue;
                }
                
                lastText = &renderText;

                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = 0;

                // Add in text-indent. This is added in only once.
                float ti = 0;
                if (!addedTextIndent || hasRemainingNegativeTextIndent) {
                    ti = textIndent.ceilToFloat();
                    childMin += ti;
                    widths.beginMin += ti;

                    // It the text indent negative and larger than the child minimum, we re-use the remainder
                    // in future minimum calculations, but using the negative value again on the maximum
                    // will lead to under-counting the max pref width.
                    if (!addedTextIndent) {
                        childMax += ti;
                        widths.beginMax += ti;
                        addedTextIndent = true;
                    }

                    if (childMin < 0) {
                        textIndent = childMin;
                        hasRemainingNegativeTextIndent = true;
                    }
                }
                
                // See if we have a hanging punctuation situation at the start.
                if (canHangPunctuationAtStart && !addedStartPunctuationHang) {
                    unsigned startIndex = strippingBeginWS ? renderText.firstCharacterIndexStrippingSpaces() : 0;
                    float hangStartWidth = renderText.hangablePunctuationStartWidth(startIndex);
                    childMin -= hangStartWidth;
                    widths.beginMin -= hangStartWidth;
                    childMax -= hangStartWidth;
                    widths.beginMax -= hangStartWidth;
                    addedStartPunctuationHang = true;
                }
                
                // If we have no breakable characters at all,
                // then this is the easy case. We add ourselves to the current
                // min and max and continue.
                if (!widths.hasBreakableChar)
                    inlineMin += childMin;
                else {
                    // We have a breakable character. Now we need to know if
                    // we start and end with whitespace.
                    if (widths.beginWS) {
                        // End the current line.
                        minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                    } else {
                        inlineMin += widths.beginMin;
                        minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                        childMin -= ti;
                    }

                    inlineMin = childMin;

                    if (widths.endWS) {
                        // We end in whitespace, which means we can end our current line.
                        minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = 0;
                        shouldBreakLineAfterText = false;
                    } else {
                        minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                        inlineMin = widths.endMin;
                        shouldBreakLineAfterText = true;
                    }
                }

                if (widths.hasBreak) {
                    inlineMax += widths.beginMax;
                    maxLogicalWidth = preferredWidth(maxLogicalWidth, inlineMax);
                    maxLogicalWidth = preferredWidth(maxLogicalWidth, childMax);
                    inlineMax = widths.endMax;
                    addedTextIndent = true;
                    addedStartPunctuationHang = true;
                } else
                    inlineMax += std::max<float>(0, childMax);
            }

            // Ignore spaces after a list marker.
            if (child->isListMarker())
                stripFrontSpaces = true;
        } else {
            minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
            maxLogicalWidth = preferredWidth(maxLogicalWidth, inlineMax);
            inlineMin = inlineMax = 0;
            stripFrontSpaces = true;
            trailingSpaceChild = 0;
            addedTextIndent = true;
            addedStartPunctuationHang = true;
        }

        if (!child->isText() && child->isRenderInline())
            isPrevChildInlineFlow = true;
        else
            isPrevChildInlineFlow = false;

        oldAutoWrap = autoWrap;
    }

    if (styleToUse.collapseWhiteSpace())
        stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);
    
    if (canHangPunctuationAtEnd && lastText && lastText->text().length() > 0) {
        unsigned endIndex = trailingSpaceChild == lastText ? lastText->lastCharacterIndexStrippingSpaces() : lastText->text().length() - 1;
        float endHangWidth = lastText->hangablePunctuationEndWidth(endIndex);
        inlineMin -= endHangWidth;
        inlineMax -= endHangWidth;
    }

    minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
    maxLogicalWidth = preferredWidth(maxLogicalWidth, inlineMax);
}

}
// namespace WebCore
