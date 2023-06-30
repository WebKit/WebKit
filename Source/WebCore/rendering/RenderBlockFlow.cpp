/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2015 Google Inc. All rights reserved.
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
#include "ElementInlines.h"
#include "FloatingObjects.h"
#include "FrameSelection.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestLocation.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBoxInlines.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorTextBox.h"
#include "InlineWalker.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutRepainter.h"
#include "LegacyInlineTextBox.h"
#include "LegacyLineLayout.h"
#include "LegacyRootInlineBox.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderMarquee.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderTableCellInlines.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
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

static_assert(sizeof(RenderBlockFlow::MarginValues) == sizeof(LayoutUnit[4]), "MarginValues should stay small");
static_assert(sizeof(RenderBlockFlow::MarginInfo) == sizeof(SameSizeAsMarginInfo), "MarginInfo should stay small");

// Our MarginInfo state used when laying out block children.
RenderBlockFlow::MarginInfo::MarginInfo(const RenderBlockFlow& block, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding)
    : m_atBeforeSideOfBlock(true)
    , m_atAfterSideOfBlock(false)
    , m_hasMarginBeforeQuirk(false)
    , m_hasMarginAfterQuirk(false)
    , m_determinedMarginBeforeQuirk(false)
{
    const RenderStyle& blockStyle = block.style();
    ASSERT(block.isRenderView() || block.parent());
    m_canCollapseWithChildren = !block.createsNewFormattingContext() && !block.isRenderView();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !beforeBorderPadding;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    m_canCollapseMarginAfterWithChildren = m_canCollapseWithChildren && !afterBorderPadding
        && blockStyle.logicalHeight().isAuto() && !blockStyle.logicalHeight().value();
    
    m_quirkContainer = block.isTableCell() || block.isBody();

    m_positiveMargin = m_canCollapseMarginBeforeWithChildren ? block.maxPositiveMarginBefore() : 0_lu;
    m_negativeMargin = m_canCollapseMarginBeforeWithChildren ? block.maxNegativeMarginBefore() : 0_lu;
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

    if (legacyLineLayout())
        legacyLineLayout()->lineBoxes().deleteLineBoxes();

    blockWillBeDestroyed();

    // NOTE: This jumps down to RenderBox, bypassing RenderBlock since it would do duplicate work.
    RenderBox::willBeDestroyed();
}

RenderMultiColumnFlow* RenderBlockFlow::multiColumnFlowSlowCase() const
{
    return rareBlockFlowData()->m_multiColumnFlow.get();
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

    // Inline blocks are covered by the isReplacedOrInlineBlock() check in the avoidFloats method.
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
    bool needAdjustIntrinsicLogicalWidthsForColumns = true;
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
            needAdjustIntrinsicLogicalWidthsForColumns = false;
        }
    } else if (childrenInline())
        computeInlinePreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);
    else
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    if (needAdjustIntrinsicLogicalWidthsForColumns)
        adjustIntrinsicLogicalWidthsForColumns(minLogicalWidth, maxLogicalWidth);

    if (!style().autoWrap() && childrenInline()) {
        // A horizontal marquee with inline children has no minimum width.
        auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
        if (scrollableArea && scrollableArea->marquee() && scrollableArea->marquee()->isHorizontal())
            minLogicalWidth = 0;
    }

    if (is<RenderTableCell>(*this)) {
        Length tableCellWidth = downcast<RenderTableCell>(*this).styleOrColLogicalWidth();
        if (tableCellWidth.isFixed() && tableCellWidth.value() > 0)
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(tableCellWidth));
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
        return LayoutUnit(style().fontDescription().computedSize()); // "1em" is recommended as the normal gap setting. Matches <p> margins.
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
    LayoutUnit colWidth = std::max(1_lu, LayoutUnit(style().columnWidth()));
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
    if (isRenderSVGBlock() || isRubyRun() || isRubyBlock() || isRubyInline() || isRubyBase())
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

void RenderBlockFlow::setChildrenInline(bool value)
{
    if (childrenInline() && !value) {
        setLineLayoutPath(UndeterminedPath);
        m_lineLayout = std::monostate();
    }

    RenderBlock::setChildrenInline(value);
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
    resetLogicalHeightBeforeLayoutIfNeeded();

    bool pageLogicalHeightChanged = false;
    checkForPaginationLogicalHeightChange(relayoutChildren, pageLogicalHeight, pageLogicalHeightChanged);

    LayoutUnit repaintLogicalTop;
    LayoutUnit repaintLogicalBottom;
    LayoutUnit maxFloatLogicalBottom;
    const RenderStyle& styleToUse = style();
    do {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || styleToUse.isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged);

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

            setHasMarginBeforeQuirk(styleToUse.marginBefore().hasQuirk());
            setHasMarginAfterQuirk(styleToUse.marginAfter().hasQuirk());
            setPaginationStrut(0);
        }
        if (!firstChild() && !isAnonymousBlock())
            setChildrenInline(true);
        dirtyForLayoutFromPercentageHeightDescendants();
        layoutInFlowChildren(relayoutChildren, repaintLogicalTop, repaintLogicalBottom, maxFloatLogicalBottom);

        // The containing block's of the floats within this BFC may specify margin-trim: block-end
        // so we need to go through the floats that this BFC is aware of and check if any of
        // the float's need their block-end margin trimmed
        if (m_floatingObjects && establishesBlockFormattingContext())
            trimFloatBlockEndMargins(blockFormattingContextInFlowBlockLevelContentHeight());

        // Expand our intrinsic height to encompass floats.
        LayoutUnit toAdd = borderAndPaddingAfter() + scrollbarLogicalHeight();
        if (lowestFloatLogicalBottom() > (logicalHeight() - toAdd) && createsNewFormattingContext())
            setLogicalHeight(lowestFloatLogicalBottom() + toAdd);
        if (shouldBreakAtLineToAvoidWidow()) {
            setEverHadLayout(true);
            continue;
        }
        break;
    } while (true);

    if (relayoutForPagination()) {
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
        // FIXME: This could be removed once relayoutForPagination() either stop recursing or we manage to
        // re-order them.
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || styleToUse.isFlippedBlocksWritingMode(), pageLogicalHeight, pageLogicalHeightChanged);

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
        // it had to lay out. We wouldn't need the hasNonVisibleOverflow() hack in that case either.
        LayoutUnit repaintLogicalLeft = logicalLeftVisualOverflow();
        LayoutUnit repaintLogicalRight = logicalRightVisualOverflow();
        if (hasNonVisibleOverflow()) {
            // If we have clipped overflow, we should use layout overflow as well, since visual overflow from lines didn't propagate to our block's overflow.
            // Note the old code did this as well but even for overflow:visible. The addition of hasNonVisibleOverflow() at least tightens up the hack a bit.
            // layoutInlineChildren should be patched to compute the entire repaint rect.
            repaintLogicalLeft = std::min(repaintLogicalLeft, logicalLeftLayoutOverflow());
            repaintLogicalRight = std::max(repaintLogicalRight, logicalRightLayoutOverflow());
        }
        
        LayoutRect repaintRect;
        if (isHorizontalWritingMode())
            repaintRect = LayoutRect(repaintLogicalLeft, repaintLogicalTop, repaintLogicalRight - repaintLogicalLeft, repaintLogicalBottom - repaintLogicalTop);
        else
            repaintRect = LayoutRect(repaintLogicalTop, repaintLogicalLeft, repaintLogicalBottom - repaintLogicalTop, repaintLogicalRight - repaintLogicalLeft);

        if (hasNonVisibleOverflow()) {
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

static RenderBlockFlow* firstInlineFormattingContextRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    // FIXME: Remove this after implementing "last line damage".
    for (auto* child = enclosingBlockContainer.firstChild(); child; child = child->previousSibling()) {
        if (!is<RenderBlockFlow>(child) || downcast<RenderBlockFlow>(*child).createsNewFormattingContext())
            continue;
        auto& blockContainer = downcast<RenderBlockFlow>(*child);
        if (blockContainer.hasLines())
            return &blockContainer;
        if (auto* descendantRoot = firstInlineFormattingContextRoot(blockContainer))
            return descendantRoot;
    }
    return nullptr;
}

static RenderBlockFlow* lastInlineFormattingContextRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    for (auto* child = enclosingBlockContainer.lastChild(); child; child = child->previousSibling()) {
        if (!is<RenderBlockFlow>(child) || downcast<RenderBlockFlow>(*child).createsNewFormattingContext())
            continue;
        auto& blockContainer = downcast<RenderBlockFlow>(*child);
        if (blockContainer.hasLines())
            return &blockContainer;
        if (auto* descendantRoot = lastInlineFormattingContextRoot(blockContainer))
            return descendantRoot;
    }
    return nullptr;
}

class TextBoxTrimmer {
public:
    TextBoxTrimmer(const RenderBlockFlow& flow, const RenderBlockFlow* inlineFormattingContextRootForTextBoxTrimEnd = nullptr)
        : m_flow(flow)
    {
        setTextBoxTrimForSubtree(inlineFormattingContextRootForTextBoxTrimEnd);
    }

    ~TextBoxTrimmer()
    {
        adjustTextBoxTrimAfterLayout();
    }

private:
    void setTextBoxTrimForSubtree(const RenderBlockFlow* inlineFormattingContextRootForTextBoxTrimEnd);
    void adjustTextBoxTrimAfterLayout();

    const RenderBlockFlow& m_flow;
};

void TextBoxTrimmer::setTextBoxTrimForSubtree(const RenderBlockFlow* inlineFormattingContextRootForTextBoxTrimEnd)
{
    auto* layoutState = m_flow.view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    auto textBoxTrim = m_flow.style().textBoxTrim();
    if (textBoxTrim == TextBoxTrim::None) {
        if (m_flow.borderAndPaddingStart()) {
            // For block containers: trim the block-start side of the first formatted line to the corresponding
            // text-box-edge metric of its root inline box. If there is no such line, or if there is intervening non-zero padding or borders,
            // there is no effect.
            layoutState->resetTextBoxTrim();
        }
        return;
    }
    // FIXME: Add support for nested leading trims if applicable.
    layoutState->resetTextBoxTrim();
    auto applyTextBoxTrimStart = textBoxTrim == TextBoxTrim::Start || textBoxTrim == TextBoxTrim::Both;
    auto applyTextBoxTrimEnd = (textBoxTrim == TextBoxTrim::End || textBoxTrim == TextBoxTrim::Both) && inlineFormattingContextRootForTextBoxTrimEnd;
    if (applyTextBoxTrimEnd) {
        layoutState->addTextBoxTrimEnd(*inlineFormattingContextRootForTextBoxTrimEnd);
        // FIXME: Instead we should just damage the last line.
        if (auto* firstFormattedLineRoot = firstInlineFormattingContextRoot(m_flow); firstFormattedLineRoot && firstFormattedLineRoot != inlineFormattingContextRootForTextBoxTrimEnd) {
            // When we run a "last line" layout on the last formatting context, we should not trim the first line ever (see FIXME).
            applyTextBoxTrimStart = false;
        }
    }
    if (applyTextBoxTrimStart)
        layoutState->addTextBoxTrimStart();
}

void TextBoxTrimmer::adjustTextBoxTrimAfterLayout()
{
    auto* layoutState = m_flow.view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    if (m_flow.style().textBoxTrim() != TextBoxTrim::None)
        return layoutState->resetTextBoxTrim();
    // This is propagated text-box-trim.
    if (layoutState->hasTextBoxTrimStart() && m_flow.hasLines()) {
        // Only the first formatted line is trimmed.
        layoutState->removeTextBoxTrimStart();
    }
}

