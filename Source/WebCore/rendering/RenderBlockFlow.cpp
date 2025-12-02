/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "BlockStepSizing.h"
#include "DocumentView.h"
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
#include "LineClampUpdater.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementStyleInlines.h"
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
#include "RenderObjectInlines.h"
#include "RenderTableCellInlines.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TextAutoSizing.h"
#include "TextBoxTrimmer.h"
#include "TextUtil.h"
#include "VisiblePosition.h"
#include <ranges>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderBlockFlow);
WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderBlockFlowRareData);

bool RenderBlock::s_canPropagateFloatIntoSibling = false;

struct SameSizeAsMarginInfo {
    uint32_t bitfields : 16;
    LayoutUnit margins[2];
};

static_assert(sizeof(MarginValues) == sizeof(LayoutUnit[4]), "MarginValues should stay small");
static_assert(sizeof(RenderBlockFlow::MarginInfo) == sizeof(SameSizeAsMarginInfo), "MarginInfo should stay small");

RenderBlockFlowRareData::RenderBlockFlowRareData(const RenderBlockFlow& block)
    : m_margins(positiveMarginBeforeDefault(block), negativeMarginBeforeDefault(block), positiveMarginAfterDefault(block), negativeMarginAfterDefault(block))
    , m_lineBreakToAvoidWidow(-1)
    , m_didBreakAtLineToAvoidWidow(false)
{
}

RenderBlockFlowRareData::~RenderBlockFlowRareData() = default;

// Our MarginInfo state used when laying out block children.
RenderBlockFlow::MarginInfo::MarginInfo(const RenderBlockFlow& block, IgnoreScrollbarForAfterMargin ignoreScrollbarForAfterMargin)
{
    auto& blockStyle = block.style();
    ASSERT(block.isRenderView() || block.parent());

    m_canCollapseWithChildren = !block.createsNewFormattingContext() && !block.isRenderView();

    m_canCollapseMarginBeforeWithChildren = m_canCollapseWithChildren && !block.borderAndPaddingBefore();

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins. To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it. We also don't collapse if we have any bottom border/padding.
    auto canCollapseMarginAfterWithChildren = [&]() -> bool {
        if (!m_canCollapseWithChildren)
            return false;
        if (!blockStyle.logicalHeight().isAuto())
            return false;
        if (block.borderAndPaddingAfter())
            return false;
        // FIXME: Check if all callsites are supposed to take scrollbar into account here.
        return ignoreScrollbarForAfterMargin == IgnoreScrollbarForAfterMargin::Yes ? true : !block.scrollbarLogicalHeight();
    };
    m_canCollapseMarginAfterWithChildren = canCollapseMarginAfterWithChildren();

    m_quirkContainer = block.isRenderTableCell() || block.isBody();

    m_positiveMargin = m_canCollapseMarginBeforeWithChildren ? block.maxPositiveMarginBefore() : 0_lu;
    m_negativeMargin = m_canCollapseMarginBeforeWithChildren ? block.maxNegativeMarginBefore() : 0_lu;
}

RenderBlockFlow::MarginInfo::MarginInfo(bool canCollapseWithChildren, bool canCollapseMarginBeforeWithChildren, bool canCollapseMarginAfterWithChildren, bool quirkContainer, bool atBeforeSideOfBlock, bool atAfterSideOfBlock,  bool hasMarginBeforeQuirk, bool hasMarginAfterQuirk, bool determinedMarginBeforeQuirk, LayoutUnit positiveMargin, LayoutUnit negativeMargin)
    : m_canCollapseWithChildren(canCollapseWithChildren)
    , m_canCollapseMarginBeforeWithChildren(canCollapseMarginBeforeWithChildren)
    , m_canCollapseMarginAfterWithChildren(canCollapseMarginAfterWithChildren)
    , m_quirkContainer(quirkContainer)
    , m_atBeforeSideOfBlock(atBeforeSideOfBlock)
    , m_atAfterSideOfBlock(atAfterSideOfBlock)
    , m_hasMarginBeforeQuirk(hasMarginBeforeQuirk)
    , m_hasMarginAfterQuirk(hasMarginAfterQuirk)
    , m_determinedMarginBeforeQuirk(determinedMarginBeforeQuirk)
    , m_positiveMargin(positiveMargin)
    , m_negativeMargin(negativeMargin)
{
}

RenderBlockFlow::RenderBlockFlow(Type type, Element& element, RenderStyle&& style, OptionSet<BlockFlowFlag> flags)
    : RenderBlock(type, element, WTFMove(style), { }, flags)
#if ENABLE(TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    ASSERT(isRenderBlockFlow());
    setChildrenInline(true);
}

RenderBlockFlow::RenderBlockFlow(Type type, Document& document, RenderStyle&& style, OptionSet<BlockFlowFlag> flags)
    : RenderBlock(type, document, WTFMove(style), { }, flags)
#if ENABLE(TEXT_AUTOSIZING)
    , m_widthForTextAutosizing(-1)
    , m_lineCountForTextAutosizing(NOT_SET)
#endif
{
    ASSERT(isRenderBlockFlow());
    setChildrenInline(true);
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderBlockFlow::~RenderBlockFlow() = default;

void RenderBlockFlow::willBeDestroyed()
{
    if (!renderTreeBeingDestroyed()) {
        if (legacyRootBox()) {
            // We can't wait for RenderBox::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            if (isSelectionBorder())
                frame().selection().setNeedsSelectionUpdate();

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                if (auto* childBox = legacyRootBox()->firstChild())
                    childBox->removeFromParent();
            }
        } else if (auto* parent = this->parent(); parent && parent->isSVGRenderer())
            parent->dirtyLineFromChangedChild();
    }

    if (svgTextLayout())
        svgTextLayout()->deleteLegacyRootBox();

    RenderBlock::willBeDestroyed();
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
        if (auto* siblingBlock = dynamicDowncast<RenderBlockFlow>(*sibling)) {
            if (!siblingBlock->avoidsFloats())
                return siblingBlock;
        }
        if (sibling->isFloating())
            parentHasFloats = true;
    }
    return nullptr;
}

void RenderBlockFlow::rebuildFloatingObjectSetFromIntrudingFloats()
{
    if (layoutContext().isSkippedContentRootForLayout(*this))
        return;

    auto mayHaveStaleFloatingObjects = [&] {
        if (style().isSkippedRootOrSkippedContent())
            return true;
        if (auto wasSkipped = wasSkippedDuringLastLayoutDueToContentVisibility())
            return *wasSkipped;
        return false;
    };
    if (mayHaveStaleFloatingObjects())
        m_floatingObjects = { };

    HashSet<CheckedPtr<RenderBox>> oldIntrudingFloatSet;

    if (m_floatingObjects) {
        m_floatingObjects->setHorizontalWritingMode(isHorizontalWritingMode());
        if (!childrenInline()) {
            for (auto& floatingObject : m_floatingObjects->set()) {
                if (!floatingObject->renderer())
                    continue;
                if (!floatingObject->isDescendant())
                    oldIntrudingFloatSet.add(floatingObject->renderer());
            }
        }
        m_floatingObjects->clear();
    }

    // Inline blocks are covered by the isBlockLevelReplacedOrAtomicInline() check in the avoidFloats method.
    if (avoidsFloats() || isDocumentElementRenderer() || isRenderView() || isFloatingOrOutOfFlowPositioned() || isRenderTableCell()) {
        if (!oldIntrudingFloatSet.isEmpty())
            markAllDescendantsWithFloatsForLayout();
        return;
    }

    auto parentBlock = [&]() -> CheckedPtr<RenderBlockFlow> {
        if (auto* blockFlowParent = dynamicDowncast<RenderBlockFlow>(parent()))
            return blockFlowParent;
        if (auto* inlineBoxParent = dynamicDowncast<RenderInline>(parent())) {
            ASSERT(settings().blocksInInlineLayoutEnabled());
            return dynamicDowncast<RenderBlockFlow>(inlineBoxParent->containingBlock());
        }
        // We should not process floats if the parent node is not a RenderBlock. Otherwise, we will add
        // floats in an invalid context. This will cause a crash arising from a bad cast on the parent.
        // See <rdar://problem/8049753>, where float property is applied on a text node in a SVG.
        return { };
    }();
    if (!parentBlock)
        return;

    // First add in floats from the parent. Self-collapsing blocks let their parent track any floats that intrude into
    // them (as opposed to floats they contain themselves) so check for those here too. If margin collapsing has moved
    // us up past the top a previous sibling then we need to check for floats from the parent too.
    bool parentHasFloats = false;
    RenderBlockFlow* previousBlock = previousSiblingWithOverhangingFloats(parentHasFloats);
    LayoutUnit logicalTopOffset = logicalTop();
    bool parentHasIntrudingFloats = !parentHasFloats && (!previousBlock  || previousBlock->isSelfCollapsingBlock() || previousBlock->logicalTop() > logicalTopOffset) && parentBlock->lowestFloatLogicalBottom() > logicalTopOffset;
    if (parentHasFloats || parentHasIntrudingFloats)
        addIntrudingFloats(parentBlock.get(), parentBlock.get(), parentBlock->logicalLeftOffsetForContent(), logicalTopOffset);

    // Add overhanging floats from the previous RenderBlock, but only if it has a float that intrudes into our space.    
    if (previousBlock) {
        logicalTopOffset -= previousBlock->logicalTop();
        if (previousBlock->lowestFloatLogicalBottom() > logicalTopOffset)
            addIntrudingFloats(previousBlock, parentBlock.get(), 0, logicalTopOffset);
    }

    if (!childrenInline() && !oldIntrudingFloatSet.isEmpty()) {
        // If there are previously intruding floats that no longer intrude, then children with floats
        // should also get layout because they might need their floating object lists cleared.
        if (m_floatingObjects->set().size() < oldIntrudingFloatSet.size())
            markAllDescendantsWithFloatsForLayout();
        else {
            for (auto& floatingObject : m_floatingObjects->set()) {
                if (!floatingObject->renderer())
                    continue;
                oldIntrudingFloatSet.remove(floatingObject->renderer());
                if (oldIntrudingFloatSet.isEmpty())
                    break;
            }
            if (!oldIntrudingFloatSet.isEmpty())
                markAllDescendantsWithFloatsForLayout();
        }
    }
}

void RenderBlockFlow::adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!style().columnCount().isAuto() || !style().columnWidth().isAuto()) {
        // The min/max intrinsic widths calculated really tell how much space elements need when
        // laid out inside the columns. In order to eventually end up with the desired column width,
        // we need to convert them to values pertaining to the multicol container.
        int columnCount = style().columnCount().tryValue().value_or(1).value;
        LayoutUnit columnWidth;
        LayoutUnit colGap = columnGap();
        LayoutUnit gapExtra = (columnCount - 1) * colGap;
        if (auto columnWidthLength = style().columnWidth().tryLength()) {
            columnWidth = Style::evaluate<LayoutUnit>(*columnWidthLength, Style::ZoomNeeded { });
            minLogicalWidth = std::min(minLogicalWidth, columnWidth);
        } else
            minLogicalWidth = minLogicalWidth * columnCount + gapExtra;
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
        CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
        if (scrollableArea && scrollableArea->marquee() && scrollableArea->marquee()->isHorizontal())
            minLogicalWidth = 0;
    }

    if (auto* cell = dynamicDowncast<RenderTableCell>(*this)) {
        auto [ tableCellWidth, usedZoom ] = cell->styleOrColLogicalWidth();
        if (auto fixedTableCellWidth = tableCellWidth.tryFixed(); fixedTableCellWidth && fixedTableCellWidth->isPositive())
            maxLogicalWidth = std::max(minLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(*fixedTableCellWidth));
    }

    int scrollbarWidth = intrinsicScrollbarLogicalWidthIncludingGutter();
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
    return Style::evaluate<LayoutUnit>(style().columnGap(), contentBoxLogicalWidth(), Style::ZoomNeeded { });
}

void RenderBlockFlow::computeColumnCountAndWidth()
{
    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    unsigned desiredColumnCount = 1;
    LayoutUnit desiredColumnWidth = contentBoxLogicalWidth();

    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document().paginated() || (style().columnCount().isAuto() && style().columnWidth().isAuto()) || !style().hasInlineColumnAxis()) {
        setComputedColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }

    LayoutUnit availWidth = desiredColumnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = std::max(1_lu, Style::evaluate<LayoutUnit>(style().columnWidth().tryLength().value_or(0_css_px), Style::ZoomNeeded { }));
    unsigned colCount = std::max<unsigned>(1, style().columnCount().tryValue().value_or(1).value);

    if (style().columnWidth().isAuto() && !style().columnCount().isAuto()) {
        desiredColumnCount = colCount;
        desiredColumnWidth = std::max<LayoutUnit>(0, (availWidth - ((desiredColumnCount - 1) * colGap)) / desiredColumnCount);
    } else if (!style().columnWidth().isAuto() && style().columnCount().isAuto()) {
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
    if (isRenderFileUploadControl() || isRenderTextControl() || isRenderListBox())
        return false;
    if (isRenderSVGBlock())
        return false;
    if (style().display() == DisplayType::RubyBlock || style().display() == DisplayType::RubyAnnotation)
        return false;
#if ENABLE(MATHML)
    if (isRenderMathMLBlock())
        return false;
#endif // ENABLE(MATHML)

    if (!firstChild())
        return false;

    if (style().pseudoElementType())
        return false;

    // If overflow-y is set to paged-x or paged-y on the body or html element, we'll handle the paginating in the RenderView instead.
    if ((style().overflowY() == Overflow::PagedX || style().overflowY() == Overflow::PagedY) && !(isDocumentElementRenderer() || isBody()))
        return true;

    if (!style().specifiesColumns())
        return false;

    // column-axis with opposite writing direction initiates MultiColumnFlow.
    if (!style().hasInlineColumnAxis())
        return true;

    // Non-auto column-width or column-count always initiates MultiColumnFlow.
    if (!style().columnWidth().isAuto() || !style().columnCount().isAuto())
        return true;

    if (desiredColumnCount)
        return desiredColumnCount.value() > 1;

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

void RenderBlockFlow::layoutBlockWithNoChildren()
{
    ASSERT(!firstChild());

    // Empty block containers produce empty formatting lines which may affect trim-start/end.
    auto textBoxTrimmer = TextBoxTrimmer { *this };
    auto repainter = LayoutRepainter { *this };

    // FIXME: Instead of taking floats from previous sibling and forwarding them to next unconditionally, we should completely skip these empty block containers.
    rebuildFloatingObjectSetFromIntrudingFloats();

    auto computeInlineAxisSize =[&] {
        updateLogicalWidth();
    };
    computeInlineAxisSize();

    auto computeBlockAxisSize =[&] {
        auto& style = this->style();

        if (!is<RenderTableCell>(*this)) {
            initMaxMarginValues();
            setHasMarginBeforeQuirk(style.marginBefore().hasQuirk());
            setHasMarginAfterQuirk(style.marginAfter().hasQuirk());
        }
        setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight() + (hasLineIfEmpty() ? lineHeight() : 0_lu));
        updateLogicalHeight();
    };
    computeBlockAxisSize();

    auto computeOverflow = [&] {
        clearOverflow();
        addVisualEffectOverflow();
        addVisualOverflowFromTheme();
    };
    computeOverflow();

    auto updateLayerProperties = [&] {
        updateLayerTransform();
    };
    if (hasLayer())
        updateLayerProperties();

    repainter.repaintAfterLayout();
}