void RenderBlockFlow::layoutInFlowChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom, LayoutUnit& maxFloatLogicalBottom)
{
    if (!firstChild()) {
        auto logicalHeight = borderAndPaddingBefore() + borderAndPaddingAfter() + scrollbarLogicalHeight();
        if (hasLineIfEmpty())
            logicalHeight += lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
        setLogicalHeight(logicalHeight);

        repaintLogicalTop = { };
        repaintLogicalBottom = { };
        maxFloatLogicalBottom = { };
        return;
    }

    if (childrenInline()) {
        auto textBoxTrimmer = TextBoxTrimmer { *this, this };
        layoutInlineChildren(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
        return;
    }

    {
        // With block children, there's no way to tell what the last formatted line is until after we finished laying out the subtree.
        auto textBoxTrimmer = TextBoxTrimmer { *this };
        layoutBlockChildren(relayoutChildren, maxFloatLogicalBottom);
    }

    auto handleTextBoxTrimEnd = [&] {
        auto hasTextBoxTrimEnd = style().textBoxTrim() == TextBoxTrim::End || style().textBoxTrim() == TextBoxTrim::Both;
        if (!hasTextBoxTrimEnd)
            return;
        // Dirty the last formatted line (in the last IFC) and issue relayout with forcing trimming the last line.
        if (auto* rootForLastFormattedLine = lastInlineFormattingContextRoot(*this)) {
            for (RenderBlock* ancestor = rootForLastFormattedLine; ancestor && ancestor != this; ancestor = ancestor->containingBlock()) {
                // FIXME: We should be able to damage the last line only.
                ancestor->setNeedsLayout(MarkOnlyThis);
            }

            auto textBoxTrimmer = TextBoxTrimmer { *this, rootForLastFormattedLine };
            layoutBlockChildren(relayoutChildren, maxFloatLogicalBottom);
        }
    };
    handleTextBoxTrimEnd();
}

void RenderBlockFlow::layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom)
{
    ASSERT(firstChild());

    LayoutUnit beforeEdge = borderAndPaddingBefore();
    LayoutUnit afterEdge = borderAndPaddingAfter() + scrollbarLogicalHeight();

    setLogicalHeight(beforeEdge);
    auto* layoutState = view().frameView().layoutContext().layoutState(); 
    // Lay out our hypothetical grid line as though it occurs at the top of the block.
    if (layoutState->lineGrid() == this)
        layoutLineGridBox();

    // The margin struct caches all our current margin collapsing state.
    MarginInfo marginInfo(*this, beforeEdge, afterEdge);

    auto hasMarginTrimState = false;
    auto updateMarginTrimStateIfNeeded = [&] {
        auto containingBlockTrimmingState = layoutState->blockStartTrimming();
        if (style().marginTrim().contains(MarginTrimType::BlockStart))
            layoutState->pushBlockStartTrimming(true);
        else if (!marginInfo.canCollapseMarginBeforeWithChildren() && containingBlockTrimmingState)
            layoutState->pushBlockStartTrimming(false);
        else if (marginInfo.canCollapseMarginBeforeWithChildren() && containingBlockTrimmingState)
            layoutState->pushBlockStartTrimming(containingBlockTrimmingState.value());
        else
            return;
        hasMarginTrimState = true;
    };

    updateMarginTrimStateIfNeeded();

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
            auto markSiblingsIfIntrudingForLayout = [&] {
                // Let's find out if this float box is (was) intruding to sibling boxes and mark them for layout accordingly. 
                if (!child.selfNeedsLayout() || !child.everHadLayout()) {
                    // At this point floatingObjectSet() is purged, we can't check whether
                    // this is a new or an existing float in this block container.
                    return;
                }
                for (auto* nextSibling = child.nextSibling(); nextSibling; nextSibling = nextSibling->nextSibling()) {
                    if (!is<RenderBlockFlow>(*nextSibling))
                        continue;
                    auto& block = downcast<RenderBlockFlow>(*nextSibling);
                    if (block.avoidsFloats() && !block.shrinkToAvoidFloats())
                        continue;
                    if (block.containsFloat(child))
                        block.markAllDescendantsWithFloatsForLayout();
                }
            };
            markSiblingsIfIntrudingForLayout();
            insertFloatingObject(child);
            adjustFloatingBlock(marginInfo);
            continue;
        }

        // Lay out the child.
        layoutBlockChild(child, marginInfo, previousFloatLogicalBottom, maxFloatLogicalBottom);
    }
    
    if (style().marginTrim().contains(MarginTrimType::BlockEnd))
        trimBlockEndChildrenMargins();
    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    handleAfterSideOfBlock(beforeEdge, afterEdge, marginInfo);
    if (hasMarginTrimState)
        layoutState->popBlockStartTrimming();
}


void RenderBlockFlow::trimBlockEndChildrenMargins()
{
    auto trimSelfCollapsingChildDescendants = [&](RenderBox& child) {
        ASSERT(child.isSelfCollapsingBlock());
        for (auto itr = RenderIterator<RenderBox>(&child, child.firstChildBox()); itr; itr = itr.traverseNext()) {
            setTrimmedMarginForChild(*itr, MarginTrimType::BlockStart);
            setTrimmedMarginForChild(*itr, MarginTrimType::BlockEnd);
        }
    };

    ASSERT(style().marginTrim().contains(MarginTrimType::BlockEnd));
    // If we are trimming the block end margin, we need to make sure we trim the margin of the children
    // at the end of the block by walking back up the container. Any self collapsing children will also need to
    // have their position adjusted to below the last non self-collapsing child in its containing block
    auto* child = lastChildBox();
    while (child) {
        if (child->isExcludedFromNormalLayout() || !child->isInFlow()) {
            child = child->previousSiblingBox();
            continue;
        }

        auto* childContainingBlock = child->containingBlock();
        setTrimmedMarginForChild(*child, MarginTrimType::BlockEnd);
        if (child->isSelfCollapsingBlock()) {
            setTrimmedMarginForChild(*child, MarginTrimType::BlockStart);
            childContainingBlock->setLogicalTopForChild(*child, childContainingBlock->logicalHeight());
            
            // If this self-collapsing child has any other children, which must also be
            // self-collapsing, we should trim the margins of all its descendants
            if (child->firstChildBox() && !child->childrenInline())
                trimSelfCollapsingChildDescendants(*child);

            child = child->previousSiblingBox();
        }  else if (auto* nestedBlock = dynamicDowncast<RenderBlockFlow>(child); nestedBlock && nestedBlock->isBlockContainer() && !nestedBlock->childrenInline() && !nestedBlock->style().marginTrim().contains(MarginTrimType::BlockEnd)) {
            MarginInfo nestedBlockMarginInfo(*nestedBlock, nestedBlock->borderAndPaddingBefore(), nestedBlock->borderAndPaddingAfter());
            // The margins *inside* this nested block are protected so we should not introspect and try to
            // trim any of them.
            if (!nestedBlockMarginInfo.canCollapseMarginAfterWithChildren())
                break;

            child = child->lastChildBox();
        } else
            // We hit another type of block child that doesn't apply to our search. We can just
            // end the search since nothing before this block can affect the bottom margin of the outer one we are trimming for.
            break;
    }
}

void RenderBlockFlow::simplifiedNormalFlowLayout()
{
    if (!childrenInline()) {
        RenderBlock::simplifiedNormalFlowLayout();
        return;
    }

    bool shouldUpdateOverflow = false;
    ListHashSet<LegacyRootInlineBox*> lineBoxes;
    for (InlineWalker walker(*this); !walker.atEnd(); walker.advance()) {
        RenderObject& renderer = *walker.current();
        if (!renderer.isOutOfFlowPositioned() && (renderer.isReplacedOrInlineBlock() || renderer.isFloating())) {
            RenderBox& box = downcast<RenderBox>(renderer);
            box.layoutIfNeeded();
            shouldUpdateOverflow = true;
            if (box.inlineBoxWrapper())
                lineBoxes.add(&box.inlineBoxWrapper()->root());
        } else if (is<RenderText>(renderer) || is<RenderInline>(renderer))
            renderer.clearNeedsLayout();
    }

    if (!shouldUpdateOverflow)
        return;

    if (auto* lineLayout = modernLineLayout()) {
        lineLayout->updateOverflow();
        return;
    }

    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    for (auto it = lineBoxes.begin(), end = lineBoxes.end(); it != end; ++it) {
        LegacyRootInlineBox* box = *it;
        box->computeOverflow(box->lineTop(), box->lineBottom(), textBoxDataMap);
    }
}

void RenderBlockFlow::computeAndSetLineLayoutPath()
{
    if (lineLayoutPath() != UndeterminedPath)
        return;

    auto compute = [&] {
        if (LayoutIntegration::LineLayout::canUseFor(*this))
            return ModernPath;
        return LegacyPath;
    };

    setLineLayoutPath(compute());
}