void RenderBlockFlow::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    auto isPaginated = [&] {
        // FIXME: Grid calls into layout outside of regular layout phase (during preferred width computation).
        if (auto* layoutState = view().frameView().layoutContext().layoutState())
            return layoutState->isPaginated();
        return false;
    }();

    if (!firstChild() && !isPaginated && !is<RenderMultiColumnSet>(*this))
        return layoutBlockWithNoChildren();

    LayoutRepainter repainter(*this);

    if (recomputeLogicalWidthAndColumnWidth())
        relayoutChildren = RelayoutChildren::Yes;

    if (auto* layoutState = view().frameView().layoutContext().layoutState(); layoutState && layoutState->legacyLineClamp() && !isFieldset())
        relayoutChildren = RelayoutChildren::Yes;

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
    LayoutUnit pageRemaining;
    const RenderStyle& styleToUse = style();
    do {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || styleToUse.writingMode().isBlockFlipped(), pageLogicalHeight, pageLogicalHeightChanged);

        preparePaginationBeforeBlockLayout(relayoutChildren);
        if (isPaginated)
            pageRemaining = pageLogicalHeightForOffset(0_lu);

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
        bool isCell = isRenderTableCell();
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
        // Expand our intrinsic height to encompass floats.
        LayoutUnit toAdd = borderAndPaddingAfter() + scrollbarLogicalHeight();
        if (lowestFloatLogicalBottom() > (logicalHeight() - toAdd) && createsNewFormattingContext())
            setLogicalHeight(lowestFloatLogicalBottom() + toAdd);
        if (shouldBreakAtLineToAvoidWidow()) {
            setEverHadLayout();
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
    if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*this))
        fragmentedFlow->applyBreakAfterContent(oldClientAfterEdge);

    updateLogicalHeight();
    LayoutUnit newHeight = logicalHeight();

    LayoutUnit alignContentShift;
    auto shouldApplyAlignContent = [&] {
        // Alignment isn't supported when fragmenting.
        if (isPaginated && pageRemaining <= newHeight)
            return false;
        // Table cell alignment is handled in RenderTableCell::computeIntrinsicPadding.
        if (isRenderTableCell())
            return false;
        return !is<HTMLInputElement>(element());
    };
    if (shouldApplyAlignContent()) {
        alignContentShift = shiftForAlignContent(oldHeight, repaintLogicalTop, repaintLogicalBottom);
        oldClientAfterEdge += alignContentShift;
        if (alignContentShift < 0)
            ensureRareBlockFlowData().m_alignContentShift = alignContentShift;
    } else if (hasRareBlockFlowData())
        rareBlockFlowData()->m_alignContentShift = { };

    {
        // FIXME: This could be removed once relayoutForPagination() either stop recursing or we manage to
        // re-order them.
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || styleToUse.writingMode().isBlockFlipped(), pageLogicalHeight, pageLogicalHeightChanged);

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
        if (heightChanged || alignContentShift)
            relayoutChildren = RelayoutChildren::Yes;
        if (isDocumentElementRenderer())
            layoutOutOfFlowBoxes(RelayoutChildren::Yes);
        else
            layoutOutOfFlowBoxes(relayoutChildren);
    }

    updateDescendantTransformsAfterLayout();

    // Add overflow from children (unless we're multi-column, since in that case all our child overflow is clipped anyway).
    computeOverflow(oldClientAfterEdge);

    auto* state = view().frameView().layoutContext().layoutState();
    if (state && state->pageLogicalHeight())
        setPageLogicalOffset(state->pageLogicalOffset(this, logicalTop()));

    updateLayerTransform();

    // FIXME: This repaint logic should be moved into a separate helper function!
    // Repaint with our new bounds if they are different from our old bounds.
    bool didFullRepaint = repainter.repaintAfterLayout();
    if (!didFullRepaint && repaintLogicalTop != repaintLogicalBottom && (styleToUse.usedVisibility() == Visibility::Visible || enclosingLayer()->hasVisibleContent())) {
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
}

void RenderBlockFlow::dirtyForLayoutFromPercentageHeightDescendants()
{
    auto* descendants = percentHeightDescendants();
    if (!descendants)
        return;

    for (auto& descendant : *descendants) {
        // Let's not dirty the height perecentage descendant when it has an absolutely positioned containing block ancestor. We should be able to dirty such boxes through the regular invalidation logic.
        bool descendantNeedsLayout = true;
        for (auto* ancestor = descendant.containingBlock(); ancestor && ancestor != this; ancestor = ancestor->containingBlock()) {
            if (ancestor->isOutOfFlowPositioned()) {
                descendantNeedsLayout = false;
                break;
            }
        }
        if (!descendantNeedsLayout)
            continue;

        for (CheckedPtr<RenderElement> renderer = &descendant; renderer && renderer != this && !renderer->normalChildNeedsLayout(); renderer = renderer->container()) {
            renderer->setChildNeedsLayout(MarkOnlyThis);
            if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(renderer.get())) {
                // If the width of an image is affected by the height of a child (e.g., an image with an aspect ratio),
                // then we have to dirty preferred widths, since even enclosing blocks can become dirty as a result.
                // (A horizontal flexbox that contains an inline image wrapped in an anonymous block for example.)
                if (renderBox->hasIntrinsicAspectRatio() || renderBox->style().hasAspectRatio())
                    renderBox->setNeedsPreferredWidthsUpdate();
            }
        }
    }
}

LayoutUnit RenderBlockFlow::shiftForAlignContent(LayoutUnit intrinsicLogicalHeight, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto alignment = style().alignContent().resolve();

    // Exit if no alignment necessary.
    if (alignment.isNormal() || alignment.isStartward())
        return 0_lu;

    // Calculate alignment shift.
    LayoutUnit computedLogicalHeight = logicalHeight();
    LayoutUnit space = computedLogicalHeight - intrinsicLogicalHeight;
    if (space <= 0) {
        bool overflowIsSafe = (alignment.overflow() == OverflowAlignment::Default && !isScrollContainerY())
            || alignment.overflow() == OverflowAlignment::Safe
            || alignment.position() == ContentPosition::Normal;
        if (overflowIsSafe)
            return 0_lu; // Floored at zero; we're done
    }
    if (alignment.isCentered())
        space = space / 2;

    // Now shift all our content.
    if (CheckedPtr inlineLayout = this->inlineLayout())
        inlineLayout->shiftLinesBy(space);
    else if (auto* svgTextLayout = this->svgTextLayout()) {
        if (isHorizontalWritingMode())
            svgTextLayout->shiftLineBy(0, space);
        else
            svgTextLayout->shiftLineBy(-space, 0);
    } else {
        for (CheckedPtr child = firstChildBox(); child; child = child->nextSiblingBox()) {
            setLogicalTopForChild(*child, logicalTopForChild(*child) + space);
            if (child->isOutOfFlowPositioned() && child->style().hasStaticBlockPosition(isHorizontalWritingMode())) {
                ASSERT(child->layer());
                child->layer()->setStaticBlockPosition(child->layer()->staticBlockPosition() + space);
                child->setChildNeedsLayout(MarkOnlyThis);
            }
        }
    }
    if (m_floatingObjects)
        m_floatingObjects->shiftFloatsBy(space);

    // Update repaint region.
    if (space < 0_lu)
        repaintLogicalTop += space;
    else
        repaintLogicalBottom += space;

    return space;
}

void RenderBlockFlow::layoutInFlowChildren(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom, LayoutUnit& maxFloatLogicalBottom)
{
    if (!firstChild()) {
        // Empty block containers produce empty formatting lines which may affect trim-start/end.
        auto textBoxTrimmer = TextBoxTrimmer { *this };

        auto logicalHeight = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (hasLineIfEmpty())
            logicalHeight += lineHeight();
        setLogicalHeight(logicalHeight);

        repaintLogicalTop = { };
        repaintLogicalBottom = { };
        maxFloatLogicalBottom = { };
        return;
    }

    // FIXME: We should bail out sooner when subtree layout entry point is _inside_ a skipped subtree.
    if ((layoutContext().isSkippedContentRootForLayout(*this) || layoutContext().isSkippedContentForLayout(*this)) && !(isRenderMultiColumnFlow() || multiColumnFlow())) {
        clearNeedsLayoutForSkippedContent();
        return;
    }

    {
        auto textBoxTrimmer = TextBoxTrimmer { *this };
        auto lineClampUpdater = LineClampUpdater { *this };
        childrenInline() ? layoutInlineChildren(relayoutChildren, repaintLogicalTop, repaintLogicalBottom) : layoutBlockChildren(relayoutChildren, maxFloatLogicalBottom);
    }
    {
        auto applyTextBoxTrimEndIfNeeded = [&] {
            // With block children and blocks-inside-inline, there's no way to tell what the last formatted line is until after we finished laying out the subtree.
            // Dirty the last formatted line (in the last IFC) and issue relayout with forcing trimming the last line if applicable.
            if (auto* rootForLastFormattedLine = TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd(*this)) {
                ASSERT(rootForLastFormattedLine != this);
                // FIXME: We should be able to damage the last line only.
                for (RenderBlock* ancestor = rootForLastFormattedLine; ancestor && ancestor != this; ancestor = ancestor->containingBlock())
                    ancestor->setNeedsLayout(MarkOnlyThis);

                auto textBoxTrimmer = TextBoxTrimmer { *this, *rootForLastFormattedLine };
                childrenInline() ? layoutInlineChildren(RelayoutChildren::No, repaintLogicalTop, repaintLogicalBottom) : layoutBlockChildren(RelayoutChildren::No, maxFloatLogicalBottom);
            }
        };
        applyTextBoxTrimEndIfNeeded();
    }
}

void RenderBlockFlow::layoutBlockChildren(RelayoutChildren relayoutChildren, LayoutUnit& maxFloatLogicalBottom)
{
    ASSERT(firstChild());

    setLogicalHeight(borderAndPaddingBefore());
    auto* layoutState = view().frameView().layoutContext().layoutState(); 

    // The margin struct caches all our current margin collapsing state.
    auto marginInfo = MarginInfo { *this, MarginInfo::IgnoreScrollbarForAfterMargin::No };

    bool marginTrimBlockStartFromContainingBlock = layoutState->marginTrimBlockStart();
    bool newMarginTrimBlockStartForSubtree = [&] {
        if (style().marginTrim().contains(Style::MarginTrimSide::BlockStart))
            return true;
        if (!marginInfo.canCollapseMarginBeforeWithChildren() && marginTrimBlockStartFromContainingBlock)
            return false;
        return marginTrimBlockStartFromContainingBlock;
    }();

    layoutState->setMarginTrimBlockStart(newMarginTrimBlockStartForSubtree);
    auto resetBlockStartMarginTrimming = WTF::makeScopeExit([&] {
        layoutState->setMarginTrimBlockStart(marginTrimBlockStartFromContainingBlock);
    });


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

        if (layoutContext().isSkippedContentForLayout(child) && !(isRenderMultiColumnFlow() || multiColumnFlow())) {
            ASSERT(child.isColumnSpanner());

            child.clearNeedsLayout();
            child.clearNeedsLayoutForSkippedContent();
            continue;
        }

        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);

        if (child.isOutOfFlowPositioned()) {
            child.containingBlock()->addOutOfFlowBox(child);
            adjustOutOfFlowBlock(child, marginInfo);
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
                    CheckedPtr block = dynamicDowncast<RenderBlockFlow>(*nextSibling);
                    if (!block)
                        continue;
                    if (block->avoidsFloats() && !block->shrinkToAvoidFloats())
                        continue;
                    if (block->containsFloat(child))
                        block->markAllDescendantsWithFloatsForLayout();
                }
            };
            markSiblingsIfIntrudingForLayout();
            insertFloatingBoxAndMarkForLayout(child);
            adjustFloatingBlock(marginInfo);
            continue;
        }

        // Lay out the child.
        layoutBlockChild(child, marginInfo, previousFloatLogicalBottom, maxFloatLogicalBottom);
    }
    
    if (style().marginTrim().contains(Style::MarginTrimSide::BlockEnd))
        trimBlockEndChildrenMargins();
    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    auto borderBoxLogicalHeight = handleAfterSideOfBlock(marginInfo, logicalHeight() - borderAndPaddingBefore());
    setLogicalHeight(borderBoxLogicalHeight);
}

RenderBlockFlow::BlockPositionAndMargin RenderBlockFlow::layoutBlockChildFromInlineLayout(RenderBox& child, LayoutUnit contentHeight, MarginInfo marginInfo)
{
    // Render tree uses block height to track the child block layout position. Set it to the current position before calling layoutBlockChild.
    setLogicalHeight(contentHeight);

    auto previousFloatLogicalBottom = LayoutUnit { };
    auto maxFloatLogicalBottom = LayoutUnit { };
    layoutBlockChild(child, marginInfo, previousFloatLogicalBottom, maxFloatLogicalBottom);
    return { child.logicalTop() - contentHeight, marginInfo };
}

void RenderBlockFlow::trimBlockEndChildrenMargins()
{
    auto trimSelfCollapsingChildDescendantsMargins = [&](RenderBox& child) {
        ASSERT(child.isSelfCollapsingBlock());
        for (auto itr = RenderIterator<RenderBox>(&child, child.firstChildBox()); itr; itr = itr.traverseNext()) {
            setTrimmedMarginForChild(*itr, Style::MarginTrimSide::BlockStart);
            setTrimmedMarginForChild(*itr, Style::MarginTrimSide::BlockEnd);
        }
    };

    ASSERT(style().marginTrim().contains(Style::MarginTrimSide::BlockEnd));
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
        setTrimmedMarginForChild(*child, Style::MarginTrimSide::BlockEnd);
        if (child->isSelfCollapsingBlock()) {
            setTrimmedMarginForChild(*child, Style::MarginTrimSide::BlockStart);
            childContainingBlock->setLogicalTopForChild(*child, childContainingBlock->logicalHeight());
            
            // If this self-collapsing child has any other children, which must also be
            // self-collapsing, we should trim the margins of all its descendants
            if (child->firstChildBox() && !child->childrenInline())
                trimSelfCollapsingChildDescendantsMargins(*child);

            child = child->previousSiblingBox();
        }  else if (auto* nestedBlock = dynamicDowncast<RenderBlockFlow>(child); nestedBlock && nestedBlock->isBlockContainer() && !nestedBlock->childrenInline() && !nestedBlock->style().marginTrim().contains(Style::MarginTrimSide::BlockEnd)) {
            // The margins *inside* this nested block are protected so we should not introspect and try to trim any of them.
            if (!MarginInfo { *nestedBlock }.canCollapseMarginAfterWithChildren())
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
    for (InlineWalker walker(*this); !walker.atEnd(); walker.advance()) {
        RenderObject& renderer = *walker.current();
        if (!renderer.isOutOfFlowPositioned() && (renderer.isBlockLevelReplacedOrAtomicInline() || renderer.isFloating())) {
            RenderBox& box = downcast<RenderBox>(renderer);
            box.layoutIfNeeded();
            shouldUpdateOverflow = true;
        } else if (is<RenderText>(renderer) || is<RenderInline>(renderer))
            renderer.clearNeedsLayout();
    }

    if (!shouldUpdateOverflow)
        return;

    if (auto* lineLayout = inlineLayout()) {
        lineLayout->updateOverflow();
        return;
    }
}

void RenderBlockFlow::computeAndSetLineLayoutPath()
{
    if (lineLayoutPath() != UndeterminedPath)
        return;
    setLineLayoutPath(LayoutIntegration::LineLayout::canUseFor(*this) ? InlinePath : SvgTextPath);
}

void RenderBlockFlow::layoutInlineChildren(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    computeAndSetLineLayoutPath();

    if (lineLayoutPath() == InlinePath)
        return layoutInlineContent(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);

    if (!svgTextLayout())
        m_lineLayout = makeUnique<LegacyLineLayout>(*this);

    svgTextLayout()->layoutLineBoxes();
    m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow = { };
}

void RenderBlockFlow::performBlockStepSizing(RenderBox& child, LayoutUnit blockStepSizeForChild) const
{
    ASSERT(BlockStepSizing::childHasSupportedStyle(child.style()));

    auto extraSpace = BlockStepSizing::computeExtraSpace(blockStepSizeForChild, logicalMarginBoxHeightForChild(child));
    if (!extraSpace)
        return;

    switch (child.style().blockStepInsert()) {
    case BlockStepInsert::MarginBox:
        BlockStepSizing::distributeExtraSpaceToChildMargins(child, extraSpace, writingMode());
        break;
    case BlockStepInsert::ContentBox:
        BlockStepSizing::distributeExtraSpaceToChildContentArea(child, extraSpace, writingMode());
        break;
    case BlockStepInsert::PaddingBox:
        BlockStepSizing::distributeExtraSpaceToChildPadding(child, extraSpace, writingMode());
        break;
    }
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
    else if (logicalTopEstimate.mightBeSaturated()) [[unlikely]]
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

    auto& childStyle = child.style();
    if (auto blockStepSizeForChild = childStyle.blockStepSize().tryLength(); blockStepSizeForChild && BlockStepSizing::childHasSupportedStyle(childStyle))
        performBlockStepSizing(child, LayoutUnit(blockStepSizeForChild->resolveZoom(Style::ZoomNeeded { })));

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

        if (auto* layoutState = frame().view()->layoutContext().layoutState(); layoutState && layoutState->marginTrimBlockStart())
            layoutState->setMarginTrimBlockStart(false);
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
        if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
            fragmentedFlow->fragmentedFlowDescendantBoxLaidOut(&child);
        // Check for an after page/column break.
        LayoutUnit newHeight = applyAfterBreak(child, logicalHeight(), marginInfo);
        if (newHeight != height())
            setLogicalHeight(newHeight);
    }

    ASSERT(view().frameView().layoutContext().layoutDeltaMatches(oldLayoutDelta));
}

void RenderBlockFlow::adjustOutOfFlowBlock(RenderBox& child, const MarginInfo& marginInfo)
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

void RenderBlockFlow::determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode applyDelta)
{
    LayoutUnit startPosition = borderAndPaddingStart();
    LayoutUnit initialStartPosition = startPosition;
    auto verticalScrollbarWidthClampedToContentBox = std::min<LayoutUnit>(verticalScrollbarWidth(), std::max(0_lu, logicalWidth() - borderAndPaddingLogicalWidth()));
    if ((shouldPlaceVerticalScrollbarOnLeft() || style().scrollbarGutter().isStableBothEdges()) && isHorizontalWritingMode())
        startPosition += (writingMode().isLogicalLeftInlineStart() ? 1 : -1) * verticalScrollbarWidthClampedToContentBox;
    if (style().scrollbarGutter().isStableBothEdges() && !isHorizontalWritingMode())
        startPosition += (writingMode().isLogicalLeftInlineStart() ? 1 : -1) * horizontalScrollbarHeight();
    LayoutUnit totalAvailableLogicalWidth = borderAndPaddingLogicalWidth() + contentBoxLogicalWidth();

    LayoutUnit childMarginStart = marginStartForChild(child);
    LayoutUnit newPosition = startPosition + childMarginStart;

    LayoutUnit positionToAvoidFloats;
    
    if (child.avoidsFloats() && containsFloats())
        positionToAvoidFloats = startOffsetForLine(logicalTopForChild(child), logicalHeightForChild(child));

    // If the child has an offset from the content edge to avoid floats then use that, otherwise let any negative
    // margin pull it back over the content edge or any positive margin push it out.
    // If the child is being centred then the margin calculated to do that has factored in any offset required to
    // avoid floats, so use it if necessary.

    if (style().textAlign() == Style::TextAlign::WebKitCenter || child.style().marginStart(writingMode()).isAuto())
        newPosition = std::max(newPosition, positionToAvoidFloats + childMarginStart);
    else if (positionToAvoidFloats > initialStartPosition)
        newPosition = std::max(newPosition, positionToAvoidFloats);

    setLogicalLeftForChild(child, writingMode().isLogicalLeftInlineStart() ? newPosition : totalAvailableLogicalWidth - newPosition - logicalWidthForChild(child), applyDelta);
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

void RenderBlockFlow::updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop)
{
    if (child.style().isOriginalDisplayInlineType())
        setStaticInlinePositionForChild(child, staticInlinePositionForOriginalDisplayInline(logicalTop));
    else
        setStaticInlinePositionForChild(child, startOffsetForContent());
}

void RenderBlockFlow::setStaticInlinePositionForChild(RenderBox& child, LayoutUnit inlinePosition)
{
    if (enclosingFragmentedFlow()) {
        // Shift the inline position to exclude the fragment offset.
        inlinePosition += startOffsetForContent() - startOffsetForContent();
    }
    child.layer()->setStaticInlinePosition(inlinePosition);
}

LayoutUnit RenderBlockFlow::staticInlinePositionForOriginalDisplayInline(LayoutUnit logicalTop)
{
    Style::TextAlign textAlign = style().textAlign();

    float logicalLeft = logicalLeftOffsetForLine(logicalTop);
    float logicalRight = logicalRightOffsetForLine(logicalTop);

    bool isRightAligned = false;
    switch (textAlign) {
    case Style::TextAlign::Left:
    case Style::TextAlign::WebKitLeft:
        break;
    case Style::TextAlign::Right:
    case Style::TextAlign::WebKitRight:
        isRightAligned = true;
        break;
    case Style::TextAlign::Center:
    case Style::TextAlign::WebKitCenter:
        logicalLeft += (logicalRight - logicalLeft) / 2;
        break;
    case Style::TextAlign::Justify:
    case Style::TextAlign::Start:
        if (writingMode().isBidiRTL())
            isRightAligned = true;
        break;
    case Style::TextAlign::End:
        if (writingMode().isBidiLTR())
            isRightAligned = true;
        break;
    }

    if (isRightAligned == writingMode().isLogicalLeftLineLeft())
        logicalLeft = logicalRight;

    if (!writingMode().isLogicalLeftInlineStart())
        return LayoutUnit(logicalWidth() - logicalLeft);

    return LayoutUnit(logicalLeft);
}

MarginValues RenderBlockFlow::marginValuesForChild(RenderBox& child) const
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

    if (inlineLayout())
        return !inlineLayout()->isSelfCollapsingContent();

    if (svgTextLayout())
        return svgTextLayout()->lineCount();

    // Containers with no children.
    return false;
}

LayoutUnit RenderBlockFlow::collapseMargins(RenderBox& child, MarginInfo& marginInfo)
{
    auto beforeCollapseLogicalTop = logicalHeight();
    auto logicalTop = collapseMarginsWithChildInfo(&child, marginInfo);
    auto addIntrudingFloatsFromPreviousBlocks = [&] {
        for (auto* previousSibling = child.previousSibling(); previousSibling; previousSibling = previousSibling->previousSibling()) {
            CheckedPtr previousBlockSibling = dynamicDowncast<RenderBlockFlow>(previousSibling);
            if (!previousBlockSibling || previousBlockSibling->createsNewFormattingContext())
                continue;
            if (previousBlockSibling->logicalTop() + previousBlockSibling->lowestFloatLogicalBottom() <= logicalTop)
                break;
            // If |child| is a self-collapsing block it may have collapsed into a previous sibling and although it hasn't reduced the height of the parent yet
            // any floats from the parent will now overhang.
            auto oldLogicalHeight = logicalHeight();
            setLogicalHeight(logicalTop);
            if (previousBlockSibling->containsFloats() && !previousBlockSibling->avoidsFloats())
                addOverhangingFloats(*previousBlockSibling, false);
            setLogicalHeight(oldLogicalHeight);
        }
    };
    addIntrudingFloatsFromPreviousBlocks();
    // If |child|'s previous sibling is or contains a self-collapsing block that cleared a float and margin collapsing resulted in |child| moving up
    // into the margin area of the self-collapsing block then the float it clears is now intruding into |child|. Layout again so that we can look for
    // floats in the parent that overhang |child|'s new logical top.
    auto logicalTopIntrudesIntoFloat = logicalTop < beforeCollapseLogicalTop;
    if (logicalTopIntrudesIntoFloat && containsFloats() && !child.avoidsFloats() && lowestFloatLogicalBottom() > logicalTop)
        child.setNeedsLayout();
    return logicalTop;
}

std::optional<LayoutUnit> RenderBlockFlow::selfCollapsingMarginBeforeWithClear(RenderObject* candidate)
{
    CheckedPtr candidateBlockFlow = dynamicDowncast<RenderBlockFlow>(candidate);
    if (!candidateBlockFlow)
        return { };

    if (!candidateBlockFlow->isSelfCollapsingBlock())
        return { };

    if (RenderStyle::usedClear(*candidateBlockFlow) == UsedClear::None || !containsFloats())
        return { };

    auto clear = computedClearDeltaForChild(*candidateBlockFlow, candidateBlockFlow->logicalHeight());
    // Just because a block box has the clear property set, it does not mean we always get clearance (e.g. when the box is below the cleared floats)
    if (clear < candidateBlockFlow->logicalBottom())
        return { };

    return marginValuesForChild(*candidateBlockFlow).positiveMarginBefore();
}