void RenderBlockFlow::layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    computeAndSetLineLayoutPath();

    if (lineLayoutPath() == ModernPath) {
        layoutModernLines(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
        return;
    }

    if (!legacyLineLayout())
        m_lineLayout = makeUnique<LegacyLineLayout>(*this);

    legacyLineLayout()->layoutLineBoxes(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
    m_previousModernLineLayoutContentBoxLogicalHeight = { };
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

#if ASSERT_ENABLED
    LayoutSize oldLayoutDelta = view().frameView().layoutContext().layoutDelta();
#endif
    // Position the child as though it didn't collapse with the top.
    setLogicalTopForChild(child, logicalTopEstimate, ApplyLayoutDelta);
    estimateFragmentRangeForBoxChild(child);

    auto* childBlockFlow = dynamicDowncast<RenderBlockFlow>(child);
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
    if (marginInfo.atBeforeSideOfBlock() && !child.isSelfCollapsingBlock()) {
        marginInfo.setAtBeforeSideOfBlock(false);

        if (auto* layoutState = frame().view()->layoutContext().layoutState(); layoutState && layoutState->blockStartTrimming()) {
            layoutState->popBlockStartTrimming();
            layoutState->pushBlockStartTrimming(false);
        }
    }
    // Now place the child in the correct left position
    determineLogicalLeftPositionForChild(child, ApplyLayoutDelta);

    // Update our height now that the child has been placed in the correct position.
    setLogicalHeight(logicalHeight() + logicalHeightForChildForFragmentation(child));

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

void RenderBlockFlow::determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode applyDelta)
{
    LayoutUnit startPosition = borderStart() + paddingStart();
    LayoutUnit initialStartPosition = startPosition;
    if ((shouldPlaceVerticalScrollbarOnLeft() || style().scrollbarGutter().bothEdges) && isHorizontalWritingMode())
        startPosition += (style().isLeftToRightDirection() ? 1 : -1) * verticalScrollbarWidth();
    if (style().scrollbarGutter().bothEdges && !isHorizontalWritingMode())
        startPosition += (style().isLeftToRightDirection() ? 1 : -1) * horizontalScrollbarHeight();
    LayoutUnit totalAvailableLogicalWidth = borderAndPaddingLogicalWidth() + availableLogicalWidth();

    LayoutUnit childMarginStart = marginStartForChild(child);
    LayoutUnit newPosition = startPosition + childMarginStart;

    LayoutUnit positionToAvoidFloats;
    
    if (child.avoidsFloats() && containsFloats())
        positionToAvoidFloats = startOffsetForLine(logicalTopForChild(child), IndentTextOrNot::DoNotIndentText, logicalHeightForChild(child));

    // If the child has an offset from the content edge to avoid floats then use that, otherwise let any negative
    // margin pull it back over the content edge or any positive margin push it out.
    // If the child is being centred then the margin calculated to do that has factored in any offset required to
    // avoid floats, so use it if necessary.

    if (style().textAlign() == TextAlignMode::WebKitCenter || child.style().marginStartUsing(&style()).isAuto())
        newPosition = std::max(newPosition, positionToAvoidFloats + childMarginStart);
    else if (positionToAvoidFloats > initialStartPosition)
        newPosition = std::max(newPosition, positionToAvoidFloats);

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

LayoutUnit RenderBlockFlow::startAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText)
{
    TextAlignMode textAlign = style().textAlign();
    bool shouldApplyIndentText = false;
    switch (textAlign) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        shouldApplyIndentText = style().isLeftToRightDirection();
        break;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        shouldApplyIndentText = !style().isLeftToRightDirection();
        break;
    case TextAlignMode::Start:
        shouldApplyIndentText = true;
        break;
    default:
        shouldApplyIndentText = false;
    }
    if (shouldApplyIndentText) // FIXME: Handle TextAlignMode::End here
        return startOffsetForLine(position, shouldIndentText);

    // updateLogicalWidthForAlignment() handles the direction of the block so no need to consider it here
    float totalLogicalWidth = 0;
    float logicalLeft = logicalLeftOffsetForLine(logicalHeight(), DoNotIndentText);
    float availableLogicalWidth = logicalRightOffsetForLine(logicalHeight(), DoNotIndentText) - logicalLeft;

    LegacyLineLayout::updateLogicalWidthForAlignment(*this, textAlign, nullptr, nullptr, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

    if (!style().isLeftToRightDirection())
        return LayoutUnit(logicalWidth() - logicalLeft);

    return LayoutUnit(logicalLeft);
}

RenderBlockFlow::MarginValues RenderBlockFlow::marginValuesForChild(RenderBox& child) const
{
    LayoutUnit childBeforePositive;
    LayoutUnit childBeforeNegative;
    LayoutUnit childAfterPositive;
    LayoutUnit childAfterNegative;

    LayoutUnit beforeMargin;
    LayoutUnit afterMargin;

    auto* childRenderBlock = dynamicDowncast<RenderBlockFlow>(child);
    
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

std::optional<LayoutUnit> RenderBlockFlow::selfCollapsingMarginBeforeWithClear(RenderObject* candidate)
{
    if (!is<RenderBlockFlow>(candidate))
        return { };

    auto& candidateBlockFlow = downcast<RenderBlockFlow>(*candidate);
    if (!candidateBlockFlow.isSelfCollapsingBlock())
        return { };

    if (RenderStyle::usedClear(candidateBlockFlow) == UsedClear::None || !containsFloats())
        return { };

    auto clear = getClearDelta(candidateBlockFlow, candidateBlockFlow.logicalHeight());
    // Just because a block box has the clear property set, it does not mean we always get clearance (e.g. when the box is below the cleared floats)
    if (clear < candidateBlockFlow.logicalBottom())
        return { };

    return marginValuesForChild(candidateBlockFlow).positiveMarginBefore();
}

LayoutUnit RenderBlockFlow::collapseMarginsWithChildInfo(RenderBox* child, RenderObject* prevSibling, MarginInfo& marginInfo)
{
    bool childIsSelfCollapsing = child ? child->isSelfCollapsingBlock() : false;
    bool beforeQuirk = child ? hasMarginBeforeQuirk(*child) : false;
    bool afterQuirk = child ? hasMarginAfterQuirk(*child) : false;
    auto trimChildBlockMargins = [&]() {
        auto childBlockFlow = dynamicDowncast<RenderBlockFlow>(child);
        if (childBlockFlow)
            childBlockFlow->setMaxMarginBeforeValues(0_lu, 0_lu);
        setTrimmedMarginForChild(*child, MarginTrimType::BlockStart);

        // The margin after for a self collapsing child should also be trimmed so it does not 
        // influence the margins of the first non collapsing child
        if (childIsSelfCollapsing) {
            if (childBlockFlow)
                childBlockFlow->setMaxMarginAfterValues(0_lu, 0_lu);
            setTrimmedMarginForChild(*child, MarginTrimType::BlockEnd);
        }
    };
    if (frame().view()->layoutContext().layoutState()->blockStartTrimming().value_or(false)) {
        ASSERT(marginInfo.atBeforeSideOfBlock());
        trimChildBlockMargins();
    }

    // Get the four margin values for the child and cache them.
    MarginValues childMargins = child ? marginValuesForChild(*child) : MarginValues(0, 0, 0, 0);
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
    }

    if (marginInfo.quirkContainer() && marginInfo.atBeforeSideOfBlock() && (posTop - negTop))
        marginInfo.setHasMarginBeforeQuirk(beforeQuirk);

    LayoutUnit beforeCollapseLogicalTop = logicalHeight();
    LayoutUnit logicalTop = beforeCollapseLogicalTop;
    // If the child's previous sibling is a self-collapsing block that cleared a float then its top border edge has been set at the bottom border edge
    // of the float. Since we want to collapse the child's top margin with the self-collapsing block's top and bottom margins we need to adjust our parent's height to match the 
    // margin top of the self-collapsing block. If the resulting collapsed margin leaves the child still intruding into the float then we will want to clear it.
    if (!marginInfo.canCollapseWithMarginBefore()) {
        if (auto value = selfCollapsingMarginBeforeWithClear(child->previousSibling()))
            setLogicalHeight(logicalHeight() - *value);
    }

    if (childIsSelfCollapsing) {
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

        if (!marginInfo.canCollapseWithMarginBefore()) {
            // We need to make sure that the position of the self-collapsing block
            // is correct, since it could have overflowing content
            // that needs to be positioned correctly (e.g., a block that
            // had a specified height of 0 but that actually had subcontent).
            logicalTop = logicalHeight() + collapsedBeforePos - collapsedBeforeNeg;
        }
    } else {
        if (!marginInfo.atBeforeSideOfBlock() || (!marginInfo.canCollapseMarginBeforeWithChildren()
            && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginBeforeQuirk()))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            setLogicalHeight(logicalHeight() + std::max(marginInfo.positiveMargin(), posTop) - std::max(marginInfo.negativeMargin(), negTop));
            logicalTop = logicalHeight();
        }

        marginInfo.setPositiveMargin(childMargins.positiveMarginAfter());
        marginInfo.setNegativeMargin(childMargins.negativeMarginAfter());

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

        // If |child|'s previous sibling is or contains a self-collapsing block that cleared a float and margin collapsing resulted in |child| moving up
        // into the margin area of the self-collapsing block then the float it clears is now intruding into |child|. Layout again so that we can look for
        // floats in the parent that overhang |child|'s new logical top.
        bool logicalTopIntrudesIntoFloat = logicalTop < beforeCollapseLogicalTop;
        if (child && logicalTopIntrudesIntoFloat && containsFloats() && !child->avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
            child->setNeedsLayout();
    }

    return logicalTop;
}

bool RenderBlockFlow::isChildEligibleForMarginTrim(MarginTrimType marginTrimType, const RenderBox& child) const
{
    ASSERT(style().marginTrim().contains(marginTrimType));
    if (!child.style().isDisplayBlockLevel())
        return false;
    if (marginTrimType == MarginTrimType::BlockStart)
        return firstInFlowChildBox() == &child;
    return lastInFlowChildBox() == &child;
}

void RenderBlockFlow::trimFloatBlockEndMargins(LayoutUnit blockFormattingContextInFlowContentHeight)
{
    ASSERT(m_floatingObjects && establishesBlockFormattingContext());
    auto computeFloatMarginAfterStartLocationInBlockFormattingContext = [&](const RenderBox& floatBox) {
        // Start with the location of the float within its containing block. If this containing
        // block is the BFC in which we are trimming, then we can use this as the starting location
        // since this is the float's positin within the BFC. If not, add the containing block's location
        // to the float's location. This new value should be the location of the float relative to this
        // new containing block. Keep doing this until we find the containing block that establishes
        // a BFC
        auto bfcRoot = floatBox.blockFormattingContextRoot();
        ASSERT(bfcRoot);

        LayoutSize offsetInsideBlockFormattingContext;
        const RenderElement* currentBox = &floatBox;
        while (currentBox != bfcRoot) {
            offsetInsideBlockFormattingContext += currentBox->offsetFromContainer(*currentBox->container(), LayoutPoint { });
            currentBox = currentBox->container();
        }
        if (bfcRoot->isHorizontalWritingMode())
            return LayoutUnit { offsetInsideBlockFormattingContext.height() + bfcRoot->logicalHeightForChild(floatBox) };
        return LayoutUnit { offsetInsideBlockFormattingContext.width() + bfcRoot->logicalWidthForChild(floatBox) };
    };
    for (auto& floatingObject : m_floatingObjects->set()) {
        auto& floatBox = floatingObject->renderer();
        auto floatContainingBlock = floatBox.containingBlock();
        if (!floatContainingBlock->style().marginTrim().contains(MarginTrimType::BlockEnd) || floatBox.marginAfter() <= 0)
            continue;

        // There are 3 different cases that need to be taken into consideration when trimming the
        // block-end margin of a float:
        // 1.) The float margin box extends past the deepest past of content (margin should be trimmed all the way)
        // 2.) The end of the deepest piece of is in between the float's border box and marign box (marign should be trimmed up to the content)
        // 3.) The deepest piece of content is below the float's margin box (no trimming occurs)
        auto floatMarginAfterStartLocation = computeFloatMarginAfterStartLocationInBlockFormattingContext(floatBox);
        auto floatMarginAfter = floatContainingBlock->marginAfterForChild(floatBox);
        if (floatMarginAfterStartLocation >= blockFormattingContextInFlowContentHeight)
            trimMarginForFloat(*floatingObject, MarginTrimType::BlockEnd, floatMarginAfter);
        else if (blockFormattingContextInFlowContentHeight > floatMarginAfterStartLocation && (blockFormattingContextInFlowContentHeight < floatMarginAfterStartLocation + floatMarginAfter))
            trimMarginForFloat(*floatingObject, MarginTrimType::BlockEnd, floatMarginAfterStartLocation + floatMarginAfter - blockFormattingContextInFlowContentHeight);
    }
}

bool RenderBlockFlow::shouldChildInlineMarginContributeToContainerIntrinsicSize(MarginTrimType marginSide, const RenderElement& child) const
{
    // For the purposes of computing a block container's intrinsic size, a float should not
    // include its trimmed margins in its intrinsic size contributions
    ASSERT((marginSide == MarginTrimType::InlineStart || marginSide == MarginTrimType::InlineEnd) && (child.containingBlock() == this));
    return !child.isFloating() || !style().marginTrim().contains(marginSide);
}

LayoutUnit RenderBlockFlow::clearFloatsIfNeeded(RenderBox& child, MarginInfo& marginInfo, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos)
{
    LayoutUnit heightIncrease = getClearDelta(child, yPos);
    if (!heightIncrease)
        return yPos;

    if (child.isSelfCollapsingBlock()) {
        // For self-collapsing blocks that clear, they can still collapse their
        // margins with following siblings. Reset the current margins to represent
        // the self-collapsing block's margins only.
        MarginValues childMargins = marginValuesForChild(child);
        marginInfo.setPositiveMargin(std::max(childMargins.positiveMarginBefore(), childMargins.positiveMarginAfter()));
        marginInfo.setNegativeMargin(std::max(childMargins.negativeMarginBefore(), childMargins.negativeMarginAfter()));

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
        // https://www.w3.org/TR/CSS2/visuren.html#clearance
        // "CSS2.1 - Computing the clearance of an element on which 'clear' is set is done..."
        setMaxMarginBeforeValues(oldTopPosMargin, oldTopNegMargin);
        marginInfo.setAtBeforeSideOfBlock(false);
    }

    return yPos + heightIncrease;
}

void RenderBlockFlow::marginBeforeEstimateForChild(RenderBox& child, LayoutUnit& positiveMarginBefore, LayoutUnit& negativeMarginBefore) const
{
    // Give up if in quirks mode and we're a body/table cell and the top margin of the child box is quirky.
    // Give up if the child specified -webkit-margin-collapse: separate that prevents collapsing.
    if (document().inQuirksMode() && hasMarginBeforeQuirk(child) && (isTableCell() || isBody()))
        return;

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
    if (!grandchildBox || RenderStyle::usedClear(*grandchildBox) != UsedClear::None)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(*this);
        if (is<RenderBlock>(*grandchildBox)) {
            RenderBlock& grandchildBlock = downcast<RenderBlock>(*grandchildBox);
            grandchildBlock.setHasMarginBeforeQuirk(grandchildBox->style().marginBefore().hasQuirk());
            grandchildBlock.setHasMarginAfterQuirk(grandchildBox->style().marginAfter().hasQuirk());
        }
    }

    // Collapse the margin of the grandchild box with our own to produce an estimate.
    childBlock.marginBeforeEstimateForChild(*grandchildBox, positiveMarginBefore, negativeMarginBefore);
}

LayoutUnit RenderBlockFlow::estimateLogicalTopPosition(RenderBox& child, const MarginInfo& marginInfo, LayoutUnit& estimateWithoutPagination)
{
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    LayoutUnit logicalTopEstimate = logicalHeight();
    if (!marginInfo.canCollapseWithMarginBefore()) {
        LayoutUnit positiveMarginBefore;
        LayoutUnit negativeMarginBefore;
        if (child.selfNeedsLayout()) {
            // Try to do a basic estimation of how the collapse is going to go.
            marginBeforeEstimateForChild(child, positiveMarginBefore, negativeMarginBefore);
        } else {
            // Use the cached collapsed margin values from a previous layout. Most of the time they
            // will be right.
            MarginValues marginValues = marginValuesForChild(child);
            positiveMarginBefore = std::max(positiveMarginBefore, marginValues.positiveMarginBefore());
            negativeMarginBefore = std::max(negativeMarginBefore, marginValues.negativeMarginBefore());
        }

        // Collapse the result with our current margins.
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
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        auto shouldTrimBlockEndMargin = style().marginTrim().contains(MarginTrimType::BlockEnd);
        auto propagatedPositiveMargin = shouldTrimBlockEndMargin ? 0_lu : marginInfo.positiveMargin();
        auto propagatedNegativeMargin = shouldTrimBlockEndMargin ? 0_lu : marginInfo.negativeMargin();
        setMaxMarginAfterValues(std::max(maxPositiveMarginAfter(), propagatedPositiveMargin), std::max(maxNegativeMarginAfter(), propagatedNegativeMargin));

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
    if (auto value = selfCollapsingMarginBeforeWithClear(lastChild()))
        setLogicalHeight(logicalHeight() - *value);

    // If we can't collapse with children then add in the bottom margin.
    if (!marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()
        && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginAfterQuirk())) {
        setLogicalHeight(logicalHeight() + marginInfo.margin());
    }

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
    bool checkColumnBreaks = fragmentedFlow && fragmentedFlow->shouldCheckColumnBreaks() && (!shouldApplyLayoutContainment() || child.previousSibling());
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

        // So our margin doesn't participate in the next collapsing steps.
        marginInfo.clearMargin();

        if (checkColumnBreaks) {
            if (isInsideMulticolFlow)
                checkFragmentBreaks = true;
        }
        if (checkFragmentBreaks) {
            LayoutUnit offsetBreakAdjustment;
            if (fragmentedFlow->addForcedFragmentBreak(this, offsetFromLogicalTopOfFirstPage() + logicalOffset, &child, false, &offsetBreakAdjustment))
                return logicalOffset + offsetBreakAdjustment;
        }
        return nextPageLogicalTop(logicalOffset, IncludePageBoundary);
    }
    return logicalOffset;
}

LayoutUnit RenderBlockFlow::adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox& child, bool atBeforeSideOfBlock)
{
    auto* childRenderBlock = dynamicDowncast<RenderBlock>(child);

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

    if (child.shouldApplySizeContainment())
        adjustSizeContainmentChildForPagination(child, result);

    // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
    LayoutUnit logicalTopBeforeUnsplittableAdjustment = result;
    LayoutUnit logicalTopAfterUnsplittableAdjustment = adjustForUnsplittableChild(child, result);
    
    LayoutUnit paginationStrut;
    LayoutUnit unsplittableAdjustmentDelta = logicalTopAfterUnsplittableAdjustment - logicalTopBeforeUnsplittableAdjustment;
    LayoutUnit childLogicalHeight = child.logicalHeight();
    if (unsplittableAdjustmentDelta) {
        setPageBreak(result, childLogicalHeight - unsplittableAdjustmentDelta);
        paginationStrut = unsplittableAdjustmentDelta;
    } else if (childRenderBlock && childRenderBlock->paginationStrut())
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

    if (!unsplittableAdjustmentDelta) {
        if (LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(result)) {
            LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(result, ExcludePageBoundary);
            LayoutUnit spaceShortage = child.logicalHeight() - remainingLogicalHeight;
            if (spaceShortage > 0) {
                // If the child crosses a column boundary, report a break, in case nothing inside it
                // has already done so. The column balancer needs to know how much it has to stretch
                // the columns to make more content fit. If no breaks are reported (but do occur),
                // the balancer will have no clue. Only measure the space after the last column
                // boundary, in case it crosses more than one.
                LayoutUnit spaceShortageInLastColumn = intMod(spaceShortage, pageLogicalHeight);
                setPageBreak(result, spaceShortageInLastColumn ? spaceShortageInLastColumn : spaceShortage);
            } else if (remainingLogicalHeight == pageLogicalHeight && offsetFromLogicalTopOfFirstPage() + child.logicalTop()) {
                // We're at the very top of a page or column, and it's not the first one. This child
                // may turn out to be the smallest piece of content that causes a page break, so we
                // need to report it.
                setPageBreak(result, childLogicalHeight);
            }
        }
    }

    // Similar to how we apply clearance. Boost height() to be the place where we're going to position the child.
    setLogicalHeight(logicalHeight() + (result - oldTop));
    
    // Return the final adjusted logical top.
    return result;
}

static inline LayoutUnit calculateMinimumPageHeight(const RenderStyle& renderStyle, const InlineIterator::LineBoxIterator& lastLine, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // We may require a certain minimum number of lines per page in order to satisfy
    // orphans and widows, and that may affect the minimum page height.
    unsigned lineCount = std::max<unsigned>(renderStyle.hasAutoOrphans() ? 1 : renderStyle.orphans(), renderStyle.hasAutoWidows() ? 1 : renderStyle.widows());
    if (lineCount > 1) {
        auto line = lastLine;
        for (unsigned i = 1; i < lineCount && line->previous(); i++)
            line = line->previous();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        lineTop = std::min(line->logicalTop(), line->inkOverflowTop());
    }
    return lineBottom - lineTop;
}

static inline bool needsAppleMailPaginationQuirk(const RenderBlockFlow& renderer)
{
    if (!renderer.settings().appleMailPaginationQuirkEnabled())
        return false;

    if (renderer.element() && renderer.element()->idForStyleResolution() == "messageContentContainer"_s)
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

void RenderBlockFlow::adjustLinePositionForPagination(LegacyRootInlineBox* rootInlineBox, LayoutUnit& delta)
{
    auto adjustment = computeLineAdjustmentForPagination(rootInlineBox, delta);

    rootInlineBox->setPaginationStrut(adjustment.strut);
    rootInlineBox->setIsFirstAfterPageBreak(adjustment.isFirstAfterPageBreak);

    delta += adjustment.strut;
}

RenderBlockFlow::LinePaginationAdjustment RenderBlockFlow::computeLineAdjustmentForPagination(const InlineIterator::LineBoxIterator& lineBox, LayoutUnit delta)
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
    auto computeLeafBoxTopAndBottom = [&] {
        auto lineTop = LayoutUnit::max();
        auto lineBottom = LayoutUnit::min();
        for (auto box = lineBox->firstLeafBox(); box; box.traverseNextOnLine()) {
            if (box->logicalTop() < lineTop)
                lineTop = box->logicalTop();
            if (box->logicalBottom() > lineBottom)
                lineBottom = box->logicalBottom();
        }
        return std::pair { lineTop, lineBottom };
    };

    auto logicalOverflowTop = LayoutUnit { lineBox->inkOverflowTop() };
    auto logicalOverflowBottom = LayoutUnit { lineBox->inkOverflowBottom() };
    auto logicalOverflowHeight = logicalOverflowBottom - logicalOverflowTop;
    auto logicalTop = LayoutUnit { lineBox->logicalTop() };
    auto logicalOffset = std::min(logicalTop, logicalOverflowTop);
    auto logicalBottom = std::max(LayoutUnit { lineBox->logicalBottom() }, logicalOverflowBottom);
    auto lineHeight = logicalBottom - logicalOffset;

    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(style(), lineBox, logicalOffset, logicalBottom));
    logicalOffset += delta;

    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);

    auto* fragmentedFlow = enclosingFragmentedFlow();
    bool hasUniformPageLogicalHeight = !fragmentedFlow || fragmentedFlow->fragmentsHaveUniformLogicalHeight();
    // If lineHeight is greater than pageLogicalHeight, but logicalVisualOverflow.height() still fits, we are
    // still going to add a strut, so that the visible overflow fits on a single page.
    if (!pageLogicalHeight || !hasNextPage(logicalOffset)) {
        // FIXME: In case the line aligns with the top of the page (or it's slightly shifted downwards) it will not be marked as the first line in the page.
        // From here, the fix is not straightforward because it's not easy to always determine when the current line is the first in the page.
        // With no valid page height, we can't possibly accommodate the widow rules.
        clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
        return { };
    }

    if (hasUniformPageLogicalHeight && logicalOverflowHeight > pageLogicalHeight) {
        // We are so tall that we are bigger than a page. Before we give up and just leave the line where it is, try drilling into the
        // line and computing a new height that excludes anything we consider "blank space". We will discard margins, descent, and even overflow. If we are
        // able to fit with the blank space and overflow excluded, we will give the line its own page with the highest non-blank element being aligned with the
        // top of the page.
        // FIXME: We are still honoring gigantic margins, which does leave open the possibility of blank pages caused by this heuristic. It remains to be seen whether or not
        // this will be a real-world issue. For now we don't try to deal with this problem.
        std::tie(logicalOffset, logicalBottom) = computeLeafBoxTopAndBottom();
        lineHeight = logicalBottom - logicalOffset;
        if (logicalOffset == LayoutUnit::max() || lineHeight > pageLogicalHeight) {
            // Give up. We're genuinely too big even after excluding blank space and overflow.
            clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
            return { };
        }
        pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
    }
    
    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(logicalOffset, ExcludePageBoundary);

    int lineNumber = lineBox->lineIndex() + 1;
    if (remainingLogicalHeight < lineHeight || (shouldBreakAtLineToAvoidWidow() && lineBreakToAvoidWidow() == lineNumber)) {
        if (lineBreakToAvoidWidow() == lineNumber)
            clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
        // If we have a non-uniform page height, then we have to shift further possibly.
        if (!hasUniformPageLogicalHeight && !pushToNextPageWithMinimumLogicalHeight(remainingLogicalHeight, logicalOffset, lineHeight))
            return { };
        if (lineHeight > pageLogicalHeight) {
            // Split the top margin in order to avoid splitting the visible part of the line.
            remainingLogicalHeight -= std::min(lineHeight - pageLogicalHeight, std::max(0_lu, logicalOverflowTop - logicalTop));
        }
        LayoutUnit totalLogicalHeight = lineHeight + std::max<LayoutUnit>(0, logicalOffset);
        LayoutUnit pageLogicalHeightAtNewOffset = hasUniformPageLogicalHeight ? pageLogicalHeight : pageLogicalHeightForOffset(logicalOffset + remainingLogicalHeight);

        setPageBreak(logicalOffset, lineHeight - remainingLogicalHeight);

        bool avoidFirstLinePageBreak = lineBox->isFirst() && totalLogicalHeight < pageLogicalHeightAtNewOffset;
        bool affectedByOrphans = !style().hasAutoOrphans() && style().orphans() >= lineNumber;

        if ((avoidFirstLinePageBreak || affectedByOrphans) && !isOutOfFlowPositioned() && !isTableCell()) {
            if (needsAppleMailPaginationQuirk(*this))
                return { };

            auto firstLineBox = InlineIterator::firstLineBoxFor(*this);
            auto firstLineBoxOverflowTop = LayoutUnit { firstLineBox ? firstLineBox->inkOverflowTop() : 0 };
            auto firstLineUpperOverhang = std::max(-firstLineBoxOverflowTop, 0_lu);
            setPaginationStrut(remainingLogicalHeight + logicalOffset + firstLineUpperOverhang);

            return { };
        }

        return { remainingLogicalHeight, true };
    }

    if (remainingLogicalHeight == pageLogicalHeight) {
        // We're at the very top of a page or column.
        bool isFirstLine = lineBox->isFirst();
        if (!isFirstLine || offsetFromLogicalTopOfFirstPage())
            setPageBreak(logicalOffset, lineHeight);

        return { 0_lu, !isFirstLine };
    }

    return { };
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
    auto* fragmentedFlow = enclosingFragmentedFlow();
    RenderFragmentContainer* currentFragmentContainer = nullptr;
    for (auto pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment); pageLogicalHeight; pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset + adjustment)) {
        if (minimumLogicalHeight <= pageLogicalHeight)
            return true;
        auto adjustedOffset = logicalOffset + adjustment;
        if (!hasNextPage(adjustedOffset))
            return false;
        if (fragmentedFlow) {
            // While in layout and the columnsets are not balanced yet, we keep finding the same (infinite tall) column over and over again.
            auto* nextFragmentContainer = fragmentedFlow->fragmentAtBlockOffset(this, adjustedOffset, true);
            ASSERT(nextFragmentContainer);
            if (nextFragmentContainer == currentFragmentContainer)
                return false;
            currentFragmentContainer = nextFragmentContainer;
        }
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

void RenderBlockFlow::adjustSizeContainmentChildForPagination(RenderBox& child, LayoutUnit offset)
{
    if (!child.shouldApplySizeContainment())
        return;

    LayoutUnit childOverflowHeight = child.isHorizontalWritingMode() ? child.layoutOverflowRect().maxY() : child.layoutOverflowRect().maxX();
    LayoutUnit childLogicalHeight = std::max(child.logicalHeight(), childOverflowHeight);

    LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(offset, ExcludePageBoundary);

    LayoutUnit spaceShortage = childLogicalHeight - remainingLogicalHeight;
    if (spaceShortage <= 0)
        return;

    if (RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->updateSpaceShortageForSizeContainment(this, offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void RenderBlockFlow::layoutLineGridBox()
{
    if (style().lineGrid() == RenderStyle::initialLineGrid()) {
        setLineGridBox(0);
        return;
    }
    
    setLineGridBox(0);

    auto lineGridBox = makeUnique<LegacyRootInlineBox>(*this);
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
    return m_floatingObjects && m_floatingObjects->set().contains<FloatingObjectHashTranslator>(renderer);
}

bool RenderBlockFlow::subtreeContainsFloat(RenderBox& renderer) const
{
    if (containsFloat(renderer))
        return true;

    for (auto& block : descendantsOfType<RenderBlock>(const_cast<RenderBlockFlow&>(*this))) {
        if (!is<RenderBlockFlow>(block))
            continue;
        auto& blockFlow = downcast<RenderBlockFlow>(block);
        if (blockFlow.containsFloat(renderer))
            return true;
    }

    return false;
}

bool RenderBlockFlow::subtreeContainsFloats() const
{
    if (containsFloats())
        return true;

    for (auto& block : descendantsOfType<RenderBlock>(const_cast<RenderBlockFlow&>(*this))) {
        if (!is<RenderBlockFlow>(block))
            continue;
        auto& blockFlow = downcast<RenderBlockFlow>(block);
        if (blockFlow.containsFloats())
            return true;
    }

    return false;
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
        auto shouldInvalidateLineLayoutPath = [&] {
            if (selfNeedsLayout() || legacyLineLayout())
                return true;
            if (modernLineLayout() && !LayoutIntegration::LineLayout::canUseForAfterStyleChange(*this, diff))
                return true;
            return false;
        };
        if (shouldInvalidateLineLayoutPath())
            invalidateLineLayoutPath();
    }

    if (auto* lineLayout = modernLineLayout())
        lineLayout->updateStyle(*this, *oldStyle);

    if (multiColumnFlow())
        updateStylesForColumnChildren(oldStyle);
}

void RenderBlockFlow::updateStylesForColumnChildren(const RenderStyle* oldStyle)
{
    auto columnsNeedLayout = oldStyle && (oldStyle->columnCount() != style().columnCount() || oldStyle->columnWidth() != style().columnWidth()); 
    for (auto* child = firstChildBox(); child && (child->isRenderFragmentedFlow() || child->isRenderMultiColumnSet()); child = child->nextSiblingBox()) {
        child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), DisplayType::Block));
        if (columnsNeedLayout)
            child->setNeedsLayoutAndPrefWidthsRecalc();
    }
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
    m_lineLayout = std::monostate();

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
            && (floatingObject.paintsFloat() || (paintAllDescendants && renderer.isDescendantOf(this)))) {
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
        LayoutPoint childPoint = columnSet.location() + flipForWritingModeForChild(columnSet, point);
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
        if (floatingObject.shouldPaint()) {
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
    m_floatingObjects = makeUnique<FloatingObjects>(*this);
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
        auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
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
    } else {
        floatBox.updateLogicalWidth();
        floatBox.computeAndSetBlockDirectionMargins(*this);
    }

    setLogicalWidthForFloat(*floatingObject, logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));

    return m_floatingObjects->add(WTFMove(floatingObject));
}