LayoutUnit RenderBlockFlow::collapseMarginsWithChildInfo(RenderBox* child, MarginInfo& marginInfo)
{
    bool childIsSelfCollapsing = child ? child->isSelfCollapsingBlock() : false;
    bool beforeQuirk = child ? hasMarginBeforeQuirk(*child) : false;
    bool afterQuirk = child ? hasMarginAfterQuirk(*child) : false;
    auto trimChildBlockMargins = [&]() {
        auto childBlockFlow = dynamicDowncast<RenderBlockFlow>(child);
        if (childBlockFlow)
            childBlockFlow->setMaxMarginBeforeValues(0_lu, 0_lu);
        setTrimmedMarginForChild(*child, Style::MarginTrimSide::BlockStart);

        // The margin after for a self collapsing child should also be trimmed so it does not 
        // influence the margins of the first non collapsing child
        if (childIsSelfCollapsing) {
            if (childBlockFlow)
                childBlockFlow->setMaxMarginAfterValues(0_lu, 0_lu);
            setTrimmedMarginForChild(*child, Style::MarginTrimSide::BlockEnd);
        }
    };
    if (frame().view()->layoutContext().layoutState()->marginTrimBlockStart()) {
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

    return logicalTop;
}

bool RenderBlockFlow::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& child) const
{
    ASSERT(style().marginTrim().contains(marginTrimSide));
    if (!child.style().isDisplayBlockLevel())
        return false;
    // https://drafts.csswg.org/css-box-4/#margin-trim-block
    // 3.3.1. Trimming Block Container Content
    // For block containers specifically, margin-trim discards:
    switch (marginTrimSide) {
    case Style::MarginTrimSide::BlockStart:
        // The block-start margin of a block-level first child, when trimming at the block-start edge.
        return firstInFlowChildBox() == &child;
    case Style::MarginTrimSide::BlockEnd:
        // The block-end margin of a block-level last child, when trimming at the block-end edge.
        return lastInFlowChildBox() == &child;
    case Style::MarginTrimSide::InlineStart:
    case Style::MarginTrimSide::InlineEnd:
        // It has no effect on the inline-axis margins of block-level descendants, nor on any margins of inline-level descendants.
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

LayoutUnit RenderBlockFlow::clearFloatsIfNeeded(RenderBox& child, MarginInfo& marginInfo, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos)
{
    LayoutUnit heightIncrease = computedClearDeltaForChild(child, yPos);
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
    if (document().inQuirksMode() && hasMarginBeforeQuirk(child) && (isRenderTableCell() || isBody()))
        return;

    LayoutUnit beforeChildMargin = marginBeforeForChild(child);
    positiveMarginBefore = std::max(positiveMarginBefore, beforeChildMargin);
    negativeMarginBefore = std::max(negativeMarginBefore, -beforeChildMargin);

    CheckedPtr childBlock = dynamicDowncast<RenderBlockFlow>(child);
    if (!childBlock)
        return;
    
    if (childBlock->childrenInline() || childBlock->isWritingModeRoot())
        return;

    if (!MarginInfo { *childBlock }.canCollapseMarginBeforeWithChildren())
        return;

    RenderBox* grandchildBox = childBlock->firstChildBox();
    for (; grandchildBox; grandchildBox = grandchildBox->nextSiblingBox()) {
        if (!grandchildBox->isFloatingOrOutOfFlowPositioned())
            break;
    }
    
    if (!grandchildBox)
        return;

    // Make sure to update the block margins now for the grandchild box so that we're looking at current values.
    if (grandchildBox->needsLayout()) {
        grandchildBox->computeAndSetBlockDirectionMargins(*this);
        if (CheckedPtr grandchildBlock = dynamicDowncast<RenderBlock>(*grandchildBox)) {
            grandchildBlock->setHasMarginBeforeQuirk(grandchildBox->style().marginBefore().hasQuirk());
            grandchildBlock->setHasMarginAfterQuirk(grandchildBox->style().marginAfter().hasQuirk());
        }
    }

    // If we have a 'clear' value but also have a margin we may not actually require clearance to move past any floats.
    // If that's the case we want to be sure we estimate the correct position including margins after any floats rather
    // than use 'clearance' later which could give us the wrong position.
    if (RenderStyle::usedClear(*grandchildBox) != UsedClear::None && !childBlock->marginBeforeForChild(*grandchildBox))
        return;

    // Collapse the margin of the grandchild box with our own to produce an estimate.
    childBlock->marginBeforeEstimateForChild(*grandchildBox, positiveMarginBefore, negativeMarginBefore);
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

    logicalTopEstimate += computedClearDeltaForChild(child, logicalTopEstimate);

    estimateWithoutPagination = logicalTopEstimate;

    if (layoutState->isPaginated()) {
        // If the object has a page or column break value of "before", then we should shift to the top of the next page.
        logicalTopEstimate = applyBeforeBreak(child, logicalTopEstimate);

        // For replaced elements and scrolled elements, we want to shift them to the next page if they don't fit on the current one.
        logicalTopEstimate = adjustForUnsplittableChild(child, logicalTopEstimate);

        if (!child.selfNeedsLayout()) {
            if (auto* block = dynamicDowncast<RenderBlock>(child))
                logicalTopEstimate += block->paginationStrut();
        }
    }

    return logicalTopEstimate;
}

void RenderBlockFlow::setCollapsedBottomMargin(const MarginInfo& marginInfo)
{
    if (marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()) {
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        auto shouldTrimBlockEndMargin = style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
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

LayoutUnit RenderBlockFlow::handleAfterSideOfBlock(MarginInfo& marginInfo, LayoutUnit contentBoxLogicalHeight)
{
    marginInfo.setAtAfterSideOfBlock(true);

    // If our last child was a self-collapsing block with clearance then our logical height is flush with the
    // bottom edge of the float that the child clears. The correct vertical position for the margin-collapsing we want
    // to perform now is at the child's margin-top - so adjust our height to that position.
    auto borderBoxLogicalHeight = borderAndPaddingBefore() + contentBoxLogicalHeight;
    if (auto selfCollapsingMarginBeforeWithClear = this->selfCollapsingMarginBeforeWithClear(lastChild()))
        borderBoxLogicalHeight -= *selfCollapsingMarginBeforeWithClear;

    // If we can't collapse with children then add in the bottom margin.
    if (!marginInfo.canCollapseWithMarginAfter() && !marginInfo.canCollapseWithMarginBefore()
        && (!document().inQuirksMode() || !marginInfo.quirkContainer() || !marginInfo.hasMarginAfterQuirk())) {
        borderBoxLogicalHeight += marginInfo.margin();
    }

    // Now add in our bottom border/padding.
    borderBoxLogicalHeight += borderAndPaddingAfter() + scrollbarLogicalHeight();

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    borderBoxLogicalHeight = std::max(borderBoxLogicalHeight, borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);

    return borderBoxLogicalHeight;
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
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    bool isInsideMulticolFlow = !!fragmentedFlow;
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
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    bool isInsideMulticolFlow = !!fragmentedFlow;
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
        if (atBeforeSideOfBlock && oldTop == result && !isOutOfFlowPositioned() && !isRenderTableCell()) {
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
    unsigned lineCount = std::max<unsigned>(renderStyle.orphans().tryValue().value_or(1).value, renderStyle.widows().tryValue().value_or(1).value);
    if (lineCount > 1) {
        auto line = lastLine;
        for (unsigned i = 1; i < lineCount && line->previous(); i++)
            line = line->previous();

        // FIXME: Paginating using line overflow isn't all fine. See FIXME in
        // adjustLinePositionForPagination() for more details.
        lineTop = std::min(line->logicalTop(), line->inkOverflowLogicalTop());
    }
    return lineBottom - lineTop;
}

static inline bool needsAppleMailPaginationQuirk(const RenderBlockFlow& renderer)
{
    if (!renderer.settings().appleMailPaginationQuirkEnabled())
        return false;

    RefPtr element = renderer.element();
    if (element && element->idForStyleResolution() == "messageContentContainer"_s)
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

RenderBlockFlow::LinePaginationAdjustment RenderBlockFlow::computeLineAdjustmentForPagination(const InlineIterator::LineBoxIterator& lineBox, LayoutUnit delta, LayoutUnit floatMinimumBottom)
{
    // In blocks-in-inline case the nested block has been adjusted already by the block layout code.
    bool isBlockInInline = lineBox->lineLeftmostLeafBox() && lineBox->lineLeftmostLeafBox()->isBlockLevelBox();
    if (isBlockInInline) {
        clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
        return { };
    }

    auto computeLeafBoxTopAndBottom = [&] {
        auto lineTop = LayoutUnit::max();
        auto lineBottom = LayoutUnit::min();
        for (auto box = lineBox->lineLeftmostLeafBox(); box; box.traverseLineRightwardOnLine()) {
            if (box->logicalTop() < lineTop)
                lineTop = box->logicalTop();
            if (box->logicalBottom() > lineBottom)
                lineBottom = box->logicalBottom();
        }
        return std::pair { lineTop, lineBottom };
    };

    auto logicalOverflowTop = LayoutUnit { lineBox->inkOverflowLogicalTop() };
    auto logicalOverflowBottom = LayoutUnit { lineBox->inkOverflowLogicalBottom() };
    auto logicalOverflowHeight = logicalOverflowBottom - logicalOverflowTop;
    auto logicalTop = LayoutUnit { lineBox->logicalTop() };
    auto logicalOffset = std::min(logicalTop, logicalOverflowTop);

    if (floatMinimumBottom) {
        // Don't push a float to the next page if it is taller than the page.
        auto floatHeight = floatMinimumBottom - logicalTop;
        if (floatHeight > pageLogicalHeightForOffset(floatMinimumBottom))
            floatMinimumBottom = 0_lu;
    }

    auto logicalBottom = std::max(std::max(LayoutUnit { lineBox->logicalBottom() }, logicalOverflowBottom), floatMinimumBottom);
    auto lineHeight = logicalBottom - logicalOffset;

    updateMinimumPageHeight(logicalOffset, calculateMinimumPageHeight(style(), lineBox, logicalOffset, logicalBottom));
    logicalOffset += delta;

    LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);

    if (!pageLogicalHeight || !hasNextPage(logicalOffset)) {
        clearShouldBreakAtLineToAvoidWidowIfNeeded(*this);
        return { };
    }

    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    bool hasUniformPageLogicalHeight = !fragmentedFlow || fragmentedFlow->fragmentsHaveUniformLogicalHeight();

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

        bool avoidFirstLinePageBreak = lineBox->isFirst() && totalLogicalHeight < pageLogicalHeightAtNewOffset && !floatMinimumBottom;
        auto orphansValue = style().orphans().tryValue();
        bool affectedByOrphans = orphansValue && orphansValue->value >= lineNumber;

        if ((avoidFirstLinePageBreak || affectedByOrphans) && !isOutOfFlowPositioned() && !isRenderTableCell()) {
            if (needsAppleMailPaginationQuirk(*this))
                return { };

            auto firstLineBox = InlineIterator::firstLineBoxFor(*this);
            auto firstLineBoxOverflowTop = LayoutUnit { firstLineBox ? firstLineBox->inkOverflowLogicalTop() : 0 };
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

    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
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
    fragmentedFlow->getFragmentRangeForBox(*this, startFragment, endFragment);
    return (endFragment && fragment != endFragment);
}

LayoutUnit RenderBlockFlow::adjustForUnsplittableChild(RenderBox& child, LayoutUnit logicalOffset, LayoutUnit childBeforeMargin, LayoutUnit childAfterMargin)
{
    // When flexboxes are embedded inside a block flow, they don't perform any adjustments for unsplittable
    // children. We'll treat flexboxes themselves as unsplittable just to get them to paginate properly inside
    // a block flow.
    bool isUnsplittable = childBoxIsUnsplittableForFragmentation(child);
    if (!isUnsplittable) {
        auto* flexibleBox = dynamicDowncast<RenderFlexibleBox>(child);
        if (!(flexibleBox && !flexibleBox->isFlexibleBoxImpl()))
        return logicalOffset;
    }
    
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
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
        bool isInitialLetter = child.isFloating() && child.style().pseudoElementType() == PseudoElementType::FirstLetter && child.style().initialLetter().drop() > 0;
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
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->setPageBreak(this, offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

void RenderBlockFlow::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
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
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        return firstPageLogicalTop + fragmentedFlow->pageLogicalTopForOffset(cumulativeOffset - firstPageLogicalTop);

    return cumulativeOffset - roundToInt(cumulativeOffset - firstPageLogicalTop) % roundToInt(pageLogicalHeight);
}

LayoutUnit RenderBlockFlow::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    // Unsplittable objects clear out the pageLogicalHeight in the layout state as a way of signaling that no
    // pagination should occur. Therefore we have to check this first and bail if the value has been set to 0.
    LayoutUnit pageLogicalHeight = view().frameView().layoutContext().layoutState()->pageLogicalHeight();
    if (!pageLogicalHeight)
        return 0;
    
    // Now check for a flow thread.
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        return fragmentedFlow->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
    return pageLogicalHeight;
}

LayoutUnit RenderBlockFlow::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    offset += offsetFromLogicalTopOfFirstPage();
    
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        return fragmentedFlow->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);

    LayoutUnit pageLogicalHeight = view().frameView().layoutContext().layoutState()->pageLogicalHeight();
    LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
    if (pageBoundaryRule == IncludePageBoundary) {
        // If includeBoundaryPoint is true the line exactly on the top edge of a
        // column will act as being part of the previous column.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
    }
    return remainingHeight;
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

    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->updateSpaceShortageForSizeContainment(this, offsetFromLogicalTopOfFirstPage() + offset, spaceShortage);
}

bool RenderBlockFlow::containsFloat(const RenderBox& renderer) const
{
    return m_floatingObjects && m_floatingObjects->set().contains<FloatingObjectHashTranslator>(renderer);
}

bool RenderBlockFlow::subtreeContainsFloat(const RenderBox& renderer) const
{
    if (containsFloat(renderer))
        return true;

    for (auto& blockFlow : childrenOfType<RenderBlockFlow>(*this)) {
        if (blockFlow.containsFloat(renderer))
            return true;
    }

    return false;
}

bool RenderBlockFlow::subtreeContainsFloats() const
{
    if (containsFloats())
        return true;

    for (auto& blockFlow : descendantsOfType<RenderBlockFlow>(*this)) {
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
        for (auto& ancestor : ancestorsOfType<RenderBlockFlow>(*this)) {
            if (ancestor.isRenderView())
                break;
            if (ancestor.hasOverhangingFloats()) {
                for (auto& floatingObject : m_floatingObjects->set()) {
                    if (!floatingObject->renderer())
                        continue;
                    if (ancestor.hasOverhangingFloat(*floatingObject->renderer())) {
                        parentBlock = &ancestor;
                        break;
                    }
                }
            }
        }

        parentBlock->markAllDescendantsWithFloatsForLayout();
        parentBlock->markSiblingsWithFloatsForLayout();
    }

    if (diff == StyleDifference::Layout && selfNeedsLayout() && childrenInline()) {
        for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance())
            walker.current()->setNeedsPreferredWidthsUpdate();
    }

    if (multiColumnFlow())
        updateStylesForColumnChildren(oldStyle);
}

void RenderBlockFlow::updateStylesForColumnChildren(const RenderStyle* oldStyle)
{
    auto columnsNeedLayout = oldStyle && (oldStyle->columnCount() != style().columnCount() || oldStyle->columnWidth() != style().columnWidth()); 
    for (auto* child = firstChildBox(); child && (child->isRenderFragmentedFlow() || child->isRenderMultiColumnSet()); child = child->nextSiblingBox()) {
        child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), DisplayType::Block));
        if (columnsNeedLayout)
            child->setNeedsLayoutAndPreferredWidthsUpdate();
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

void RenderBlockFlow::addFloatsToNewParent(RenderBlockFlow& toBlockFlow) const
{
    // When a portion of the render tree is being detached, anonymous blocks
    // will be combined as their children are deleted. In this process, the
    // anonymous block later in the tree is merged into the one preceeding it.
    // It can happen that the later block (this) contains floats that the
    // previous block (toBlockFlow) did not contain, and thus are not in the
    // floating objects list for toBlockFlow. This can result in toBlockFlow
    // containing floats that are not in its floating objects list, but are in
    // the floating objects lists of siblings and parents. This can cause
    // problems when the float itself is deleted, since the deletion code
    // assumes that if a float is not in its containing block's floating
    // objects list, it isn't in any floating objects list. In order to
    // preserve this condition (removing it has serious performance
    // implications), we need to copy the floating objects from the old block
    // (this) to the new block (toBlockFlow). The float's metrics will likely
    // all be wrong, but since toBlockFlow is already marked for layout, this
    // will get fixed before anything gets displayed.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=115566
    if (!m_floatingObjects)
        return;

    if (layoutContext().isSkippedContentForLayout(toBlockFlow))
        return;

    if (!toBlockFlow.m_floatingObjects)
        toBlockFlow.createFloatingObjects();

    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        if (toBlockFlow.containsFloat(*floatingObject->renderer()))
            continue;
        toBlockFlow.m_floatingObjects->add(floatingObject->cloneForNewParent());
    }
}

void RenderBlockFlow::computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> options)
{
    RenderBlock::computeOverflow(oldClientAfterEdge, options);

    auto addOverflowFromFloatsIfApplicable = [&] {

        if (!m_floatingObjects)
            return;
        auto shouldIncludeFloats = !multiColumnFlow() && (options.contains(ComputeOverflowOptions::RecomputeFloats) || createsNewFormattingContext() || hasSelfPaintingLayer());
        if (!shouldIncludeFloats)
            return;

        for (auto& floatingObject : m_floatingObjects->set()) {
            if (floatingObject->isDescendant())
                addOverflowFromFloatBox(*floatingObject);
        }
    };
    addOverflowFromFloatsIfApplicable();
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
    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        // Only repaint the object if it is overhanging, is not in its own layer, and
        // is our responsibility to paint (m_shouldPaint is set). When paintAllDescendants is true, the latter
        // condition is replaced with being a descendant of us.
        auto& renderer = *floatingObject->renderer();
        if (logicalBottomForFloat(*floatingObject) > logicalHeight()
            && !renderer.hasSelfPaintingLayer()
            && (floatingObject->paintsFloat() || (paintAllDescendants && renderer.isDescendantOf(this)))) {
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

    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        if (!floatingObject->shouldPaint())
            continue;

        auto floatBoxLocation = flipFloatForWritingModeForChild(*floatingObject, paintOffset + floatingObject->translationOffsetToAncestor());
        if (preservePhase) {
            floatingObject->renderer()->paint(paintInfo, floatBoxLocation);
            continue;
        }
        auto& renderer = *floatingObject->renderer();
        auto paintInfoForFloat = PaintInfo { paintInfo };

        paintInfoForFloat.phase = PaintPhase::BlockBackground;
        renderer.paint(paintInfoForFloat, floatBoxLocation);

        paintInfoForFloat.phase = PaintPhase::ChildBlockBackgrounds;
        renderer.paint(paintInfoForFloat, floatBoxLocation);

        paintInfoForFloat.phase = PaintPhase::Float;
        renderer.paint(paintInfoForFloat, floatBoxLocation);

        paintInfoForFloat.phase = PaintPhase::Foreground;
        renderer.paint(paintInfoForFloat, floatBoxLocation);

        paintInfoForFloat.phase = PaintPhase::Outline;
        renderer.paint(paintInfoForFloat, floatBoxLocation);
    }
}

void RenderBlockFlow::clipOutFloatingBoxes(RenderBlock& rootBlock, const PaintInfo* paintInfo, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock)
{
    if (!m_floatingObjects)
        return;

    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        LayoutRect floatBox(offsetFromRootBlock.width(), offsetFromRootBlock.height(), floatingObject->renderer()->width(), floatingObject->renderer()->height());
        floatBox.move(floatingObject->locationOffsetOfBorderBox());
        rootBlock.flipForWritingMode(floatBox);
        floatBox.move(rootBlockPhysicalPosition.x(), rootBlockPhysicalPosition.y());
        paintInfo->context().clipOut(snappedIntRect(floatBox));
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

void RenderBlockFlow::insertFloatingBoxAndMarkForLayout(RenderBox& floatBox)
{
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

    auto& floatingObject = insertFloatingBox(floatBox);
    setLogicalWidthForFloat(floatingObject, logicalWidthForChild(floatBox) + marginStartForChild(floatBox) + marginEndForChild(floatBox));
}

FloatingObject& RenderBlockFlow::insertFloatingBox(RenderBox& floatBox)
{
    ASSERT(floatBox.isFloating());
    ASSERT(!layoutContext().isSkippedContentForLayout(*this));

    if (!m_floatingObjects)
        createFloatingObjects();

    auto& floatingObjectSet = m_floatingObjects->set();
    auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
    if (it != floatingObjectSet.end())
        return *it->get();

    return *m_floatingObjects->add(FloatingObject::create(floatBox));
}

void RenderBlockFlow::removeFloatingBox(RenderBox& floatBox)
{
    if (!m_floatingObjects)
        return;

    auto& floatingObjectSet = m_floatingObjects->set();
    auto it = floatingObjectSet.find<FloatingObjectHashTranslator>(floatBox);
    if (it != floatingObjectSet.end())
        m_floatingObjects->remove(it->get());
}

LayoutUnit RenderBlockFlow::logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasLeftObjects())
        offset = m_floatingObjects->logicalLeftOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalLeftOffsetForLine(offset);
}

LayoutUnit RenderBlockFlow::logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const
{
    LayoutUnit offset = fixedOffset;
    if (m_floatingObjects && m_floatingObjects->hasRightObjects())
        offset = m_floatingObjects->logicalRightOffsetForPositioningFloat(fixedOffset, logicalTop, heightRemaining);
    return adjustLogicalRightOffsetForLine(offset);
}

void RenderBlockFlow::computeLogicalLocationForFloat(FloatingObject& floatingObject, LayoutUnit& logicalTopOffset)
{
    if (!floatingObject.renderer())
        return;

    auto& childBox = *floatingObject.renderer();
    LayoutUnit logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
    LayoutUnit logicalRightOffset = logicalRightOffsetForContent(); // Constant part of right offset.

    LayoutUnit floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset); // The width we look for.

    LayoutUnit floatLogicalLeft;

    bool insideFragmentedFlow = enclosingFragmentedFlow();
    bool isInitialLetter = childBox.style().pseudoElementType() == PseudoElementType::FirstLetter && childBox.style().initialLetter().drop() > 0;

    if (isInitialLetter) {
        if (auto lowestInitialLetterLogicalBottom = this->lowestInitialLetterLogicalBottom()) {
            auto letterClearance =  *lowestInitialLetterLogicalBottom - logicalTopOffset;
            if (letterClearance > 0) {
                logicalTopOffset += letterClearance;
                setLogicalHeight(logicalHeight() + letterClearance);
            }
        }
    }

    if (RenderStyle::usedFloat(childBox) == UsedFloat::Left) {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft);
        while (logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight) - floatLogicalLeft < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft);
            if (insideFragmentedFlow) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        floatLogicalLeft = std::max(logicalLeftOffset - borderAndPaddingLogicalLeft(), floatLogicalLeft);
    } else {
        LayoutUnit heightRemainingLeft = 1_lu;
        LayoutUnit heightRemainingRight = 1_lu;
        floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight);
        while (floatLogicalLeft - logicalLeftOffsetForPositioningFloat(logicalTopOffset, logicalLeftOffset, &heightRemainingLeft) < floatLogicalWidth) {
            logicalTopOffset += std::min(heightRemainingLeft, heightRemainingRight);
            floatLogicalLeft = logicalRightOffsetForPositioningFloat(logicalTopOffset, logicalRightOffset, &heightRemainingRight);
            if (insideFragmentedFlow) {
                // Have to re-evaluate all of our offsets, since they may have changed.
                logicalRightOffset = logicalRightOffsetForContent(); // Constant part of right offset.
                logicalLeftOffset = logicalLeftOffsetForContent(); // Constant part of left offset.
                floatLogicalWidth = std::min(logicalWidthForFloat(floatingObject), logicalRightOffset - logicalLeftOffset);
            }
        }
        // Use the original width of the float here, since the local variable
        // |floatLogicalWidth| was capped to the available line width. See
        // fast/block/float/clamped-right-float.html.
        floatLogicalLeft -= logicalWidthForFloat(floatingObject);
    }

    LayoutUnit childLogicalLeftMargin = writingMode().isLogicalLeftInlineStart() ? marginStartForChild(childBox) : marginEndForChild(childBox);
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
    if (!fontMetrics.capHeight())
        return;

    LayoutUnit heightOfLine = lineHeight();
    LayoutUnit beforeMarginBorderPadding = childBox.borderAndPaddingBefore() + childBox.marginBefore();
    
    // Make an adjustment to align with the cap height of a theoretical block line.
    LayoutUnit adjustment = fontMetrics.intAscent() + (heightOfLine - fontMetrics.intHeight()) / 2 - fontMetrics.intCapHeight() - beforeMarginBorderPadding;
    logicalTopOffset += adjustment;

    // For sunken and raised caps, we have to make some adjustments. Test if we're sunken or raised (dropHeightDelta will be
    // positive for raised and negative for sunken).
    int dropHeightDelta = childBox.style().initialLetter().height() - childBox.style().initialLetter().drop();

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
        if (!floatingObject.renderer())
            continue;
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        auto& childBox = *floatingObject.renderer();
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
    for (auto& floatingObject : m_floatingObjects->set()) {
        if (floatingObject->isPlaced() && floatingObject->type() & floatType)
            lowestFloatBottom = std::max(lowestFloatBottom, logicalBottomForFloat(*floatingObject));
    }
    return lowestFloatBottom;
}