FloatingObject& RenderBlockFlow::insertFloatingObjectForIFC(RenderBox& floatBox)
{
    ASSERT(floatBox.isFloating());

    if (!m_floatingObjects)
        createFloatingObjects();

    auto& floatingObjectSet = m_floatingObjects->set();
    auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
    if (it != floatingObjectSet.end())
        return *it->get();

    return *m_floatingObjects->add(FloatingObject::create(floatBox));
}

void RenderBlockFlow::removeFloatingObject(RenderBox& floatBox)
{
    if (m_floatingObjects) {
        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
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
#if ASSERT_ENABLED
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

void RenderBlockFlow::trimMarginForFloat(FloatingObject& floatingObject, MarginTrimType marginTrimType, LayoutUnit trimAmount)
{
    ASSERT(!floatingObject.isPlaced() || marginTrimType == MarginTrimType::BlockEnd);
    switch (marginTrimType) {
    case MarginTrimType::InlineStart: {
        setLogicalWidthForFloat(floatingObject, logicalWidthForFloat(floatingObject) - floatingObject.renderer().marginStart(&style()));
        floatingObject.renderer().setMarginStart(0_lu, &style());
        break; 
    }
    case MarginTrimType::InlineEnd: {
        setLogicalWidthForFloat(floatingObject, logicalWidthForFloat(floatingObject) - floatingObject.renderer().marginEnd(&style()));
        floatingObject.renderer().setMarginEnd(0_lu, &style());
        break;
    }
    case MarginTrimType::BlockStart: {
        setLogicalHeightForFloat(floatingObject, logicalHeightForFloat(floatingObject) - floatingObject.renderer().marginBefore(&style()));
        floatingObject.renderer().setMarginBefore(0_lu, &style());
        break;
    }
    case MarginTrimType::BlockEnd: {
        ASSERT(trimAmount <= floatingObject.renderer().marginAfter(&style()) && trimAmount);
        setLogicalHeightForFloat(floatingObject, logicalHeightForFloat(floatingObject) - trimAmount);
        break;
    }
    }
}

void RenderBlockFlow::computeLogicalLocationForFloat(FloatingObject& floatingObject, LayoutUnit& logicalTopOffset)
{
    auto& childBox = floatingObject.renderer();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
    LayoutUnit logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.

    LayoutUnit floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    auto containingBlockMarginTrim = style().marginTrim();
    auto floatWidthWithoutMargin = [&](FloatingObject& floatingObject, MarginTrimType marginSide) {
        auto& containingBlockStyle = style();
        switch (marginSide) {
        case MarginTrimType::InlineStart:
            return logicalWidthForFloat(floatingObject) - floatingObject.renderer().marginStart(&containingBlockStyle);
        case MarginTrimType::InlineEnd:
            return logicalWidthForFloat(floatingObject) - floatingObject.renderer().marginEnd(&containingBlockStyle);
        default: {
            ASSERT_NOT_REACHED();
            return logicalWidthForFloat(floatingObject);
        }
        }
    };
    bool insideFragmentedFlow = enclosingFragmentedFlow();
    bool isInitialLetter = childBox.style().styleType() == PseudoId::FirstLetter && childBox.style().initialLetterDrop() > 0;
    
    if (isInitialLetter) {
        if (auto lowestInitialLetterLogicalBottom = this->lowestInitialLetterLogicalBottom()) {
            auto letterClearance =  *lowestInitialLetterLogicalBottom - logicalTopOffset;
            if (letterClearance > 0) {
                logicalTopOffset += letterClearance;
                setLogicalHeight(logicalHeight() + letterClearance);
            }
        }
    }

    // For trimming the margins of floats:
    // It also affects floats for which the block container is a containing block by:
    // Discarding the inline-start/inline-end margin of an inline-start/inline-end float (or equivalent) 
    // whose outer edge on that side coincides with the inner edge of the container.
    auto floatMarginAdjoinsContainerInnerEdge = [&](MarginTrimType marginTrimType, LayoutUnit floatLogicalPosition, LayoutUnit logicalOffset) {
        return containingBlockMarginTrim.contains(marginTrimType) && floatLogicalPosition == logicalOffset;
    };
    if (RenderStyle::usedFloat(childBox) == UsedFloat::Left) {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        auto availableSpace = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight) - floatLogicalLeft;
        while (availableSpace < floatLogicalWidth) {
            if (floatMarginAdjoinsContainerInnerEdge(MarginTrimType::InlineStart, floatLogicalLeft, logicalLeftOffset) && availableSpace >= floatWidthWithoutMargin(floatingObject, MarginTrimType::InlineStart))
                break;
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
            if (insideFragmentedFlow) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
            availableSpace = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight) - floatLogicalLeft;
        }
        floatLogicalLeft = std::max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
        
        if (containingBlockMarginTrim.contains(MarginTrimType::InlineStart) && logicalLeftOffset == floatLogicalLeft)
            trimMarginForFloat(floatingObject, MarginTrimType::InlineStart);
    } else {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
        auto availableSpace = floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        while (availableSpace < floatLogicalWidth) {
            if (floatMarginAdjoinsContainerInnerEdge(MarginTrimType::InlineEnd, floatLogicalLeft, logicalRightOffset) && (availableSpace >= floatWidthWithoutMargin(floatingObject, MarginTrimType::InlineEnd)))
                break;
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, false, &heightRemainingRight);
            if (insideFragmentedFlow) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(logicalTopOffset); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(logicalTopOffset); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
            availableSpace = floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, false, &heightRemainingLeft);
        }
        // We trim before we update floatLogicalLeft becaue it currently represents the logical
        // right for the float. We would have to otherwise update floatLogicalLeft each time we
        // trim a margin by adding the trimmed margin to its value
        if (containingBlockMarginTrim.contains(MarginTrimType::InlineEnd) && logicalRightOffset == floatLogicalLeft)
            trimMarginForFloat(floatingObject, MarginTrimType::InlineEnd);    
        // Use the original width of the float here, since the local variable
        // |floatLogicalWidth| was capped to the available line width. See
        // fast/block/float/clamped-right-float.html.
        floatLogicalLeft -= logicalWidthForFloat(floatingObject);
    }
    if (style().marginTrim().contains(MarginTrimType::BlockStart) && logicalTopOffset == borderAndPaddingBefore())
        trimMarginForFloat(floatingObject, MarginTrimType::BlockStart);

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
    const FontMetrics& fontMetrics = style.metricsOfPrimaryFont();
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
        auto childBoxUsedClear = RenderStyle::usedClear(childBox);
        if (childBoxUsedClear == UsedClear::Left || childBoxUsedClear == UsedClear::Both)
            logicalTop = std::max(lowestFloatLogicalBottom(FloatingObject::FloatLeft), logicalTop);
        if (childBoxUsedClear == UsedClear::Right || childBoxUsedClear == UsedClear::Both)
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
            auto* childBlock = dynamicDowncast<RenderBlock>(childBox);
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
            shapeOutside->invalidateForSizeChangeIfNeeded();
        // If the child moved, we have to repaint it.
        if (childBox.checkForRepaintDuringLayout())
            childBox.repaintDuringLayoutIfMoved(oldRect);
    }
    return true;
}

void RenderBlockFlow::clearFloats(UsedClear usedClear)
{
    positionNewFloats();
    // set y position
    LayoutUnit newY;
    switch (usedClear) {
    case UsedClear::Left:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case UsedClear::Right:
        newY = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case UsedClear::Both:
        newY = lowestFloatLogicalBottom();
        break;
    case UsedClear::None:
        break;
    }
    // FIXME: The float search tree has floored float box position (see FloatingObjects::intervalForFloatingObject).
    newY = newY.floor();
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

std::optional<LayoutUnit> RenderBlockFlow::lowestInitialLetterLogicalBottom() const
{
    if (!m_floatingObjects)
        return { };
    auto lowestFloatBottom = std::optional<LayoutUnit> { };
    auto& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        if (floatingObject.isPlaced() && floatingObject.renderer().style().styleType() == PseudoId::FirstLetter && floatingObject.renderer().style().initialLetterDrop() > 0)
            lowestFloatBottom = std::max(lowestFloatBottom.value_or(0_lu), logicalBottomForFloat(floatingObject));
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
    auto blockHasOverflowClip = effectiveOverflowX() == Overflow::Clip || effectiveOverflowY() == Overflow::Clip;
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
                if (!floatingObject.hasAncestorWithOverflowClip() && floatingObject.renderer().enclosingFloatPaintingLayer() == enclosingFloatPaintingLayer()) {
                    floatingObject.setPaintsFloat(false);
                    shouldPaint = true;
                }
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                m_floatingObjects->add(floatingObject.copyToNewContainer(offset, shouldPaint, true, floatingObject.hasAncestorWithOverflowClip() || blockHasOverflowClip));
            }
        } else {
            const auto& renderer = floatingObject.renderer();
            if (makeChildPaintOtherFloats && !floatingObject.paintsFloat() && !renderer.hasSelfPaintingLayer()
                && renderer.isDescendantOf(&child) && renderer.enclosingFloatPaintingLayer() == child.enclosingFloatPaintingLayer()) {
                // The float is not overhanging from this block, so if it is a descendant of the child, the child should
                // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
                // layer.
                // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
                // it should paint.
                floatingObject.setPaintsFloat(true);
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
    const auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(renderer);
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
            if (!m_floatingObjects || !m_floatingObjects->set().contains(&floatingObject)) {
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
        if ((floatToRemove ? blockFlow.subtreeContainsFloat(*floatToRemove) : blockFlow.subtreeContainsFloats()) || blockFlow.shrinkToAvoidFloats())
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
        if (!is<RenderBlockFlow>(*next) || (!floatToRemove && (next->isFloatingOrOutOfFlowPositioned() || downcast<RenderBlockFlow>(*next).avoidsFloats())))
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
    UsedClear usedClear = RenderStyle::usedClear(child);
    bool clearSet = usedClear != UsedClear::None;
    LayoutUnit logicalBottom;
    switch (usedClear) {
    case UsedClear::None:
        break;
    case UsedClear::Left:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatLeft);
        break;
    case UsedClear::Right:
        logicalBottom = lowestFloatLogicalBottom(FloatingObject::FloatRight);
        break;
    case UsedClear::Both:
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
        if (floatingObject.shouldPaint()) {
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

    if (modernLineLayout())
        return modernLineLayout()->hitTest(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    return legacyLineLayout() && legacyLineLayout()->lineBoxes().hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

void RenderBlockFlow::addOverflowFromInlineChildren()
{
    if (modernLineLayout()) {
        modernLineLayout()->collectOverflow();
        return;
    }
    
    if (legacyLineLayout())
        legacyLineLayout()->addOverflowFromInlineChildren();
}

void RenderBlockFlow::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, LegacyRootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    // Floats currently affect the choice of layout path.
    if (modernLineLayout()) {
        invalidateLineLayoutPath();
        return;
    }

    LegacyRootInlineBox* lowestDirtyLine = lastRootBox();
    LegacyRootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->lineBoxBottom() >= logicalBottom && logicalBottom < LayoutUnit::max()) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest != highest && (afterLowest->lineBoxBottom() >= logicalTop || afterLowest->lineBoxBottom() < 0)) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

std::optional<LayoutUnit> RenderBlockFlow::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun() && !isGridItem() && !isFlexItem())
        return std::nullopt;

    if (shouldApplyLayoutContainment())
        return std::nullopt;

    if (!childrenInline())
        return RenderBlock::firstLineBaseline();

    if (!hasLines())
        return std::nullopt;

    if (modernLineLayout())
        return LayoutUnit { floorToInt(modernLineLayout()->firstLinePhysicalBaseline()) };

    ASSERT(firstRootBox());
    if (style().isFlippedLinesWritingMode())
        return LayoutUnit { firstRootBox()->logicalTop() + firstLineStyle().metricsOfPrimaryFont().descent(firstRootBox()->baselineType()) };
    return LayoutUnit { firstRootBox()->logicalTop() + firstLineStyle().metricsOfPrimaryFont().ascent(firstRootBox()->baselineType()) };
}

std::optional<LayoutUnit> RenderBlockFlow::lastLineBaseline() const
{
    if (isWritingModeRoot() && !isRubyRun() && !isGridItem() && !isFlexItem())
        return std::nullopt;

    if (shouldApplyLayoutContainment())
        return std::nullopt;

    if (!childrenInline())
        return RenderBlock::lastLineBaseline();

    if (!hasLines())
        return std::nullopt;

    if (auto* lineLayout = modernLineLayout())
        return LayoutUnit { floorToInt(lineLayout->lastLinePhysicalBaseline()) };

    ASSERT(lastRootBox()); 
    auto rootBox = lastRootBox();
    const RenderStyle& lastLineBoxStyle = InlineIterator::lastLineBoxFor(*this)->style();
    if (style().isFlippedLinesWritingMode())
        return LayoutUnit { rootBox->logicalTop() + lastLineBoxStyle.metricsOfPrimaryFont().descent(rootBox->baselineType()) };
    return LayoutUnit { rootBox->logicalTop() + lastLineBoxStyle.metricsOfPrimaryFont().ascent(rootBox->baselineType()) };
}

std::optional<LayoutUnit> RenderBlockFlow::inlineBlockBaseline(LineDirectionMode lineDirection) const
{
    if (isWritingModeRoot() && !isRubyRun())
        return std::nullopt;

    if (shouldApplyLayoutContainment())
        return RenderBlock::inlineBlockBaseline(lineDirection);

    if (style().display() == DisplayType::InlineBlock) {
        // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or if its 'overflow'
        // property has a computed value other than 'visible'. see https://www.w3.org/TR/CSS22/visudet.html
        auto shouldSynthesizeBaseline = !style().isOverflowVisible() && !is<HTMLFormControlElement>(element());
        if (shouldSynthesizeBaseline)
            return std::nullopt;
    }
    // Note that here we only take the left and bottom into consideration. Our caller takes the right and top into consideration.
    auto boxHeight = synthesizedBaseline(*this, *parentStyle(), lineDirection, BorderBox) + (lineDirection == HorizontalLine ? m_marginBox.bottom() : m_marginBox.left());
    LayoutUnit lastBaseline;
    if (!childrenInline()) {
        auto inlineBlockBaseline = RenderBlock::inlineBlockBaseline(lineDirection);
        if (!inlineBlockBaseline)
            return inlineBlockBaseline;
        lastBaseline = inlineBlockBaseline.value();
    } else {
        if (!hasLines()) {
            if (!hasLineIfEmpty())
                return std::nullopt;
            const auto& fontMetrics = firstLineStyle().metricsOfPrimaryFont();
            return LayoutUnit { LayoutUnit(fontMetrics.ascent()
                + (lineHeight(true, lineDirection, PositionOfInteriorLineBoxes) - fontMetrics.height()) / 2
                + (lineDirection == HorizontalLine ? borderTop() + paddingTop() : borderRight() + paddingRight())).toInt() };
        }

        if (legacyLineLayout()) {
            bool isFirstLine = lastRootBox() == firstRootBox();
            const auto& style = isFirstLine ? firstLineStyle() : this->style();
            // LegacyInlineFlowBox::placeBoxesInBlockDirection will flip lines in case of verticalLR mode, so we can assume verticalRL for now.
            lastBaseline = style.metricsOfPrimaryFont().ascent(lastRootBox()->baselineType())
                + (style.isFlippedLinesWritingMode() ? logicalHeight() - lastRootBox()->logicalBottom() : lastRootBox()->logicalTop());
        }
        else if (modernLineLayout())
            lastBaseline = floorToInt(modernLineLayout()->lastLineLogicalBaseline());
    }
    // According to the CSS spec http://www.w3.org/TR/CSS21/visudet.html, we shouldn't be performing this min, but should
    // instead be returning boxHeight directly. However, we feel that a min here is better behavior (and is consistent
    // enough with the spec to not cause tons of breakages).
    return LayoutUnit { style().overflowY() == Overflow::Visible ? lastBaseline : std::min(boxHeight, lastBaseline) };
}

LayoutUnit RenderBlockFlow::adjustEnclosingTopForPrecedingBlock(LayoutUnit top) const
{
    if (selectionState() != RenderObject::HighlightState::Inside && selectionState() != RenderObject::HighlightState::End)
        return top;

    if (isSelectionRoot())
        return top;

    LayoutSize offsetToBlockBefore;

    auto blockBeforeWithinSelectionRoot = [&]() -> const RenderBlockFlow* {
        const RenderElement* object = this;
        const RenderObject* sibling = nullptr;
        do {
            sibling = object->previousSibling();
            while (sibling && (!is<RenderBlock>(*sibling) || downcast<RenderBlock>(*sibling).isSelectionRoot()))
                sibling = sibling->previousSibling();

            offsetToBlockBefore -= LayoutSize(downcast<RenderBlock>(*object).logicalLeft(), downcast<RenderBlock>(*object).logicalTop());
            object = object->parent();
        } while (!sibling && is<RenderBlock>(object) && !downcast<RenderBlock>(*object).isSelectionRoot());

        if (!sibling)
            return nullptr;

        auto* beforeBlock = downcast<RenderBlock>(sibling);

        offsetToBlockBefore += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

        auto* child = beforeBlock->lastChild();
        while (is<RenderBlock>(child)) {
            beforeBlock = downcast<RenderBlock>(child);
            offsetToBlockBefore += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
            child = beforeBlock->lastChild();
        }
        return is<RenderBlockFlow>(beforeBlock) ? downcast<RenderBlockFlow>(beforeBlock) : nullptr;
    };

    auto* blockBefore = blockBeforeWithinSelectionRoot();
    if (!blockBefore)
        return top;

    // Do not adjust blocks sharing the same line.
    if (!offsetToBlockBefore.height())
        return top;

    if (auto lastLineBox = InlineIterator::lastLineBoxFor(*blockBefore)) {
        auto lastLineSelectionState = LineSelection::selectionState(*lastLineBox);
        if (lastLineSelectionState != RenderObject::HighlightState::Inside && lastLineSelectionState != RenderObject::HighlightState::Start)
            return top;

        auto lastLineSelectionBottom = LineSelection::logicalBottom(*lastLineBox) + offsetToBlockBefore.height();
        top = std::max(top, LayoutUnit { lastLineSelectionBottom });
    }
    return top;
}

GapRects RenderBlockFlow::inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    bool containsStart = selectionState() == HighlightState::Start || selectionState() == HighlightState::Both;

    if (!hasLines()) {
        if (containsStart) {
            // Update our lastLogicalTop to be the bottom of the block. <hr>s or empty blocks with height can trip this case.
            lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight();
            lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache);
            lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight(), cache);
        }
        return { };
    }

    auto hasSelectedChildren = [&](const InlineIterator::LineBoxIterator& lineBox) {
        return LineSelection::selectionState(*lineBox) != RenderObject::HighlightState::None;
    };

    auto lineSelectionGap = [&](const InlineIterator::LineBoxIterator& lineBox, LayoutUnit selTop, LayoutUnit selHeight) -> GapRects {
        auto lineState = LineSelection::selectionState(*lineBox);

        bool leftGap, rightGap;
        getSelectionGapInfo(lineState, leftGap, rightGap);

        GapRects result;

        auto firstSelectedBox = [&]() -> InlineIterator::LeafBoxIterator {
            for (auto box = lineBox->firstLeafBox(); box; box.traverseNextOnLine()) {
                if (box->selectionState() != RenderObject::HighlightState::None)
                    return box;
            }
            return { };
        }();

        auto lastSelectedBox = [&]() -> InlineIterator::LeafBoxIterator {
            for (auto box = lineBox->lastLeafBox(); box; box.traversePreviousOnLine()) {
                if (box->selectionState() != RenderObject::HighlightState::None)
                    return box;
            }
            return { };
        }();

        if (leftGap) {
            result.uniteLeft(logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, firstSelectedBox->renderer().parent(), LayoutUnit(firstSelectedBox->logicalLeftIgnoringInlineDirection()),
                selTop, selHeight, cache, paintInfo));
        }
        if (rightGap) {
            result.uniteRight(logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastSelectedBox->renderer().parent(), LayoutUnit(lastSelectedBox->logicalRightIgnoringInlineDirection()),
                selTop, selHeight, cache, paintInfo));
        }

        // When dealing with bidi text, a non-contiguous selection region is possible.
        // e.g. The logical text aaaAAAbbb (capitals denote RTL text and non-capitals LTR) is layed out
        // visually as 3 text runs |aaa|bbb|AAA| if we select 4 characters from the start of the text the
        // selection will look like (underline denotes selection):
        // |aaa|bbb|AAA|
        //  ___       _
        // We can see that the |bbb| run is not part of the selection while the runs around it are.
        if (firstSelectedBox && firstSelectedBox != lastSelectedBox) {
            // Now fill in any gaps on the line that occurred between two selected elements.
            LayoutUnit lastLogicalLeft { firstSelectedBox->logicalRightIgnoringInlineDirection() };
            bool isPreviousBoxSelected = firstSelectedBox->selectionState() != RenderObject::HighlightState::None;
            for (auto box = firstSelectedBox; box; box.traverseNextOnLine()) {
                if (box->selectionState() != RenderObject::HighlightState::None) {
                    LayoutRect logicalRect { lastLogicalLeft, selTop, LayoutUnit(box->logicalLeftIgnoringInlineDirection() - lastLogicalLeft), selHeight };
                    logicalRect.move(isHorizontalWritingMode() ? offsetFromRootBlock : LayoutSize(offsetFromRootBlock.height(), offsetFromRootBlock.width()));
                    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
                    if (isPreviousBoxSelected && gapRect.width() > 0 && gapRect.height() > 0) {
                        if (paintInfo && box->renderer().parent()->style().visibility() == Visibility::Visible)
                            paintInfo->context().fillRect(gapRect, box->renderer().parent()->selectionBackgroundColor());
                        // VisibleSelection may be non-contiguous, see comment above.
                        result.uniteCenter(gapRect);
                    }
                    lastLogicalLeft = box->logicalRightIgnoringInlineDirection();
                }
                if (box == lastSelectedBox)
                    break;
                isPreviousBoxSelected = box->selectionState() != RenderObject::HighlightState::None;
            }
        }

        return result;
    };

    InlineIterator::LineBoxIterator lastSelectedLineBox;
    auto lineBox = InlineIterator::firstLineBoxFor(*this);
    for (; lineBox && !hasSelectedChildren(lineBox); lineBox.traverseNext()) { }

    GapRects result;

    // Now paint the gaps for the lines.
    for (; lineBox && hasSelectedChildren(lineBox); lineBox.traverseNext()) {
        auto selectionTop =  LayoutUnit { LineSelection::logicalTopAdjustedForPrecedingBlock(*lineBox) };
        auto selectionHeight = LayoutUnit { std::max(0.f, LineSelection::logicalBottom(*lineBox) - selectionTop) };

        if (!containsStart && !lastSelectedLineBox
            && selectionState() != HighlightState::Start
            && selectionState() != HighlightState::Both && !isRubyBase())
            result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, selectionTop, cache, paintInfo));

        LayoutRect logicalRect { LayoutUnit(lineBox->contentLogicalLeft()), selectionTop, LayoutUnit(lineBox->contentLogicalWidth()), selectionTop + selectionHeight };
        logicalRect.move(isHorizontalWritingMode() ? offsetFromRootBlock : offsetFromRootBlock.transposedSize());
        LayoutRect physicalRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
        if (!paintInfo || (isHorizontalWritingMode() && physicalRect.y() < paintInfo->rect.maxY() && physicalRect.maxY() > paintInfo->rect.y())
            || (!isHorizontalWritingMode() && physicalRect.x() < paintInfo->rect.maxX() && physicalRect.maxX() > paintInfo->rect.x()))
            result.unite(lineSelectionGap(lineBox, selectionTop, selectionHeight));

        lastSelectedLineBox = lineBox;
    }

    if (containsStart && !lastSelectedLineBox) {
        // VisibleSelection must start just after our last line.
        lastSelectedLineBox = InlineIterator::lastLineBoxFor(*this);
    }

    if (lastSelectedLineBox && selectionState() != HighlightState::End && selectionState() != HighlightState::Both) {
        // Update our lastY to be the bottom of the last selected line.
        auto lastLineSelectionBottom = LayoutUnit { LineSelection::logicalBottom(*lastSelectedLineBox) };
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + lastLineSelectionBottom;
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, lastLineSelectionBottom, cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, lastLineSelectionBottom, cache);
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
    ensureRareBlockFlowData().m_multiColumnFlow = fragmentedFlow;
}