std::optional<LayoutUnit> RenderBlockFlow::lowestInitialLetterLogicalBottom() const
{
    if (!m_floatingObjects)
        return { };
    auto lowestFloatBottom = std::optional<LayoutUnit> { };
    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        if (floatingObject->isPlaced() && floatingObject->renderer()->style().pseudoElementType() == PseudoElementType::FirstLetter && floatingObject->renderer()->style().initialLetter().drop() > 0)
            lowestFloatBottom = std::max(lowestFloatBottom.value_or(0_lu), logicalBottomForFloat(*floatingObject));
    }
    return lowestFloatBottom;
}

LayoutUnit RenderBlockFlow::addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats)
{
    ASSERT(!layoutContext().isSkippedContentForLayout(*this));
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (!child.containsFloats() || child.createsNewFormattingContext())
        return 0;

    LayoutUnit childLogicalTop = child.logicalTop();
    LayoutUnit childLogicalLeft = child.logicalLeft();
    LayoutUnit lowestFloatLogicalBottom;

    // Floats that will remain the child's responsibility to paint should factor into its
    // overflow.
    auto blockHasOverflowClip = effectiveOverflowX() == Overflow::Clip || effectiveOverflowY() == Overflow::Clip;
    for (auto& floatingObject : child.m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        LayoutUnit floatLogicalBottom = std::min(logicalBottomForFloat(*floatingObject), LayoutUnit::max() - childLogicalTop);
        LayoutUnit logicalBottom = childLogicalTop + floatLogicalBottom;
        lowestFloatLogicalBottom = std::max(lowestFloatLogicalBottom, logicalBottom);
        CheckedRef renderer = *floatingObject->renderer();

        if (logicalBottom > logicalHeight()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(renderer)) {
                LayoutSize offset = isHorizontalWritingMode() ? LayoutSize(-childLogicalLeft, -childLogicalTop) : LayoutSize(-childLogicalTop, -childLogicalLeft);
                bool shouldPaint = false;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly). We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a self-painting layer boundary.
                if (!floatingObject->hasAncestorWithOverflowClip() && renderer->enclosingFloatPaintingLayer() == enclosingFloatPaintingLayer()) {
                    floatingObject->setPaintsFloat(false);
                    shouldPaint = true;
                }
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                m_floatingObjects->add(floatingObject->copyToNewContainer(offset, shouldPaint, true, floatingObject->hasAncestorWithOverflowClip() || blockHasOverflowClip));
            }
        } else {
            if (makeChildPaintOtherFloats && !floatingObject->paintsFloat() && !renderer->hasSelfPaintingLayer()
                && renderer->isDescendantOf(&child) && renderer->enclosingFloatPaintingLayer() == child.enclosingFloatPaintingLayer()) {
                // The float is not overhanging from this block, so if it is a descendant of the child, the child should
                // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
                // layer.
                // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
                // it should paint.
                floatingObject->setPaintsFloat(true);
            }
            
            // Since the float doesn't overhang, it didn't get put into our list. We need to add its overflow in to the child now.
            if (floatingObject->isDescendant())
                child.addOverflowFromFloatBox(*floatingObject);
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

void RenderBlockFlow::addIntrudingFloats(RenderBlockFlow* previousBlock, RenderBlockFlow* container, LayoutUnit logicalLeftOffset, LayoutUnit logicalTopOffset)
{
    ASSERT(!avoidsFloats());
    ASSERT(!layoutContext().isSkippedContentForLayout(*this));

    // If we create our own block formatting context then our contents don't interact with floats outside it, even those from our parent.
    if (createsNewFormattingContext())
        return;

    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!previousBlock->m_floatingObjects)
        return;

    logicalLeftOffset += marginLogicalLeft();

    for (auto& previousBlockFloatingObject : previousBlock->m_floatingObjects->set()) {
        if (!previousBlockFloatingObject->renderer())
            continue;

        if (logicalBottomForFloat(*previousBlockFloatingObject) > logicalTopOffset) {
            if (!m_floatingObjects || !m_floatingObjects->set().contains(previousBlockFloatingObject)) {
                // We create the floating object list lazily.
                if (!m_floatingObjects)
                    createFloatingObjects();

                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since this margin was added already through the modification of the |logicalLeftOffset| variable
                // above. |logicalLeftOffset| will equal the margin in this case, so it's already been taken
                // into account. Only apply this code if previousBlock is the parent, since otherwise the left margin
                // will get applied twice.
                LayoutSize offset = isHorizontalWritingMode()
                    ? LayoutSize(logicalLeftOffset - (previousBlock != container ? previousBlock->marginLeft() : 0_lu), logicalTopOffset)
                    : LayoutSize(logicalTopOffset, logicalLeftOffset - (previousBlock != container ? previousBlock->marginTop() : 0_lu));

                m_floatingObjects->add(previousBlockFloatingObject->copyToNewContainer(offset));
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
        removeFloatingBox(*floatToRemove);
    else if (childrenInline())
        return;

    // Iterate over our block children and mark them as needed.
    for (auto& block : childrenOfType<RenderBlock>(*this)) {
        if (!floatToRemove && block.isFloatingOrOutOfFlowPositioned())
            continue;
        CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(block);
        if (!blockFlow) {
            if (block.shrinkToAvoidFloats() && block.everHadLayout())
                block.setChildNeedsLayout(markParents);
            continue;
        }
        if ((floatToRemove ? blockFlow->subtreeContainsFloat(*floatToRemove) : blockFlow->subtreeContainsFloats()) || blockFlow->shrinkToAvoidFloats())
            blockFlow->markAllDescendantsWithFloatsForLayout(floatToRemove, inLayout);
    }
}

void RenderBlockFlow::markSiblingsWithFloatsForLayout(RenderBox* floatToRemove)
{
    ASSERT(!floatToRemove || floatToRemove->isFloating());

    auto markSiblingsWithIntrusiveFloatForLayoutIfApplicable = [&](auto& floatBoxToRemove) {
        for (auto* nextSibling = this->nextSibling(); nextSibling; nextSibling = nextSibling->nextSibling()) {
            CheckedPtr nextSiblingBlockFlow = dynamicDowncast<RenderBlockFlow>(*nextSibling);
            if (!nextSiblingBlockFlow)
                continue;
            auto shouldCheckSubtree = isSkippedContentRoot(*nextSiblingBlockFlow) || nextSiblingBlockFlow->isSkippedContent() || nextSiblingBlockFlow->containsFloat(floatBoxToRemove);
            if (shouldCheckSubtree)
                nextSiblingBlockFlow->markAllDescendantsWithFloatsForLayout(&floatBoxToRemove);
        }
    };

    if (floatToRemove) {
        markSiblingsWithIntrusiveFloatForLayoutIfApplicable(*floatToRemove);
        return;
    }

    if (!m_floatingObjects)
        return;

    for (auto& floatingObject : m_floatingObjects->set()) {
        if (!floatingObject->renderer())
            continue;
        markSiblingsWithIntrusiveFloatForLayoutIfApplicable(*floatingObject->renderer());
    }
}

LayoutPoint RenderBlockFlow::flipFloatForWritingModeForChild(const FloatingObject& child, const LayoutPoint& point) const
{
    if (!child.renderer())
        return point;

    if (!writingMode().isBlockFlipped())
        return point;
    
    // This is similar to RenderBox::flipForWritingModeForChild. We have to subtract out our left/top offsets twice, since
    // it's going to get added back in. We hide this complication here so that the calling code looks normal for the unflipped
    // case.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child.renderer()->height() - 2 * child.locationOffsetOfBorderBox().height());
    return LayoutPoint(point.x() + width() - child.renderer()->width() - 2 * child.locationOffsetOfBorderBox().width(), point.y());
}

LayoutUnit RenderBlockFlow::computedClearDeltaForChild(RenderBox& child, LayoutUnit logicalTop)
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
            LayoutUnit availableLogicalWidthAtNewLogicalTopOffset = availableLogicalWidthForLine(newLogicalTop, logicalHeightForChild(child));
            if (availableLogicalWidthAtNewLogicalTopOffset == availableLogicalWidthForContent())
                return newLogicalTop - logicalTop;

            LayoutRect borderBox = child.borderBoxRect();
            LayoutUnit childLogicalWidthAtOldLogicalTopOffset = isHorizontalWritingMode() ? borderBox.width() : borderBox.height();

            // FIXME: None of this is right for perpendicular writing-mode children.
            LayoutUnit childOldLogicalWidth = child.logicalWidth();
            LayoutUnit childOldMarginLeft = child.marginLeft();
            LayoutUnit childOldMarginRight = child.marginRight();
            LayoutUnit childOldLogicalTop = child.logicalTop();

            child.setLogicalTop(newLogicalTop);
            child.updateLogicalWidth();
            borderBox = child.borderBoxRect();
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
    if (auto* renderView = dynamicDowncast<RenderView>(*this))
        adjustedLocation += toLayoutSize(renderView->frameView().scrollPosition());

    for (auto& floatingObject : m_floatingObjects->set() | std::views::reverse) {
        auto* renderer = floatingObject->renderer();
        if (!renderer)
            continue;
        if (floatingObject->shouldPaint()) {
            LayoutPoint childPoint = flipFloatForWritingModeForChild(*floatingObject, adjustedLocation + floatingObject->translationOffsetToAncestor());
            if (renderer->hitTest(request, result, locationInContainer, childPoint)) {
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

    return inlineLayout() && inlineLayout()->hitTest(request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

void RenderBlockFlow::addOverflowFromInlineChildren()
{
    if (inlineLayout()) {
        inlineLayout()->collectOverflow();
        return;
    }
    
    if (svgTextLayout())
        svgTextLayout()->addOverflowFromInlineChildren();
}

void RenderBlockFlow::addOverflowFromInFlowChildren(OptionSet<ComputeOverflowOptions> options)
{
    if (childrenInline()) {
        addOverflowFromInlineChildren();

        // If this block is flowed inside a flow thread, make sure its overflow is propagated to the containing fragments.
        if (m_overflow) {
            if (CheckedPtr flow = enclosingFragmentedFlow())
                flow->addFragmentsVisualOverflow(*this, m_overflow->visualOverflowRect());
        }
    } else
        RenderBlock::addOverflowFromInFlowChildren(options);
}

std::optional<LayoutUnit> RenderBlockFlow::firstLineBaseline() const
{
    if (isWritingModeRoot() && !isGridItem() && !isFlexItem())
        return { };

    if (shouldApplyLayoutContainment())
        return { };

    if (!childrenInline())
        return RenderBlock::firstLineBaseline();

    if (!hasLines()) {
        if (hasLineIfEmpty()) {
            auto& fontMetrics = firstLineStyle().metricsOfPrimaryFont();
            return { LayoutUnit(borderAndPaddingBefore() + fontMetrics.intAscent() + (firstLineStyle().computedLineHeight() - fontMetrics.intHeight()) / 2) };
        }
        return { };
    }

    if (auto* lineLayout = this->inlineLayout())
        return LayoutUnit { floorToInt(lineLayout->firstLineBaseline()) };

    ASSERT_NOT_REACHED();
    return { };
}

std::optional<LayoutUnit> RenderBlockFlow::lastLineBaseline() const
{
    if (isWritingModeRoot() && !isGridItem() && !isFlexItem())
        return { };

    if (shouldApplyLayoutContainment())
        return { };

    if (!childrenInline())
        return RenderBlock::lastLineBaseline();

    if (!hasLines()) {
        if (hasLineIfEmpty()) {
            auto& fontMetrics = style().metricsOfPrimaryFont();
            return { LayoutUnit(borderAndPaddingBefore() + fontMetrics.intAscent() + (style().computedLineHeight() - fontMetrics.intHeight()) / 2) };
        }
        return { };
    }

    if (auto* lineLayout = inlineLayout())
        return LayoutUnit { floorToInt(lineLayout->lastLineBaseline()) };

    ASSERT_NOT_REACHED();
    return { };
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
            sibling = object->previousInFlowSibling();
            while (sibling) {
                auto* siblingBlock = dynamicDowncast<RenderBlock>(*sibling);
                if (siblingBlock && !siblingBlock->isSelectionRoot())
                    break;
                sibling = sibling->previousInFlowSibling();
            }

            auto& objectBlock = downcast<RenderBlock>(*object);
            offsetToBlockBefore -= LayoutSize(objectBlock.logicalLeft(), objectBlock.logicalTop());
            object = object->parent();
        } while (!sibling && is<RenderBlock>(object) && !downcast<RenderBlock>(*object).isSelectionRoot());

        if (!sibling)
            return nullptr;

        auto* beforeBlock = downcast<RenderBlock>(sibling);

        offsetToBlockBefore += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

        auto* child = beforeBlock->lastChild();
        while (auto* childBlock = dynamicDowncast<RenderBlock>(child)) {
            beforeBlock = childBlock;
            offsetToBlockBefore += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
            child = beforeBlock->lastChild();
        }
        return dynamicDowncast<RenderBlockFlow>(beforeBlock);
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
    ASSERT(!isSkippedContent());

    auto updateLastLogicalValues = [&](auto logicalTop, auto logicalLeft, auto logicalRight) {
        lastLogicalTop = logicalTop;
        lastLogicalLeft = logicalLeft;
        lastLogicalRight = logicalRight;
    };

    bool containsStart = selectionState() == HighlightState::Start || selectionState() == HighlightState::Both;
    if (isSkippedContentRoot(*this)) {
        if (containsStart)
            updateLastLogicalValues(blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight(), logicalLeftOffsetForContent(), logicalRightOffsetForContent());
        return { };
    }

    if (!hasLines()) {
        // Update our lastLogicalTop to be the bottom of the block. <hr>s or empty blocks with height can trip this case.
        if (containsStart)
            updateLastLogicalValues(blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight(), logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache), logicalRightSelectionOffset(rootBlock, logicalHeight(), cache));
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
            for (auto box = lineBox->lineLeftmostLeafBox(); box; box.traverseLineRightwardOnLine()) {
                if (box->selectionState() != RenderObject::HighlightState::None)
                    return box;
            }
            return { };
        }();

        auto lastSelectedBox = [&]() -> InlineIterator::LeafBoxIterator {
            for (auto box = lineBox->lineRightmostLeafBox(); box; box.traverseLineLeftwardOnLine()) {
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
            for (auto box = firstSelectedBox; box; box.traverseLineRightwardOnLine()) {
                if (box->selectionState() != RenderObject::HighlightState::None) {
                    LayoutRect logicalRect { lastLogicalLeft, selTop, LayoutUnit(box->logicalLeftIgnoringInlineDirection() - lastLogicalLeft), selHeight };
                    logicalRect.move(isHorizontalWritingMode() ? offsetFromRootBlock : LayoutSize(offsetFromRootBlock.height(), offsetFromRootBlock.width()));
                    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
                    if (isPreviousBoxSelected && gapRect.width() > 0 && gapRect.height() > 0) {
                        if (paintInfo && box->renderer().parent()->style().usedVisibility() == Visibility::Visible)
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
            && selectionState() != HighlightState::Both)
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
        updateLastLogicalValues(blockDirectionOffset(rootBlock, offsetFromRootBlock) + lastLineSelectionBottom, logicalLeftSelectionOffset(rootBlock, lastLineSelectionBottom, cache), logicalRightSelectionOffset(rootBlock, lastLineSelectionBottom, cache));
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
    rareBlockFlowData()->m_multiColumnFlow = { };
}

int RenderBlockFlow::lineCount() const
{
    if (!childrenInline()) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (inlineLayout())
        return inlineLayout()->lineCount();
    if (svgTextLayout())
        return svgTextLayout()->lineCount();

    return 0;
}

bool RenderBlockFlow::containsNonZeroBidiLevel() const
{
    for (auto lineBox = InlineIterator::firstLineBoxFor(*this); lineBox; lineBox.traverseNext()) {
        for (auto box = lineBox->lineLeftmostLeafBox(); box; box = box.traverseLineRightwardOnLine()) {
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

    auto* textBox = dynamicDowncast<InlineIterator::TextBoxIterator>(box);
    if (!textBox)
        return makeDeprecatedLegacyPosition(box->renderer().nonPseudoNode(), start ? box->renderer().caretMinOffset() : box->renderer().caretMaxOffset());

    return makeDeprecatedLegacyPosition((*textBox)->renderer().nonPseudoNode(), start ? (*textBox)->start() : (*textBox)->end());
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
            if (!child->height() || child->style().usedVisibility() != WebCore::Visibility::Visible || child->isFloatingOrOutOfFlowPositioned())
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
            
            if (localPoint.y() >= top && localPoint.y() < bottom) {
                if (auto* childAsBlock = dynamicDowncast<RenderBlock>(*child)) {
                    block = childAsBlock;
                    break;
                }
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
    for (auto box = InlineIterator::firstRootInlineBoxFor(blockFlow); box; box.traverseInlineBoxLineRightward()) {
        if (previousRootInlineBoxBottom) {
            if (localPoint.y() < *previousRootInlineBoxBottom)
                return nullptr;

            if (localPoint.y() > *previousRootInlineBoxBottom && localPoint.y() < box->logicalTop()) {
                if (auto closestBox = closestBoxForHorizontalPosition(*box->lineBox(), localPoint.x())) {
                    if (auto* textRenderer = dynamicDowncast<RenderText>(closestBox->renderer()))
                        return const_cast<RenderText*>(textRenderer);
                }
            }
        }
        previousRootInlineBoxBottom = box->logicalBottom();
    }
    return nullptr;
}

PositionWithAffinity RenderBlockFlow::positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents, HitTestSource source)
{
    ASSERT(childrenInline());

    auto firstLineBox = InlineIterator::firstLineBoxFor(*this);

    if (!firstLineBox)
        return createPositionWithAffinity(0, Affinity::Downstream);

    bool linesAreFlipped = writingMode().isLineInverted();
    bool blocksAreFlipped = writingMode().isBlockFlipped();

    // look for the closest line box in the root box which is at the passed-in y coordinate
    InlineIterator::LeafBoxIterator closestBox;
    InlineIterator::LineBoxIterator firstLineBoxWithChildren;
    InlineIterator::LineBoxIterator lastLineBoxWithChildren;
    for (auto lineBox = firstLineBox; lineBox; lineBox.traverseNext()) {
        if (!lineBox->lineLeftmostLeafBox())
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
                while (nextLineBoxWithChildren && !nextLineBoxWithChildren->lineLeftmostLeafBox())
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

    bool moveCaretToBoundary = protectedFrame()->protectedEditor()->behavior().shouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

    if (!moveCaretToBoundary && !closestBox && lastLineBoxWithChildren) {
        // y coordinate is below last root line box, pretend we hit it
        closestBox = closestBoxForHorizontalPosition(*lastLineBoxWithChildren, pointInLogicalContents.x());
    }

    if (closestBox) {
        if (moveCaretToBoundary) {
            auto firstLineWithChildrenTop = LayoutUnit { std::min(previousLineBoxContentBottomOrBorderAndPadding(*firstLineBoxWithChildren), firstLineBoxWithChildren->contentLogicalTop()) };
            if (pointInLogicalContents.y() < firstLineWithChildrenTop
                || (blocksAreFlipped && pointInLogicalContents.y() == firstLineWithChildrenTop)) {
                auto box = firstLineBoxWithChildren->lineLeftmostLeafBox();
                if (box->isLineBreak()) {
                    if (auto next = box->nextLineRightwardOnLineIgnoringLineBreak())
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
        if (closestBox->renderer().isBlockLevelReplacedOrAtomicInline())
            return positionForPointRespectingEditingBoundaries(*this, const_cast<RenderBox&>(downcast<RenderBox>(closestBox->renderer())), point, source);
        return const_cast<RenderObject&>(closestBox->renderer()).positionForPoint(point, source, nullptr);
    }

    if (lastLineBoxWithChildren) {
        // We hit this case for Mac behavior when the Y coordinate is below the last box.
        if (auto blockBox = lastLineBoxWithChildren->blockLevelBox()) {
            auto& childBlockRenderer = const_cast<RenderBox&>(downcast<RenderBox>(blockBox->renderer()));
            return positionForPointRespectingEditingBoundaries(*this, childBlockRenderer, pointInLogicalContents, source);
        }
        ASSERT(moveCaretToBoundary);
        InlineIterator::LineLogicalOrderCache orderCache;
        if (auto logicallyLastBox = InlineIterator::lastLeafOnLineInLogicalOrderWithNode(lastLineBoxWithChildren, orderCache))
            return positionForRun(*this, logicallyLastBox, false);
    }

    // Can't reach this. We have a root line box, but it has no kids.
    // FIXME: This should ASSERT_NOT_REACHED(), but clicking on placeholder text
    // seems to hit this code path.
    return createPositionWithAffinity(0, Affinity::Downstream);
}

Position RenderBlockFlow::positionForPoint(const LayoutPoint& point, HitTestSource source)
{
    return positionForPoint(point, source, nullptr).position();
}

PositionWithAffinity RenderBlockFlow::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer*)
{
    return RenderBlock::positionForPoint(point, source, nullptr);
}

void RenderBlockFlow::paintInlineChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(childrenInline());

    if (!inlineLayout())
        return;

    inlineLayout()->paint(paintInfo, paintOffset);
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
            layoutBlock(RelayoutChildren::No);
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

bool RenderBlockFlow::hasBlocksInInlineLayout() const
{
    auto* inlineLayout = this->inlineLayout();
    return inlineLayout && inlineLayout->hasBlocks();
}

void RenderBlockFlow::invalidateLineLayout(InvalidationReason invalidationReason)
{
    if (lineLayoutPath() == UndeterminedPath)
        return;

    auto issueNeedsLayoutIfApplicable = [&] {
        if (selfNeedsLayout() || normalChildNeedsLayout())
            return;
        // FIXME: We should just kick off a subtree layout here (if needed at all) see webkit.org/b/172947.
        setNeedsLayout();
    };

    if (inlineLayout()) {
        ASSERT(!m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow);
        m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow = inlineContentTopAndBottomIncludingInkOverflow();
    }

    switch (invalidationReason) {
    case InvalidationReason::InternalMove:
        if (AXObjectCache* cache = protectedDocument()->existingAXObjectCache())
            cache->deferRecomputeIsIgnored(protectedElement().get());
        break;
    case InvalidationReason::ContentChange: {
        // Since we eagerly remove the display content here, repaints issued between this invalidation (triggered by style change/content mutation) and the subsequent layout would produce empty rects.
        repaint();
        for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
            auto& renderer = *walker.current();
            if (!renderer.everHadLayout())
                continue;
            if (!renderer.isInFlow() && inlineLayout()->contains(downcast<RenderElement>(renderer)))
                renderer.repaint();
            renderer.setNeedsPreferredWidthsUpdate();
        }
        issueNeedsLayoutIfApplicable();
        break;
    }
    case InvalidationReason::InsertionOrRemoval:
        setLineLayoutPath(UndeterminedPath);
        issueNeedsLayoutIfApplicable();
        break;
    default:
        break;
    }

    m_lineLayout = std::monostate();
}

static bool hasSimpleStaticPositionForInlineLevelOutOfFlowChildrenByStyle(const RenderStyle& rootStyle)
{
    if (rootStyle.textAlign() != Style::TextAlign::Start)
        return false;
    if (!rootStyle.textIndent().length.isKnownZero())
        return false;
    return true;
}

static void setFullRepaintOnParentInlineBoxLayerIfNeeded(const RenderText& renderer)
{
    // Repaints (on self) are normally issued either during layout using LayoutRepainter inside ::layout() functions (#1)
    // or after layout, while recursing the layer tree (#2).
    // Additionally, repaint at the block level (#3) takes care of regular in-flow content.
    // However in case of text content, we don't have (#1), (#2) is primarily a geometry diff type of repaint meaning
    // no repaint happens unless content size changes (or full repaint bit is set on the layer)
    // and (#3) only works when the block container and the text content share the same layer.
    // Here we mark the parent inline box's layer dirty to trigger repaint at (#2).
    if (!renderer.needsLayout())
        return;
    CheckedPtr parent = renderer.parent();
    if (!parent) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!parent->isInline() || !parent->hasLayer())
        return;
    downcast<RenderLayerModelObject>(*parent).checkedLayer()->setRepaintStatus(RepaintStatus::NeedsFullRepaint);
}

std::pair<float, float> RenderBlockFlow::inlineContentTopAndBottomIncludingInkOverflow() const
{
    if (m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow)
        return *m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow;

    if (!inlineLayout())
        return { };

    auto firstLineBox = InlineIterator::firstLineBoxFor(*this);
    auto lastLineBox = InlineIterator::lastLineBoxFor(*this);
    if (!firstLineBox)
        return { };

    auto logicalTop = std::min(firstLineBox->logicalTop(), firstLineBox->contentLogicalTop());
    auto logicalBottom = std::max(lastLineBox->logicalBottom(), lastLineBox->contentLogicalBottom());

    if (!inlineLayout()->hasInkOverflow())
        return { logicalTop, logicalBottom };

    for (auto lineBox = firstLineBox; lineBox; lineBox.traverseNext()) {
        logicalTop = std::min(logicalTop, lineBox->inkOverflowLogicalTop());
        logicalBottom = std::max(logicalBottom, lineBox->inkOverflowLogicalBottom());
    }
    return { logicalTop, logicalBottom };
}

bool RenderBlockFlow::markInlineContentDirtyForLayout(RelayoutChildren relayoutChildren)
{
    auto hasSimpleOutOfFlowContentOnly = !hasLineIfEmpty();
    auto hasSimpleStaticPositionForInlineLevelOutOfFlowContentByStyle = hasSimpleStaticPositionForInlineLevelOutOfFlowChildrenByStyle(style());

    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();
        auto* box = dynamicDowncast<RenderBox>(renderer);
        auto childNeedsLayout = relayoutChildren == RelayoutChildren::Yes || (box && box->hasRelativeDimensions());
        auto childNeedsPreferredWidthComputation = relayoutChildren == RelayoutChildren::Yes && box && box->shouldInvalidatePreferredWidths();
        if (childNeedsLayout)
            renderer.setNeedsLayout(MarkOnlyThis);
        if (childNeedsPreferredWidthComputation)
            renderer.setNeedsPreferredWidthsUpdate(MarkOnlyThis);

        if (renderer.isOutOfFlowPositioned()) {
            renderer.containingBlock()->addOutOfFlowBox(*box);
            // FIXME: This is only needed because of the synchronous layout call in setStaticPositionsForSimpleOutOfFlowContent
            // which itself appears to be a workaround for a bad subtree layout shown by
            // fast/block/positioning/static_out_of_flow_inside_layout_boundary.html
            auto& style = downcast<RenderElement>(renderer).style();
            auto hasParentRelativeHeightOrTop = [&] {
                if (style.logicalHeight().isPercentOrCalculated() || style.logicalTop().isPercentOrCalculated())
                    return true;
                return !style.logicalBottom().isAuto();
            }();
            if (hasParentRelativeHeightOrTop)
                hasSimpleOutOfFlowContentOnly = false;

            if (hasSimpleOutOfFlowContentOnly && style.isOriginalDisplayInlineType())
                hasSimpleOutOfFlowContentOnly = hasSimpleStaticPositionForInlineLevelOutOfFlowContentByStyle;
        } else
            hasSimpleOutOfFlowContentOnly = false;

        if (!renderer.needsLayout() && !renderer.needsPreferredLogicalWidthsUpdate())
            continue;

        if (auto* renderText = dynamicDowncast<RenderText>(renderer))
            setFullRepaintOnParentInlineBoxLayerIfNeeded(*renderText);

        if (auto* inlineLevelBox = dynamicDowncast<RenderBox>(renderer)) {
            // FIXME: Move this to where the actual content change happens and call it on the parent IFC.
            auto shouldTriggerFullLayout = inlineLevelBox->isInline() && (inlineLevelBox->needsSimplifiedNormalFlowLayout() || inlineLevelBox->normalChildNeedsLayout() || inlineLevelBox->outOfFlowChildNeedsLayout()) && inlineLayout();
            if (shouldTriggerFullLayout)
                inlineLayout()->boxContentWillChange(*inlineLevelBox);
        }

        if (is<RenderLineBreak>(renderer) || is<RenderInline>(renderer) || is<RenderText>(renderer))
            renderer.clearNeedsLayout();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE) && ENABLE(AX_THREAD_TEXT_APIS)
        if (CheckedPtr cache = protectedDocument()->existingAXObjectCache())
            cache->onTextRunsChanged(renderer);
#endif

        if (CheckedPtr renderCombineText = dynamicDowncast<RenderCombineText>(renderer)) {
            renderCombineText->combineTextIfNeeded();
            continue;
        }
    }
    return hasSimpleOutOfFlowContentOnly;
}

std::optional<LayoutUnit> RenderBlockFlow::updateLineClampStateAndLogicalHeightAfterLayout()
{
    auto& layoutState = *view().frameView().layoutContext().layoutState();
    auto& inlineLayout = *this->inlineLayout();

    auto legacyLineClamp = layoutState.legacyLineClamp();
    if (!legacyLineClamp || isFloatingOrOutOfFlowPositioned())
        return { };

    legacyLineClamp->currentLineCount += inlineLayout.lineCount();
    if (legacyLineClamp->clampedRenderer) {
        // We've already clamped this flex container at a previous flex item.
        layoutState.setLegacyLineClamp(*legacyLineClamp);
        return { };
    }

    auto clampedContentHeight = [&]() -> std::optional<LayoutUnit> {
        if (auto clampedHeight = inlineLayout.clampedContentLogicalHeight())
            return clampedHeight;
        if (legacyLineClamp->currentLineCount == legacyLineClamp->maximumLineCount) {
            // Even if we did not truncate the content, this might be our clamping position.
            return LayoutUnit { inlineLayout.contentLogicalHeight() };
        }
        return { };
    };
    if (auto logicalHeight = clampedContentHeight()) {
        legacyLineClamp->clampedContentLogicalHeight = logicalHeight;
        legacyLineClamp->clampedRenderer = this;
        layoutState.setLegacyLineClamp(*legacyLineClamp);
        return logicalHeight;
    }
    layoutState.setLegacyLineClamp(*legacyLineClamp);
    return { };
}

void RenderBlockFlow::updateRepaintTopAndBottomAfterLayout(RelayoutChildren relayoutChildren, std::optional<LayoutRect> partialRepaintRect, std::pair<float, float> oldContentTopAndBottomIncludingInkOverflow, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto isFullLayout = selfNeedsLayout() || relayoutChildren == RelayoutChildren::Yes;
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

    auto contentTopAndBottomIncludingInkOverflow = inlineContentTopAndBottomIncludingInkOverflow();
    auto damageTopIncludingInkOverflow = std::min(oldContentTopAndBottomIncludingInkOverflow.first, contentTopAndBottomIncludingInkOverflow.first);
    auto damageBottomIncludingInkOverflow = std::max(oldContentTopAndBottomIncludingInkOverflow.second, contentTopAndBottomIncludingInkOverflow.second);

    repaintLogicalTop = std::min(LayoutUnit::fromFloatFloor(damageTopIncludingInkOverflow), borderAndPaddingBefore());
    repaintLogicalBottom = std::max(LayoutUnit::fromFloatCeil(damageBottomIncludingInkOverflow), logicalHeight());
}

void RenderBlockFlow::layoutInlineContent(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto hasSimpleOutOfFlowContentOnly = markInlineContentDirtyForLayout(relayoutChildren);

    if (hasSimpleOutOfFlowContentOnly) {
        // Shortcut the layout.
        m_lineLayout = std::monostate();

        setStaticPositionsForSimpleOutOfFlowContent();
        setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
        return;
    }

    auto oldContentTopAndBottomIncludingInkOverflow = inlineContentTopAndBottomIncludingInkOverflow();
    m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow = { };

    if (!inlineLayout())
        m_lineLayout = makeUnique<LayoutIntegration::LineLayout>(*this);
    auto& inlineLayout = *this->inlineLayout();

    ASSERT(containingBlock() || is<RenderView>(*this));
    inlineLayout.updateFormattingContexGeometries(containingBlock() ? containingBlockLogicalWidthForContent() : LayoutUnit());

    auto marginInfo = MarginInfo { *this, MarginInfo::IgnoreScrollbarForAfterMargin::No };
    auto partialRepaintRect = inlineLayout.layout(marginInfo, relayoutChildren == RelayoutChildren::Yes ? LayoutIntegration::LineLayout::ForceFullLayout::Yes : LayoutIntegration::LineLayout::ForceFullLayout::No);

    auto clampedContentHeight = updateLineClampStateAndLogicalHeightAfterLayout();
    auto contentBoxHeight = clampedContentHeight.value_or(!hasLines() && hasLineIfEmpty() ? lineHeight() : inlineLayout.contentLogicalHeight());
    auto borderBoxLogicalHeight = handleAfterSideOfBlock(marginInfo, contentBoxHeight);
    setLogicalHeight(borderBoxLogicalHeight);
    updateRepaintTopAndBottomAfterLayout(relayoutChildren, partialRepaintRect, oldContentTopAndBottomIncludingInkOverflow, repaintLogicalTop, repaintLogicalBottom);

    if (CheckedPtr cache = protectedDocument()->existingAXObjectCache())
        cache->onLaidOutInlineContent(*this);
}

void RenderBlockFlow::setStaticPositionsForSimpleOutOfFlowContent()
{
    ASSERT(childrenInline());
#ifndef NDEBUG
    ASSERT(!hasLineIfEmpty());
    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        if (walker.current()->style().isDisplayInlineType()) {
            ASSERT(hasSimpleStaticPositionForInlineLevelOutOfFlowChildrenByStyle(style()));
            break;
        }
    }
#endif
    // We have nothing but out-of-flow boxes so we don't need to run the actual line layout.
    // Instead, we can just set the static positions to the point where all these boxes would end up.
    // This is a common case when using transforms to animate positioned boxes.
    auto staticPosition = LayoutPoint { borderAndPaddingStart(), borderAndPaddingBefore() };

    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto& renderer = downcast<RenderBox>(*walker.current());
        auto& layer = *renderer.layer();

        ASSERT(renderer.isOutOfFlowPositioned());

        auto previousStaticPosition = LayoutPoint { layer.staticInlinePosition(), layer.staticBlockPosition() };
        auto delta = staticPosition - previousStaticPosition;
        auto hasStaticInlinePositioning = renderer.style().hasStaticInlinePosition(isHorizontalWritingMode());

        layer.setStaticInlinePosition(staticPosition.x());
        layer.setStaticBlockPosition(staticPosition.y());

        if (!delta.isZero() && hasStaticInlinePositioning)
            renderer.setChildNeedsLayout(MarkOnlyThis);
    }
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
    if (auto* inlineLayout = this->inlineLayout()) {
        inlineLayout->outputLineTree(stream, depth);
        return;
    }
    if (auto* root = legacyRootBox())
        root->outputLineTreeAndMark(stream, markedBox, depth);
}
#endif

RenderBlockFlowRareData& RenderBlockFlow::ensureRareBlockFlowData()
{
    if (hasRareBlockFlowData())
        return *m_rareBlockFlowData;
    materializeRareBlockFlowData();
    return *m_rareBlockFlowData;
}

void RenderBlockFlow::materializeRareBlockFlowData()
{
    ASSERT(!hasRareBlockFlowData());
    m_rareBlockFlowData = makeUnique<RenderBlockFlowRareData>(*this);
}

#if ENABLE(TEXT_AUTOSIZING)

static inline bool isVisibleRenderText(const RenderObject& renderer)
{
    auto* renderText = dynamicDowncast<RenderText>(renderer);
    if (!renderText)
        return false;

    return !renderText->linesBoundingBox().isEmpty() && !renderText->text().containsOnly<isASCIIWhitespace>();
}

static inline bool resizeTextPermitted(const RenderObject& renderer)
{
    // We disallow resizing for text input fields and textarea to address <rdar://problem/5792987> and <rdar://problem/8021123>
    for (auto* ancestor = renderer.parent(); ancestor; ancestor = ancestor->parent()) {
        // Get the first non-shadow HTMLElement and see if it's an input.
        if (RefPtr element = dynamicDowncast<HTMLElement>(ancestor->element()); element && !element->isInShadowTree())
            return !is<HTMLInputElement>(*element) && !is<HTMLTextAreaElement>(*element);
    }
    return true;
}

static bool isNonBlocksOrNonFixedHeightListItems(const RenderObject& renderer)
{
    if (!renderer.isRenderBlock())
        return true;
    if (CheckedPtr renderListItem = dynamicDowncast<RenderListItem>(renderer))
        return !renderListItem->style().height().isFixed();
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
        if (style().usedVisibility() != Visibility::Visible)
            lineCount = NO_LINE;
        else {
            size_t lineCountInBlock = 0;
            if (childrenInline())
                lineCountInBlock = this->lineCount();
            else {
                for (auto& listItem : childrenOfType<RenderListItem>(*this)) {
                    if (!listItem.childrenInline() || listItem.style().usedVisibility() != Visibility::Visible)
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
                protectedDocument()->textAutoSizing().addTextNode(*text.protectedTextNode(), candidateNewSize);
        }

        descendant = RenderObjectTraversal::nextSkippingChildren(text, this);
    }
}

#endif // ENABLE(TEXT_AUTOSIZING)

void RenderBlockFlow::layoutExcludedChildren(RelayoutChildren relayoutChildren)
{
    RenderBlock::layoutExcludedChildren(relayoutChildren);

    auto* fragmentedFlow = multiColumnFlow();
    if (!fragmentedFlow)
        return;

    fragmentedFlow->setIsExcludedFromNormalLayout(true);

    setLogicalTopForChild(*fragmentedFlow, borderAndPaddingBefore());

    if (relayoutChildren == RelayoutChildren::Yes)
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

void RenderBlockFlow::checkForPaginationLogicalHeightChange(RelayoutChildren& relayoutChildren, LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged)
{
    // If we don't use columns or flow threads, then bail.
    if (!isRenderFragmentedFlow() && !multiColumnFlow())
        return;
    
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    if (RenderMultiColumnFlow* fragmentedFlow = multiColumnFlow()) {
        LayoutUnit newColumnHeight;
        if (hasDefiniteLogicalHeight() || view().frameView().pagination().mode != Pagination::Mode::Unpaginated) {
            auto computedValues = computeLogicalHeight(0_lu, logicalTop());
            newColumnHeight = std::max<LayoutUnit>(computedValues.m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight(), 0);
            if (fragmentedFlow->columnHeightAvailable() != newColumnHeight)
                relayoutChildren = RelayoutChildren::Yes;
        }
        fragmentedFlow->setColumnHeightAvailable(newColumnHeight);
    } else if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*this)) {
        // FIXME: This is a hack to always make sure we have a page logical height, if said height
        // is known. The page logical height thing in RenderLayoutState is meaningless for flow
        // thread-based pagination (page height isn't necessarily uniform throughout the flow
        // thread), but as long as it is used universally as a means to determine whether page
        // height is known or not, we need this. Page height is unknown when column balancing is
        // enabled and flow thread height is still unknown (i.e. during the first layout pass). When
        // it's unknown, we need to prevent the pagination code from assuming page breaks everywhere
        // and thereby eating every top margin. It should be trivial to clean up and get rid of this
        // hack once the old multicol implementation is gone (see also RenderView::pushLayoutStateForPagination).
        pageLogicalHeight = fragmentedFlow->isPageLogicalHeightKnown() ? 1_lu : 0_lu;

        pageLogicalHeightChanged = fragmentedFlow->pageLogicalSizeChanged();
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
        setNeedsLayoutAndPreferredWidthsUpdate();
}

LayoutUnit RenderBlockFlow::computedColumnWidth() const
{
    if (multiColumnFlow())
        return multiColumnFlow()->computedColumnWidth();
    return contentBoxLogicalWidth();
}

unsigned RenderBlockFlow::computedColumnCount() const
{
    if (multiColumnFlow())
        return multiColumnFlow()->computedColumnCount();
    
    return 1;
}

LayoutOptionalOutsets RenderBlockFlow::allowedLayoutOverflow() const
{
    LayoutOptionalOutsets allowance = RenderBox::allowedLayoutOverflow();

    if (!style().alignContent().isNormal()) {
        if (hasRareBlockFlowData()) {
            if (isHorizontalWritingMode())
                allowance.setTop(-rareBlockFlowData()->m_alignContentShift);
            else
                allowance.setLeft(-rareBlockFlowData()->m_alignContentShift);
        }
    }

    if (multiColumnFlow() && style().columnProgression() != ColumnProgression::Normal) {
        if (isHorizontalWritingMode() ^ !style().hasInlineColumnAxis())
            allowance = allowance.xFlippedCopy();
        else
            allowance = allowance.yFlippedCopy();
    }

    return allowance;
}

struct InlineMinMaxIterator {
// InlineMinMaxIterator is a class that will iterate over all render objects that contribute to
//   inline min/max width calculations. Note the following about the way it walks:
//   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
//   (2) We do not drill into the children of floats or replaced elements, since you can't break
//       in the middle of such an element.
//   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
//       distinct borders/margin/padding that contribute to the min/max width.
    InlineMinMaxIterator(const RenderBlockFlow& blockContainer)
        : m_blockContainer(blockContainer)
        {
        }

    RenderObject* next();
    bool isEndOfInline() const { return m_isEndOfInline; }

private:
    const RenderBlockFlow& m_blockContainer;

    RenderObject* m_current { nullptr };
    bool m_isEndOfInline { false };
    bool m_initial { true };
};

RenderObject* InlineMinMaxIterator::next()
{
    RenderObject* candidate = m_initial ? m_blockContainer.firstChild() : nullptr;
    m_initial = false;

    bool oldEndOfInline = m_isEndOfInline;
    m_isEndOfInline = false;
    do {

        if (!oldEndOfInline && is<RenderInline>(m_current))
            candidate = m_current->firstChildSlow();

        if (!candidate) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && m_current && m_current->isRenderInline()) {
                candidate = m_current;
                m_isEndOfInline = true;
                break;
            }

            while (m_current && m_current != &m_blockContainer) {
                candidate = m_current->nextSibling();
                if (candidate)
                    break;
                m_current = m_current->parent();
                if (m_current && m_current != &m_blockContainer && m_current->isRenderInline()) {
                    candidate = m_current;
                    m_isEndOfInline = true;
                    break;
                }
            }
        }

        if (!candidate)
            break;

        if (candidate->isOutOfFlowPositioned()) {
            m_current = candidate;
            candidate = nullptr;
            continue;
        }

        if (is<RenderInline>(*candidate) || candidate->isRenderTextOrLineBreak() || candidate->isFloating() || candidate->isBlockLevelReplacedOrAtomicInline())
            break;

        if (candidate->style().isDisplayBlockLevel()) {
            ASSERT(candidate->settings().blocksInInlineLayoutEnabled());
            break;
        }

        ASSERT_NOT_REACHED();
        m_current = candidate;
        candidate = nullptr;
    } while (m_current || m_current == &m_blockContainer);
    // Update our position.
    m_current = candidate;
    return candidate;
}

template <typename SizeType>
auto borderMarginOrPaddingWidth(LayoutUnit childValue, const SizeType& marginOrPadding, const Style::ZoomFactor& zoomFactor) -> LayoutUnit {
    if (auto fixed = marginOrPadding.tryFixed())
        return LayoutUnit(fixed->resolveZoom(zoomFactor));
    if constexpr (std::same_as<SizeType, Style::MarginEdge>) {
        if (marginOrPadding.isAuto())
            return { };
    }
    return childValue;
};

static LayoutUnit getBorderPaddingMargin(const RenderBoxModelObject& child, bool endOfInline)
{
    auto& childStyle = child.style();
    const auto& childZoomFactor = childStyle.usedZoomForLength();

    if (endOfInline) {
        return borderMarginOrPaddingWidth(child.marginEnd(), childStyle.marginEnd(), childZoomFactor) +
            borderMarginOrPaddingWidth(child.paddingEnd(), childStyle.paddingEnd(), childZoomFactor) +
            child.borderEnd();
    }
    return borderMarginOrPaddingWidth(child.marginStart(), childStyle.marginStart(), childZoomFactor) +
        borderMarginOrPaddingWidth(child.paddingStart(), childStyle.paddingStart(), childZoomFactor) +
        child.borderStart();
}

static inline void stripTrailingSpace(float& inlineMax, float& inlineMin, RenderObject* trailingSpaceChild)
{
    if (auto* renderText = dynamicDowncast<RenderText>(trailingSpaceChild)) {
        // Collapse away the trailing space at the end of a block.
        const char16_t space = ' ';
        const FontCascade& font = renderText->style().fontCascade(); // FIXME: This ignores first-line.
        float spaceWidth = font.width(RenderBlock::constructTextRun(span(space), renderText->style()));
        inlineMax -= spaceWidth + font.wordSpacing();
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

static inline std::optional<std::pair<const RenderText&, const RenderText&>> trailingRubyBaseAndAdjacentTextContent(const RenderInline& rubyBase, const RenderBlockFlow& blockContainer)
{
    // This functions returns adjacent _content_ renderers by skipping non-inline content (floats, out-of-flow content) inline boxes and related annotation boxes.
    // e.g. <ruby>
    //       <span>base</span><rt>annotation</rt>
    //       <span>adjacent base</span><rt>annotation</rt>
    //      </ruby>
    // returns "base" and "adjacent base" RenderText renderers.
    if (!rubyBase.firstInFlowChild())
        return { };

    auto shouldSkip = [&](auto& renderer) {
        if (is<RenderText>(renderer))
            return false;
        if (is<RenderInline>(renderer))
            return true;
        auto& renderBox = downcast<RenderBoxModelObject>(renderer);
        return !renderBox.isInFlow() || renderBox.style().display() == DisplayType::RubyAnnotation;
    };

    auto walker = InlineWalker(blockContainer, rubyBase.firstInFlowChild());
    auto lastInlineChildOfRubyBase = [&]() -> RenderObject* {
        RenderObject* lastChild = nullptr;
        for (; !walker.atEnd(); walker.advance()) {
            auto* renderer = walker.current();
            if (renderer->parent() == rubyBase.parent())
                return lastChild;
            if (!shouldSkip(*renderer))
                lastChild = renderer;
        }
        return { };
    };
    auto* lastCHild = lastInlineChildOfRubyBase();
    if (!lastCHild || !is<RenderText>(*lastCHild))
        return { };

    auto firstInlineAfterRubyBase = [&]() -> RenderObject* {
        for (; !walker.atEnd(); walker.advance()) {
            if (!shouldSkip(*walker.current()))
                return walker.current();
        }
        return { };
    };
    auto* firstSibling = firstInlineAfterRubyBase();
    if (!firstSibling || !is<RenderText>(*firstSibling))
        return { };

    return { std::pair<const RenderText&, const RenderText&> { downcast<RenderText>(*lastCHild), downcast<RenderText>(*firstSibling) } };
}

static inline bool hasTrailingSoftWrapOpportunity(const RenderInline& rubyBase, const RenderBlockFlow& blockContainer)
{
    if (!rubyBase.parent()->style().autoWrap())
        return false;

    if (auto lastAndNextTextContent = trailingRubyBaseAndAdjacentTextContent(rubyBase, blockContainer))
        return Layout::TextUtil::mayBreakInBetween(lastAndNextTextContent->first.text(), lastAndNextTextContent->first.style(), lastAndNextTextContent->second.text(), lastAndNextTextContent->second.style());
    return false;
}

static inline LayoutUnit preferredWidth(LayoutUnit preferredWidth, float result)
{
    return std::max(preferredWidth, LayoutUnit::fromFloatCeil(result));
}

static inline std::optional<LayoutUnit> textIndentForBlockContainer(const RenderBlockFlow& renderer)
{
    auto& style = renderer.style();
    if (auto fixedTextIndent = style.textIndent().length.tryFixed())
        return !fixedTextIndent->isZero() ? std::make_optional(LayoutUnit { fixedTextIndent->resolveZoom(style.usedZoomForLength()) }) : std::nullopt;

    auto indentValue = LayoutUnit { };
    if (auto* containingBlock = renderer.containingBlock()) {
        if (auto containingBlockFixedLogicalWidth = containingBlock->style().logicalWidth().tryFixed()) {
            auto containingBlockFixedLogicalWidthValue = Style::evaluate<LayoutUnit>(*containingBlockFixedLogicalWidth, containingBlock->style().usedZoomForLength());
            // At this point of the shrink-to-fit computation, we don't have a used value for the containing block width
            // (that's exactly to what we try to contribute here) unless the computed value is fixed.
            indentValue = Style::evaluate<LayoutUnit>(style.textIndent().length, containingBlockFixedLogicalWidthValue, containingBlock->style().usedZoomForLength());
        }
    }
    return indentValue ? std::make_optional(indentValue) : std::nullopt;
}

void RenderBlockFlow::computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!shouldApplyInlineSizeContainment());

    if (const_cast<RenderBlockFlow&>(*this).tryComputePreferredWidthsUsingInlinePath(minLogicalWidth, maxLogicalWidth))
        return;

    float inlineMax = 0.f;
    float inlineMin = 0.f;

    const RenderStyle& styleToUse = style();
    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = nullptr;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific cirucumstances (in order to match common WinIE renderings). 
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.) 
    bool allowImagesToBreak = !document().inQuirksMode() || !isRenderTableCell() || !styleToUse.logicalWidth().isIntrinsicOrLegacyIntrinsicOrAuto();

    bool oldAutoWrap = styleToUse.autoWrap();

    InlineMinMaxIterator childIterator(*this);

    // Signals the text indent was more negative than the min preferred width
    auto remainingNegativeTextIndent = std::optional<LayoutUnit> { };
    auto textIndentForMinimum = textIndentForBlockContainer(*this);
    auto textIndentForMaximum = textIndentForMinimum;
    CheckedPtr<RenderBox> previousFloat;
    bool isPrevChildInlineFlow = false;
    bool shouldBreakLineAfterText = false;
    bool canHangPunctuationAtStart = styleToUse.hangingPunctuation().contains(Style::HangingPunctuationValue::First);
    bool canHangPunctuationAtEnd = styleToUse.hangingPunctuation().contains(Style::HangingPunctuationValue::Last);
    RenderText* lastText = nullptr;
    struct RubyBaseContent {
        float minimumWidth { 0.f };
        float maxiumumWidth { 0.f };
        bool hasBreakingPositionAfter { false };
    };
    Vector<RubyBaseContent> rubyBaseContentStack;

    bool addedStartPunctuationHang = false;
    
    while (RenderObject* child = childIterator.next()) {
        // Interlinear annotations don't participate in inline layout, but they put a minimum width requirement on the associated ruby base.
        auto isInterlinearTypeAnnotation = [&] {
            if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(*child)) {
                auto& style = renderBlock->style();
                return style.display() == DisplayType::RubyAnnotation && (!style.isInterCharacterRubyPosition() || styleToUse.writingMode().isVerticalTypographic());
            }
            return false;
        };
        if (isInterlinearTypeAnnotation()) {
            auto annotationMinimumIntrinsicWidth = LayoutUnit { };
            auto annotationMaximumIntrinsicWidth = LayoutUnit { };
            computeChildPreferredLogicalWidths(downcast<RenderBlock>(*child), annotationMinimumIntrinsicWidth, annotationMaximumIntrinsicWidth);

            if (!rubyBaseContentStack.isEmpty()) {
                // Annotation box is always preceded by the associated ruby base.
                // inlineMin/max only gets expanded if the annotation is wider than the base content is.
                auto baseContent = rubyBaseContentStack.takeLast();
                inlineMax += std::max(0.f, annotationMaximumIntrinsicWidth.ceilToFloat() - baseContent.maxiumumWidth);
                if (baseContent.hasBreakingPositionAfter) {
                    // When base end has breaking position, the inlineMin value is already reset as we are not tracking the inline content for this "line" anymore.
                    // However the annotation still belows to the current "line" so we have to update the minLogicalWidth in case annotation is wider than the base content.
                    minLogicalWidth += std::max(0.f, annotationMinimumIntrinsicWidth.ceilToFloat() - baseContent.minimumWidth);
                } else
                    inlineMin += std::max(0.f, annotationMinimumIntrinsicWidth.ceilToFloat() - baseContent.minimumWidth);
            } else
                ASSERT_NOT_REACHED();
            continue;
        }

        auto resetLineForForcedLineBreak = [&] {
            if (styleToUse.collapseWhiteSpace())
                stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);
            minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
            maxLogicalWidth = preferredWidth(maxLogicalWidth, inlineMax);
            inlineMin = 0;
            inlineMax = 0;
            stripFrontSpaces = true;
            trailingSpaceChild = 0;
            textIndentForMinimum = { };
            textIndentForMaximum = { };
            remainingNegativeTextIndent = { };
            addedStartPunctuationHang = true;
            isPrevChildInlineFlow = false;
            oldAutoWrap = child->parent()->style().autoWrap();
        };

        if (child->isBR()) {
            resetLineForForcedLineBreak();
            continue;
        }

        if (child->style().isDisplayBlockLevel() && !child->isFloating() && is<RenderBox>(*child)) {
            ASSERT(settings().blocksInInlineLayoutEnabled());

            resetLineForForcedLineBreak();

            auto blockMinWidth = LayoutUnit { };
            auto blocMaxWidth = LayoutUnit { };
            computeChildPreferredLogicalWidths(downcast<RenderBox>(*child), blockMinWidth, blocMaxWidth);

            minLogicalWidth = std::max(minLogicalWidth, blockMinWidth);
            maxLogicalWidth = std::max(maxLogicalWidth, blocMaxWidth);
            continue;
        }

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
        auto autoWrap = child->isBlockLevelReplacedOrAtomicInline() || is<RenderText>(*child) ? child->parent()->style().autoWrap() : child->style().autoWrap();
        auto childMin = 0.f;
        auto childMax = 0.f;

        if (!child->isRenderText()) {
            if (child->isLineBreakOpportunity()) {
                minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                inlineMin = 0;
                continue;
            }
            auto& childStyle = downcast<RenderElement>(*child).style();
            // Case (1) and (2). Inline replaced and inline flow elements.
            if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(*child)) {
                // Add in padding/border/margin from the appropriate side of
                // the element.
                float bpm = getBorderPaddingMargin(*renderInline, childIterator.isEndOfInline());
                childMin += bpm;
                childMax += bpm;

                if (childStyle.display() == DisplayType::RubyBase && !childIterator.isEndOfInline())
                    rubyBaseContentStack.append({ inlineMin, inlineMax, false });

                inlineMin += childMin;
                inlineMax += childMax;

                if (childStyle.display() == DisplayType::RubyBase && childIterator.isEndOfInline()) {
                    if (!rubyBaseContentStack.isEmpty()) {
                        auto rubyBaseStart = rubyBaseContentStack.last();
                        auto baseHasBreakingPositionAfter = hasTrailingSoftWrapOpportunity(*renderInline, *this);
                        rubyBaseContentStack.last() = RubyBaseContent { inlineMin - rubyBaseStart.minimumWidth, inlineMax - rubyBaseStart.maxiumumWidth, baseHasBreakingPositionAfter };
                        if (baseHasBreakingPositionAfter) {
                            // Let's mark based end as a breaking opportunity. Note that annotation may chage the final value of minLogicalWidth.
                            minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                            inlineMin = 0;
                        }
                    } else
                        ASSERT_NOT_REACHED();
                }

                child->clearNeedsPreferredWidthsUpdate();
            } else {
                const auto& childZoomFactor = childStyle.usedZoomForLength();

                // Inline replaced boxes add in their margins to their min/max values.
                if (!child->isFloating())
                    lastText = nullptr;
                LayoutUnit margins;
                if (auto fixedMarginStart = childStyle.marginStart(writingMode()).tryFixed())
                    margins += LayoutUnit::fromFloatCeil(fixedMarginStart->resolveZoom(childZoomFactor));
                if (auto fixedMarginEnd = childStyle.marginEnd(writingMode()).tryFixed())
                    margins += LayoutUnit::fromFloatCeil(fixedMarginEnd->resolveZoom(childZoomFactor));
                childMin += margins.ceilToFloat();
                childMax += margins.ceilToFloat();
            }
        }

        if (!is<RenderInline>(*child) && !is<RenderText>(*child)) {
            // Case (2). Inline replaced boxes and floats.
            // Terminate the current line as far as minwidth is concerned.
            LayoutUnit childMinPreferredLogicalWidth;
            LayoutUnit childMaxPreferredLogicalWidth;
            CheckedPtr box = dynamicDowncast<RenderBox>(*child);
            if (box->isHorizontalWritingMode() != isHorizontalWritingMode()) {
                auto extent = box->computeLogicalHeight(box->borderAndPaddingLogicalHeight(), 0).m_extent;
                childMinPreferredLogicalWidth = extent;
                childMaxPreferredLogicalWidth = extent;
            } else
                computeChildPreferredLogicalWidths(*box, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);
            childMin += childMinPreferredLogicalWidth.ceilToFloat();
            childMax += childMaxPreferredLogicalWidth.ceilToFloat();

            bool clearPreviousFloat = false;
            if (box->isFloating()) {
                auto childClearValue = RenderStyle::usedClear(*box);
                if (previousFloat) {
                    auto previousFloatValue = RenderStyle::usedFloat(*previousFloat);
                    clearPreviousFloat =
                        (previousFloatValue == UsedFloat::Left && (childClearValue == UsedClear::Left || childClearValue == UsedClear::Both))
                        || (previousFloatValue == UsedFloat::Right && (childClearValue == UsedClear::Right || childClearValue == UsedClear::Both));
                }
                previousFloat = box;
            }

            bool canBreakReplacedElement = !box->isImage() || allowImagesToBreak;
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
            if (!box->isFloating()) {
                if (textIndentForMinimum) {
                    childMin += LayoutUnit { textIndentForMinimum->ceilToFloat() };
                    textIndentForMinimum = childMin < 0 ? std::make_optional(LayoutUnit::fromFloatCeil(childMin)) : std::nullopt;
                }

                if (textIndentForMaximum) {
                    childMax += LayoutUnit { textIndentForMaximum->ceilToFloat() };
                    textIndentForMaximum = childMax < 0 ? std::make_optional(LayoutUnit::fromFloatCeil(childMax)) : std::nullopt;
                }
            }

            if (canHangPunctuationAtStart && !addedStartPunctuationHang && !box->isFloating())
                addedStartPunctuationHang = true;

            // Add our width to the max.
            inlineMax += std::max<float>(0, childMax);

            if ((!autoWrap || !canBreakReplacedElement || (isPrevChildInlineFlow && !shouldBreakLineAfterText))) {
                if (box->isFloating())
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
            if (!box->isFloating()) {
                stripFrontSpaces = false;
                trailingSpaceChild = nullptr;
                lastText = nullptr;
            }
        } else if (CheckedPtr renderText = dynamicDowncast<RenderText>(*child)) {
            if (renderText->style().hasTextCombine()) {
                if (CheckedPtr renderCombineText = dynamicDowncast<RenderCombineText>(*renderText))
                    renderCombineText->combineTextIfNeeded();
            }

            // Determine if we have a breakable character. Pass in
            // whether or not we should ignore any spaces at the front
            // of the string. If those are going to be stripped out,
            // then they shouldn't be considered in the breakable char
            // check.
            bool strippingBeginWS = stripFrontSpaces;
            auto widths = renderText->trimmedPreferredWidths(inlineMax, stripFrontSpaces);

            childMin = widths.min;
            childMax = widths.max;

            // This text object will not be rendered, but it may still provide a breaking opportunity.
            if (!widths.hasBreak && !childMax) {
                if (autoWrap && (widths.beginWS || widths.endWS || widths.endZeroSpace)) {
                    minLogicalWidth = preferredWidth(minLogicalWidth, inlineMin);
                    inlineMin = 0;
                }
                continue;
            }

            lastText = renderText.get();

            if (stripFrontSpaces)
                trailingSpaceChild = child;
            else
                trailingSpaceChild = 0;

            // Add in text-indent. This is added in only once.
            float ti = 0.f;
            if (textIndentForMinimum || remainingNegativeTextIndent) {
                ti = (textIndentForMinimum ? *textIndentForMinimum : *remainingNegativeTextIndent).ceilToFloat();
                childMin += ti;
                widths.beginMin += ti;
                // It the text indent negative and larger than the child minimum, we re-use the remainder
                // in future minimum calculations, but using the negative value again on the maximum
                // will lead to under-counting the max pref width.
                textIndentForMinimum = { };
                remainingNegativeTextIndent = childMin < 0 ? std::make_optional(childMin) : std::nullopt;
            }

            if (textIndentForMaximum) {
                auto textIndent = textIndentForMaximum->ceilToFloat();
                childMax += textIndent;
                widths.beginMax += textIndent;
                textIndentForMaximum = { };
            }

            // See if we have a hanging punctuation situation at the start.
            if (canHangPunctuationAtStart && !addedStartPunctuationHang) {
                unsigned startIndex = strippingBeginWS ? renderText->firstCharacterIndexStrippingSpaces() : 0;
                float hangStartWidth = renderText->hangablePunctuationStartWidth(startIndex);
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

                if (widths.endWS || widths.endZeroSpace) {
                    // We end in breakable space, which means we can end our current line.
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
                textIndentForMinimum = { };
                textIndentForMaximum = { };
                remainingNegativeTextIndent = { };
                addedStartPunctuationHang = true;
                if (widths.endsWithBreak)
                    stripFrontSpaces = true;

            } else
                inlineMax += std::max<float>(0, childMax);
        }

        // Ignore spaces after a list marker.
        if (child->isRenderListMarker())
            stripFrontSpaces = true;

        isPrevChildInlineFlow = !child->isRenderText() && child->isRenderInline();
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

bool RenderBlockFlow::tryComputePreferredWidthsUsingInlinePath(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth)
{
    if (!firstInFlowChild())
        return false;

    computeAndSetLineLayoutPath();

    if (lineLayoutPath() != InlinePath)
        return false;

    if (!LayoutIntegration::LineLayout::canUseForPreferredWidthComputation(*this))
        return false;

    if (!inlineLayout())
        m_lineLayout = makeUnique<LayoutIntegration::LineLayout>(*this);

    std::tie(minLogicalWidth, maxLogicalWidth) = inlineLayout()->computeIntrinsicWidthConstraints();
    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto* renderer = walker.current();
        renderer->clearNeedsPreferredWidthsUpdate();
        if (auto* renderText = dynamicDowncast<RenderText>(renderer))
            renderText->resetMinMaxWidth();
    }
    return true;
}

}
// namespace WebCore