void RenderBlockFlow::clearMultiColumnFlow()
{
    ASSERT(hasRareBlockFlowData());
    ASSERT(rareBlockFlowData()->m_multiColumnFlow);
    rareBlockFlowData()->m_multiColumnFlow.clear();
}

int RenderBlockFlow::lineCount() const
{
    if (!childrenInline()) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (modernLineLayout())
        return modernLineLayout()->lineCount();
    if (legacyLineLayout())
        return legacyLineLayout()->lineCount();

    return 0;
}

bool RenderBlockFlow::containsNonZeroBidiLevel() const
{
    for (auto lineBox = InlineIterator::firstLineBoxFor(*this); lineBox; lineBox.traverseNext()) {
        for (auto box = lineBox->firstLeafBox(); box; box = box.traverseNextOnLine()) {
            if (box->bidiLevel())
                return true;
        }
    }
    return false;
}

static Position positionForRun(const RenderBlockFlow& flow, InlineIterator::BoxIterator box, bool start)
{
    if (!box)
        return Position();

    if (!box->renderer().nonPseudoNode())
        return makeDeprecatedLegacyPosition(flow.nonPseudoElement(), start ? flow.caretMinOffset() : flow.caretMaxOffset());

    if (!is<InlineIterator::TextBoxIterator>(box))
        return makeDeprecatedLegacyPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    auto& textBox = downcast<InlineIterator::TextBoxIterator>(box);
    return makeDeprecatedLegacyPosition(textBox->renderer().nonPseudoNode(), start ? textBox->start() : textBox->end());
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
    auto previousRootInlineBoxBottom = std::optional<float> { };
    for (auto box = InlineIterator::firstRootInlineBoxFor(blockFlow); box; box.traverseNextInlineBox()) {
        if (previousRootInlineBoxBottom) {
            if (localPoint.y() < *previousRootInlineBoxBottom)
                return nullptr;

            if (localPoint.y() > *previousRootInlineBoxBottom && localPoint.y() < box->logicalTop()) {
                auto closestBox = closestBoxForHorizontalPosition(*box->lineBox(), localPoint.x());
                if (closestBox && is<RenderText>(closestBox->renderer()))
                    return const_cast<RenderText*>(&downcast<RenderText>(closestBox->renderer()));
            }
        }
        previousRootInlineBoxBottom = box->logicalBottom();
    }
    return nullptr;
}

VisiblePosition RenderBlockFlow::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents, const RenderFragmentContainer* fragment)
{
    ASSERT(childrenInline());

    auto firstLineBox = InlineIterator::firstLineBoxFor(*this);

    if (!firstLineBox)
        return createVisiblePosition(0, Affinity::Downstream);

    bool linesAreFlipped = style().isFlippedLinesWritingMode();
    bool blocksAreFlipped = style().isFlippedBlocksWritingMode();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineIterator::LeafBoxIterator closestBox;
    InlineIterator::LineBoxIterator firstLineBoxWithChildren;
    InlineIterator::LineBoxIterator lastLineBoxWithChildren;
    for (auto lineBox = firstLineBox; lineBox; lineBox.traverseNext()) {
        if (fragment && lineBox->containingFragment() != fragment)
            continue;

        if (!lineBox->firstLeafBox())
            continue;
        if (!firstLineBoxWithChildren)
            firstLineBoxWithChildren = lineBox;

        if (!linesAreFlipped && lineBox->isFirstAfterPageBreak()
            && (pointInLogicalContents.y() < lineBox->logicalTop() || (blocksAreFlipped && pointInLogicalContents.y() == lineBox->logicalTop())))
            break;

        lastLineBoxWithChildren = lineBox;

        // check if this root line box is located at this y coordinate
        auto selectionBottom = LineSelection::logicalBottom(*lineBox);
        if (pointInLogicalContents.y() < selectionBottom || (blocksAreFlipped && pointInLogicalContents.y() == selectionBottom)) {
            if (linesAreFlipped) {
                auto nextLineBoxWithChildren = lineBox->next();
                while (nextLineBoxWithChildren && !nextLineBoxWithChildren->firstLeafBox())
                    nextLineBoxWithChildren.traverseNext();

                if (nextLineBoxWithChildren && nextLineBoxWithChildren->isFirstAfterPageBreak()
                    && (pointInLogicalContents.y() > nextLineBoxWithChildren->logicalTop() || (!blocksAreFlipped && pointInLogicalContents.y() == nextLineBoxWithChildren->logicalTop())))
                    continue;
            }
            closestBox = closestBoxForHorizontalPosition(*lineBox, pointInLogicalContents.x());
            if (closestBox)
                break;
        }
    }

    bool moveCaretToBoundary = frame().editor().behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastLineBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = closestBoxForHorizontalPosition(*lastLineBoxWithChildren, pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            auto firstLineWithChildrenTop = LayoutUnit { std::min(previousLineBoxContentBottomOrBorderAndPadding(*firstLineBoxWithChildren), firstLineBoxWithChildren->contentLogicalTop()) };
            if (pointInLogicalContents.y() < firstLineWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstLineWithChildrenTop)) {
                auto box = firstLineBoxWithChildren->firstLeafBox();
                if (box->isLineBreak()) {
                    if (auto next = box->nextOnLineIgnoringLineBreak())
                        box = next;
                }
                // y coordinate is above first root line box, so return the start of the first
                return positionForRun(*this, box, true);
            }
        }

        // pass the box a top position that is inside it
        auto point = LayoutPoint { pointInLogicalContents.x(), contentStartInBlockDirection(*closestBox->lineBox()) };
        if (!isHorizontalWritingMode())
            point = point.transposedPoint();
        if (closestBox->renderer().isReplacedOrInlineBlock())
            return positionForPointRespectingEditingBoundaries(*this, const_cast<RenderBox&>(downcast<RenderBox>(closestBox->renderer())), point);
        return const_cast<RenderObject&>(closestBox->renderer()).positionForPoint(point, nullptr);
    }

    if (lastLineBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        ASSERT(moveCaretToBoundary);
        InlineIterator::LineLogicalOrderCache orderCache;
        if (auto logicallyLastBox = InlineIterator::lastLeafOnLineInLogicalOrderWithNode(lastLineBoxWithChildren, orderCache))
            return positionForRun(*this, logicallyLastBox, false);
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createVisiblePosition(0, Affinity::Downstream);
}

Position RenderBlockFlow::positionForPoint(const LayoutPoint& point)
{
    return positionForPoint(point, nullptr).deepEquivalent();
}

VisiblePosition RenderBlockFlow::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer*)
{
    return RenderBlock::positionForPoint(point, nullptr);
}

void RenderBlockFlow::addFocusRingRectsForInlineChildren(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*) const
{
    ASSERT(childrenInline());
    for (auto box = InlineIterator::firstRootInlineBoxFor(*this); box; box.traverseNextInlineBox()) {
        auto lineBox = box->lineBox();
        // FIXME: This is mixing physical and logical coordinates.
        auto unflippedVisualRect = box->visualRectIgnoringBlockDirection();
        auto top = std::max(lineBox->contentLogicalTop(), unflippedVisualRect.y());
        auto bottom = std::min(lineBox->contentLogicalBottom(), unflippedVisualRect.maxY());
        auto rect = LayoutRect { LayoutUnit { additionalOffset.x() + unflippedVisualRect.x() }
            , additionalOffset.y() + top
            , LayoutUnit { unflippedVisualRect.width() }
            , bottom - top };
        if (!rect.isEmpty())
            rects.append(rect);
    }
}

void RenderBlockFlow::paintInlineChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(childrenInline());

    if (modernLineLayout()) {
        modernLineLayout()->paint(paintInfo, paintOffset);
        return;
    }

    if (legacyLineLayout())
        legacyLineLayout()->lineBoxes().paint(this, paintInfo, paintOffset);
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
    return childrenInline() ? lineCount() : false;
}

void RenderBlockFlow::invalidateLineLayoutPath()
{
    switch (lineLayoutPath()) {
    case UndeterminedPath:
    case ForcedLegacyPath:
        return;
    case LegacyPath:
        setLineLayoutPath(UndeterminedPath);
        return;
    case ModernPath: {
        // FIXME: Implement partial invalidation.
        if (modernLineLayout()) {
            m_previousModernLineLayoutContentBoxLogicalHeight = modernLineLayout()->contentBoxLogicalHeight();
            // Since we eagerly remove the display content here, repaints issued between this invalidation (triggered by style change/content mutation) and the subsequent layout would produce empty rects.
            repaint();
            for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
                auto& renderer = *walker.current(); 
                if (!renderer.isInFlow())
                    renderer.repaint();
                renderer.setPreferredLogicalWidthsDirty(true);
            }
        }
        auto path = UndeterminedPath;
        if (modernLineLayout() && modernLineLayout()->shouldSwitchToLegacyOnInvalidation())
            path = ForcedLegacyPath;
        m_lineLayout = std::monostate();
        setLineLayoutPath(path);
        if (selfNeedsLayout() || normalChildNeedsLayout())
            return;
        // FIXME: We should just kick off a subtree layout here (if needed at all) see webkit.org/b/172947.
        setNeedsLayout();
        return;
    }
    }
    ASSERT_NOT_REACHED();
}

void RenderBlockFlow::layoutModernLines(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    bool needsUpdateReplacedDimensions = false;
    auto& layoutState = *view().frameView().layoutContext().layoutState();

    if (!modernLineLayout()) {
        m_lineLayout = makeUnique<LayoutIntegration::LineLayout>(*this);
        needsUpdateReplacedDimensions = true;
    }

    auto& layoutFormattingContextLineLayout = *this->modernLineLayout();
    layoutFormattingContextLineLayout.updateInlineContentConstraints();

    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();
        auto childNeedsLayout = relayoutChildren || (is<RenderBox>(renderer) && downcast<RenderBox>(renderer).hasRelativeDimensions());
        auto childNeedsPreferredWidthComputation = relayoutChildren && is<RenderBox>(renderer) && downcast<RenderBox>(renderer).needsPreferredWidthsRecalculation();
        if (childNeedsLayout)
            renderer.setNeedsLayout(MarkOnlyThis);
        if (childNeedsPreferredWidthComputation)
            renderer.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);

        if (renderer.isOutOfFlowPositioned())
            renderer.containingBlock()->insertPositionedObject(downcast<RenderBox>(renderer));

        if (!renderer.needsLayout() && !renderer.preferredLogicalWidthsDirty() && !needsUpdateReplacedDimensions)
            continue;

        auto shouldRunInFlowLayout = renderer.isInFlow() && is<RenderElement>(renderer) && !is<RenderLineBreak>(renderer) && !is<RenderInline>(renderer) && !is<RenderCounter>(renderer);
        if (shouldRunInFlowLayout || renderer.isFloating())
            downcast<RenderElement>(renderer).layoutIfNeeded();
        else if (!renderer.isOutOfFlowPositioned())
            renderer.clearNeedsLayout();

        if (is<RenderCombineText>(renderer)) {
            downcast<RenderCombineText>(renderer).combineTextIfNeeded();
            continue;
        }
    }

    layoutFormattingContextLineLayout.updateInlineContentDimensions();

    auto contentBoxTop = borderAndPaddingBefore();

    auto computeContentHeight = [&] {
        if (!hasLines() && hasLineIfEmpty()) {
            if (m_previousModernLineLayoutContentBoxLogicalHeight)
                return *m_previousModernLineLayoutContentBoxLogicalHeight;
            return lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
        }

        return layoutFormattingContextLineLayout.contentBoxLogicalHeight();
    };

    auto computeBorderBoxBottom = [&] {
        auto contentBoxBottom = contentBoxTop + computeContentHeight();
        return contentBoxBottom + borderAndPaddingAfter() + scrollbarLogicalHeight();
    };

    auto oldBorderBoxBottom = computeBorderBoxBottom();
    m_previousModernLineLayoutContentBoxLogicalHeight = { };

    auto partialRepaintRect = layoutFormattingContextLineLayout.layout();

    auto newBorderBoxBottom = computeBorderBoxBottom();

    auto updateRepaintTopAndBottomIfNeeded = [&] {
        auto isFullLayout = selfNeedsLayout() || relayoutChildren;
        if (isFullLayout) {
            if (!selfNeedsLayout()) {
                // In order to really trigger full repaint, the block container has to have the self layout flag set (see LegacyLineLayout::layoutRunsAndFloats).
                // Without having it set, repaint after layout logic (see RenderElement::repaintAfterLayoutIfNeeded) only issues repaint on the diff of
                // before/after repaint bounds. It results in incorrect repaint when the inline content changes (new text) and expands the same time.
                // (it only affects shrink-to-fit type of containers).
                // FIXME: We have the exact damaged rect here, should be able to issue repaint on both inline and block directions.
                setNeedsLayout(MarkOnlyThis);
            }
            // Let's trigger full repaint instead for now (matching legacy line layout).
            // FIXME: We should revisit this behavior and run repaints strictly on visual overflow.
            repaintLogicalTop = { };
            repaintLogicalBottom = { };
            return;
        }

        if (partialRepaintRect) {
            repaintLogicalTop = partialRepaintRect->y();
            repaintLogicalBottom = partialRepaintRect->maxY();
            return;
        }

        auto firstLineBox = InlineIterator::firstLineBoxFor(*this);
        auto lastLineBox = InlineIterator::lastLineBoxFor(*this);
        if (!firstLineBox)
            return;

        repaintLogicalTop = std::min(contentBoxTop, LayoutUnit { firstLineBox->contentLogicalTop() });
        repaintLogicalBottom = std::max(std::max(oldBorderBoxBottom, newBorderBoxBottom), LayoutUnit { lastLineBox->contentLogicalBottom() });
        if (layoutFormattingContextLineLayout.hasVisualOverflow()) {
            for (auto lineBox = firstLineBox; lineBox; lineBox.traverseNext()) {
                repaintLogicalTop = std::min(repaintLogicalTop, LayoutUnit { lineBox->inkOverflowTop() });
                repaintLogicalBottom = std::max(repaintLogicalBottom, LayoutUnit { lineBox->inkOverflowBottom() });
            }
        }
    };
    updateRepaintTopAndBottomIfNeeded();

    setLogicalHeight(newBorderBoxBottom);
    auto updateLineClampStateAndLogicalHeightIfApplicable = [&] {
        auto lineClamp = layoutState.lineClamp();
        if (!lineClamp)
            return;
        lineClamp->currentLineCount += layoutFormattingContextLineLayout.lineCount();
        if (lineClamp->clampedRenderer) {
            // We've already clamped this flex container at a previous flex item.
            layoutState.setLineClamp(*lineClamp);
            return;
        }
        auto clampedContentHeight = [&]() -> std::optional<LayoutUnit> {
            if (auto clampedHeight = layoutFormattingContextLineLayout.clampedContentLogicalHeight())
                return clampedHeight;
            if (lineClamp->currentLineCount == lineClamp->maximumLineCount) {
                // Even if we did not truncate the content, this might be our clamping position.
                return computeContentHeight();
            }
            return { };
        };
        if (auto logicalHeight = clampedContentHeight()) {
            lineClamp->clampedContentLogicalHeight = logicalHeight;
            lineClamp->clampedRenderer = this;
            setLogicalHeight(borderAndPaddingBefore() + *logicalHeight + borderAndPaddingAfter() + scrollbarLogicalHeight());
        }
        layoutState.setLineClamp(*lineClamp);
    };
    updateLineClampStateAndLogicalHeightIfApplicable();
}

#if ENABLE(TREE_DEBUGGING)
void RenderBlockFlow::outputFloatingObjects(WTF::TextStream& stream, int depth) const
{
    if (!floatingObjectSet())
        return;

    for (auto& floatingObject : *floatingObjectSet()) {
        int printedCharacters = 0;
        while (++printedCharacters <= depth * 2)
            stream << " ";

        stream << "             ";
        stream << "floating object " << *floatingObject;
        stream.nextLine();
    }
}

void RenderBlockFlow::outputLineTreeAndMark(WTF::TextStream& stream, const LegacyInlineBox* markedBox, int depth) const
{
    if (auto* modernLineLayout = this->modernLineLayout()) {
        modernLineLayout->outputLineTree(stream, depth);
        return;
    }
    for (const LegacyRootInlineBox* root = firstRootBox(); root; root = root->nextRootBox())
        root->outputLineTreeAndMark(stream, markedBox, depth);
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
    m_rareBlockFlowData = makeUnique<RenderBlockFlow::RenderBlockFlowRareData>(*this);
}

#if ENABLE(TEXT_AUTOSIZING)

static inline bool isVisibleRenderText(const RenderObject& renderer)
{
    if (!is<RenderText>(renderer))
        return false;

    auto& renderText = downcast<RenderText>(renderer);
    return !renderText.linesBoundingBox().isEmpty() && !renderText.text().containsOnly<isASCIIWhitespace>();
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

static bool isNonBlocksOrNonFixedHeightListItems(const RenderObject& renderer)
{
    if (!renderer.isRenderBlock())
        return true;
    if (renderer.isListItem())
        return renderer.style().height().type() != LengthType::Fixed;
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
    
    unsigned lineCount = m_lineCountForTextAutosizing;
    if (lineCount == NOT_SET) {
        if (style().visibility() != Visibility::Visible)
            lineCount = NO_LINE;
        else {
            size_t lineCountInBlock = 0;
            if (childrenInline())
                lineCountInBlock = this->lineCount();
            else {
                for (auto& listItem : childrenOfType<RenderListItem>(*this)) {
                    if (!listItem.childrenInline() || listItem.style().visibility() != Visibility::Visible)
                        continue;
                    lineCountInBlock += listItem.lineCount();
                    if (lineCountInBlock > 1)
                        break;
                }
            }
            lineCount = !lineCountInBlock ? NO_LINE : lineCountInBlock == 1 ? ONE_LINE : MULTI_LINE;
        }
    }

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
        if (hasDefiniteLogicalHeight() || view().frameView().pagination().mode != Unpaginated) {
            auto computedValues = computeLogicalHeight(0_lu, logicalTop());
            newColumnHeight = std::max<LayoutUnit>(computedValues.m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight(), 0);
            if (fragmentedFlow->columnHeightAvailable() != newColumnHeight)
                relayoutChildren = true;
        }
        fragmentedFlow->setColumnHeightAvailable(newColumnHeight);
    } else if (is<RenderFragmentedFlow>(*this)) {
        RenderFragmentedFlow& fragmentedFlow = downcast<RenderFragmentedFlow>(*this);

        // FIXME: This is a hack to always make sure we have a page logical height, if said height
        // is known. The page logical height thing in RenderLayoutState is meaningless for flow
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

void RenderBlockFlow::updateColumnProgressionFromStyle(const RenderStyle& style)
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
        if (!oldEndOfInline && (current && !current->isFloating() && !current->isReplacedOrInlineBlock() && !current->isOutOfFlowPositioned()))
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

        if (!result->isOutOfFlowPositioned() && (result->isTextOrLineBreak() || result->isFloating() || result->isReplacedOrInlineBlock() || result->isRenderInline()))
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
    if (cssUnit.type() != LengthType::Auto)
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
    ASSERT(!shouldApplyInlineSizeContainment());

    if (const_cast<RenderBlockFlow&>(*this).tryComputePreferredWidthsUsingModernPath(minLogicalWidth, maxLogicalWidth))
        return;

    float inlineMax = 0;
    float inlineMin = 0;

    const RenderStyle& styleToUse = style();
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

    auto textIndent = LayoutUnit { };
    if (styleToUse.textIndent().isFixed())
        textIndent = LayoutUnit { styleToUse.textIndent().value() };
    else if (auto* containingBlock = this->containingBlock(); containingBlock && containingBlock->style().logicalWidth().isFixed()) {
        // At this point of the shrink-to-fit computatation, we don't have a used value for the containing block width
        // (that's exactly to what we try to contribute here) unless the computed value is fixed.
        textIndent = minimumValueForLength(styleToUse.textIndent(), containingBlock->style().logicalWidth().value());
    }
    RenderObject* previousFloat = 0;
    bool isPrevChildInlineFlow = false;
    bool shouldBreakLineAfterText = false;
    bool canHangPunctuationAtStart = styleToUse.hangingPunctuation().contains(HangingPunctuation::First);
    bool canHangPunctuationAtEnd = styleToUse.hangingPunctuation().contains(HangingPunctuation::Last);
    RenderText* lastText = nullptr;

    bool addedStartPunctuationHang = false;
    
    while (RenderObject* child = childIterator.next()) {
        bool autoWrap = child->isReplacedOrInlineBlock() ? child->parent()->style().autoWrap() :
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
                    Length startMargin = shouldChildInlineMarginContributeToContainerIntrinsicSize(MarginTrimType::InlineStart, *dynamicDowncast<RenderElement>(child)) ? childStyle.marginStartUsing(&style()) : Length(0, LengthType::Fixed); 
                    Length endMargin = shouldChildInlineMarginContributeToContainerIntrinsicSize(MarginTrimType::InlineEnd, *dynamicDowncast<RenderElement>(child)) ? childStyle.marginEndUsing(&style()) : Length(0, LengthType::Fixed);
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
                if (is<RenderBox>(*child) && child->isHorizontalWritingMode() != isHorizontalWritingMode()) {
                    auto& box = downcast<RenderBox>(*child);
                    auto extent = box.computeLogicalHeight(box.borderAndPaddingLogicalHeight(), 0).m_extent;
                    childMinPreferredLogicalWidth = extent;
                    childMaxPreferredLogicalWidth = extent;
                } else
                    computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);

                childMin += childMinPreferredLogicalWidth.ceilToFloat();
                childMax += childMaxPreferredLogicalWidth.ceilToFloat();

                bool clearPreviousFloat = false;
                if (child->isFloating()) {
                    auto childClearValue = RenderStyle::usedClear(*child);
                    if (previousFloat) {
                        auto previousFloatValue = RenderStyle::usedFloat(*previousFloat);
                        clearPreviousFloat =
                            (previousFloatValue == UsedFloat::Left && (childClearValue == UsedClear::Left || childClearValue == UsedClear::Both))
                            || (previousFloatValue == UsedFloat::Right && (childClearValue == UsedClear::Right || childClearValue == UsedClear::Both));
                    }
                    previousFloat = child;
                }

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
                    LayoutUnit ceiledIndent { textIndent.ceilToFloat() };
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
                    if (widths.endsWithBreak)
                        stripFrontSpaces = true;

                } else
                    inlineMax += std::max<float>(0, childMax);
            }

            // Ignore spaces after a list marker.
            if (child->isListMarker())
                stripFrontSpaces = true;
        } else {
            if (styleToUse.collapseWhiteSpace())
                stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);
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

bool RenderBlockFlow::tryComputePreferredWidthsUsingModernPath(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth)
{
    if (!firstInFlowChild())
        return false;

    computeAndSetLineLayoutPath();

    if (lineLayoutPath() != ModernPath)
        return false;

    if (!LayoutIntegration::LineLayout::canUseForPreferredWidthComputation(*this))
        return false;

    if (!modernLineLayout())
        m_lineLayout = makeUnique<LayoutIntegration::LineLayout>(*this);

    modernLineLayout()->updateInlineContentDimensions();

    std::tie(minLogicalWidth, maxLogicalWidth) = modernLineLayout()->computeIntrinsicWidthConstraints();
    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto* renderer = walker.current();
        renderer->setPreferredLogicalWidthsDirty(false);
        if (auto* renderText = dynamicDowncast<RenderText>(renderer))
            renderText->resetMinMaxWidth();
    }
    return true;
}

LayoutUnit RenderBlockFlow::blockFormattingContextInFlowBlockLevelContentHeight() const
{
    ASSERT(establishesBlockFormattingContext());
    // For block layout we should just be able to check the height of the last in flow box
    auto lastChild = lastInFlowChildBox();
    if (!lastChild)
        return 0_lu;
    // Child's margin box starting location + border box height + margin after size
    return logicalMarginBoxTopForChild(*lastChild) + logicalMarginBoxHeightForChild(*lastChild);
}



}
// namespace WebCore
