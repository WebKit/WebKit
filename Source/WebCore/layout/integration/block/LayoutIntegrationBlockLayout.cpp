/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutIntegrationBlockLayout.h"

#include "AXObjectCache.h"
#include "AnchorPositionEvaluator.h"
#include "BlockFormattingContext.h"
#include "BlockFormattingUtils.h"
#include "BlockIntegrationUtils.h"
#include "FlexFormattingUtils.h"
#include "FloatingObjects.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutRepainter.h"
#include "LineClampUpdater.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "Logging.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ScrollbarUpdateScope.h"
#include "SubtreeScrollbarChangesState.h"
#include "TextBoxTrimmer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace LayoutIntegration {

BlockLayout::BlockLayout(RenderBlockFlow& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
}

// The block size the percentage block sizes of the children of the root resolve against (RenderBox::computePercentageLogicalHeight). In
// quirks mode that is past the containing blocks with an auto block size, without the margins, borders and padding of the body and the
// root element (quirks §3.5).
static std::optional<LayoutUnit> contentBoxBlockSizeForPercentages(const RenderBlockFlow& rootRenderer)
{
    // A table cell gives its children the block size the row gives it, once it has one, not its own (RenderTableSection::relayoutCellIfFlexed).
    if (CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(rootRenderer)) {
        if (tableCell->style().logicalHeight().isPercentOrCalculated() && tableCell->table()->style().logicalHeight().isAuto())
            return { };
        return tableCell->overridingBorderBoxLogicalHeight().transform([&](auto borderBoxBlockSize) {
            return borderBoxBlockSize - tableCell->computedCSSPaddingBefore() - tableCell->computedCSSPaddingAfter() - tableCell->borderLogicalHeight() - tableCell->scrollbarLogicalHeight();
        });
    }
    auto isPerpendicular = [&](const RenderBlock& containingBlock) {
        return rootRenderer.isHorizontalWritingMode() != containingBlock.isHorizontalWritingMode();
    };
    if (!rootRenderer.skipContainingBlockForPercentHeightCalculation(rootRenderer, false))
        return rootRenderer.availableLogicalHeightForPercentageComputation();
    CheckedPtr<const RenderBox> containingBlockChild = &rootRenderer;
    CheckedPtr<const RenderBlock> containingBlock = &rootRenderer;
    auto skippedMarginsBordersAndPadding = 0_lu;
    while (containingBlock && !is<RenderView>(*containingBlock) && rootRenderer.skipContainingBlockForPercentHeightCalculation(*containingBlock, isPerpendicular(*containingBlock))) {
        if (containingBlock->isBody() || containingBlock->isDocumentElementRenderer()) {
            auto writingMode = containingBlock->writingMode();
            skippedMarginsBordersAndPadding += containingBlock->marginBefore(writingMode) + containingBlock->marginAfter(writingMode) + containingBlock->borderAndPaddingLogicalHeight();
        }
        containingBlockChild = containingBlock;
        containingBlock = containingBlock->containingBlock();
    }
    if (!containingBlock || is<RenderTableCell>(*containingBlock))
        return { };
    if (is<RenderView>(*containingBlock) && rootRenderer.view().frameView().isAutoSizeEnabled())
        return { };
    auto blockSize = isPerpendicular(*containingBlock) ? std::optional { containingBlockChild->containingBlockLogicalWidthForContent() } : containingBlock->availableLogicalHeightForPercentageComputation();
    // A flex container stretching its items knows its block size before it has one (RenderFlexibleBox::isInCrossAxisStretchLayout).
    if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(*containingBlock); !blockSize && flexContainer && flexContainer->isInCrossAxisStretchLayout())
        blockSize = flexContainer->contentBoxLogicalHeight();
    return blockSize.transform([&](auto blockSize) {
        return std::max(0_lu, blockSize - skippedMarginsBordersAndPadding);
    });
}

static Layout::ConstraintsForBlockContent constraintsForBlockContent(const RenderBlockFlow& rootRenderer, RelayoutChildren relayoutChildren)
{
    // The root lives in its parent's formatting context, which has already decided its size. WebKit compatibility: the collapsed border halves
    // of a table cell are in the writing mode of the table (RenderTableCell::borderStart). Block content starts after those on the cell's own
    // inline-start and block-start sides (RenderBlockFlow::determineLogicalLeftPositionForChild), inline content after the physical ones on
    // those sides (BoxGeometryUpdater::setFormattingContextRootGeometry).
    auto writingMode = rootRenderer.writingMode();
    auto borderWidths = rootRenderer.borderWidths();
    auto borderStart = rootRenderer.childrenInline() ? borderWidths.start(writingMode) : rootRenderer.borderStart();
    auto borderBefore = rootRenderer.childrenInline() ? borderWidths.before(writingMode) : rootRenderer.borderBefore();
    auto contentBoxLogicalLeft = borderStart + rootRenderer.paddingStart();
    // The scrollbar in the inline axis sits between the border and the padding: the vertical one, on the left or on the right, in horizontal
    // writing modes and the horizontal one, at the bottom, in vertical writing modes. The block formatting context measures from the inline
    // start, so it counts when it is on that side, like RenderBlockFlow::determineLogicalLeftPositionForChild, which measures from the logical
    // left (top in vertical writing modes) and clamps it to the space it has.
    // With scrollbar-gutter: stable both-edges, a gutter is on that side either way.
    auto scrollbarIsOnLogicalLeft = writingMode.isHorizontal() && rootRenderer.shouldPlaceVerticalScrollbarOnLeft();
    if (rootRenderer.style().scrollbarGutter().isStableBothEdges() || scrollbarIsOnLogicalLeft == writingMode.isLogicalLeftInlineStart())
        contentBoxLogicalLeft += std::min<LayoutUnit>(rootRenderer.scrollbarLogicalWidth(), std::max(0_lu, rootRenderer.logicalWidth() - rootRenderer.borderAndPaddingLogicalWidth()));
    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLogicalLeft, rootRenderer.contentBoxLogicalWidth() };
    return { { horizontalConstraints, borderBefore + rootRenderer.paddingBefore() }, contentBoxBlockSizeForPercentages(rootRenderer), rootRenderer.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding),
        rootRenderer.availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding), relayoutChildren == RelayoutChildren::Yes };
}

// The last formatted line of a box with trim-end and block-level content, a block-level box on the last line of its inline content included,
// is only known once the content is laid out, which it is again with it (RenderBlockFlow::layoutInFlowChildren). That inline content is laid
// out again (TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd).
static Layout::BlockLayoutState::LastFormattedLineRoots lastFormattedLineRootsForTrimEnd(const RenderBlockFlow* rootRenderer, const Layout::BlockLayoutResult& result)
{
    auto lastFormattedLineRoots = Layout::BlockLayoutState::LastFormattedLineRoots { };
    auto addLastFormattedLineRoot = [&](const RenderBox& box) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
        if (!blockContainer)
            return;
        if (CheckedPtr lastFormattedLineRoot = TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd(*blockContainer)) {
            // The inline content of the block containers on the way, which may lay it out as a block-level box in it, is not kept either.
            for (CheckedPtr<RenderBlock> ancestor = lastFormattedLineRoot; ancestor && ancestor != blockContainer; ancestor = ancestor->containingBlock())
                ancestor->setNeedsLayout(MarkingBehavior::OnlyThis);
            SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBlockContainer = const_cast<RenderBlockFlow&>(*blockContainer);
            mutableBlockContainer->setNeedsLayout(MarkingBehavior::OnlyThis);
            lastFormattedLineRoots.append({ box, *lastFormattedLineRoot });
        }
    };
    if (rootRenderer)
        addLastFormattedLineRoot(*rootRenderer);
    for (auto& box : result.boxes) {
        if (!box.isLaidOutThroughRenderTree)
            addLastFormattedLineRoot(box.renderer);
    }
    return lastFormattedLineRoots;
}

void BlockLayout::layout(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    integrationUtils.setRootForScrollbarChanges(rootRenderer());
    // The line-clamp budget of the root's block formatting context: the root's LineClampUpdater sets it up and reads back what the content left
    // of it. The boxes in the content laid out through the render tree do not see it, since the lines of independent formatting contexts do not
    // count (css-overflow-4 §5.2).
    auto* renderTreeLayoutState = rootRenderer().view().frameView().layoutContext().layoutState();
    auto renderTreeLineClamp = renderTreeLayoutState ? renderTreeLayoutState->lineClamp() : std::nullopt;
    if (renderTreeLineClamp)
        renderTreeLayoutState->setLineClamp({ });
    // The text-box-trim the root's TextBoxTrimmer sets up for its content, which does not reach into the independent formatting contexts in it
    // either.
    CheckedRef layoutContext = rootRenderer().view().frameView().layoutContext();
    auto renderTreeTextBoxTrim = layoutContext->textBoxTrim();
    layoutContext->setTextBoxTrim({ });

    auto hasAutoScrollbarChanges = false;
    auto layoutContent = [&](Layout::BlockLayoutState::LastFormattedLineRoots&& lastFormattedLineRoots) {
        auto blockLayoutState = Layout::BlockLayoutState { { } };
        if (renderTreeLineClamp) {
            blockLayoutState.setLineClamp(Layout::BlockLayoutState::LineClamp { renderTreeLineClamp->maximumLines, renderTreeLineClamp->shouldDiscardOverflow, false, renderTreeLineClamp->isStableTextWrap, renderTreeLineClamp->hasConstrainedTextWrapContent });
            blockLayoutState.setClampAfterBox(renderTreeLineClamp->clampAfterBox.get());
        }
        if (renderTreeTextBoxTrim)
            blockLayoutState.setTextBoxTrimState({ renderTreeTextBoxTrim->trimFirstFormattedLine, renderTreeTextBoxTrim->lastFormattedLineRoot.get() });
        blockLayoutState.setLastFormattedLineRoots(WTF::move(lastFormattedLineRoots));
        if (CheckedPtr lastFormattedLineRoot = blockLayoutState.lastFormattedLineRoot(rootRenderer()))
            blockLayoutState.setTextBoxTrimState({ blockLayoutState.textBoxTrimState().trimsFirstFormattedLine, lastFormattedLineRoot.get() });
        auto initialBlockLayoutState = blockLayoutState;
        auto constraints = constraintsForBlockContent(rootRenderer(), relayoutChildren);
        auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layout(constraints, BlockIntegrationUtils::rootGeometry(rootRenderer()));
        // align-content moves the content as a unit in the block size the root gets from that of its content (RenderBlockFlow::layoutBlock),
        // where it is laid out again.
        if (!rootRenderer().style().alignContent().isNormal()) {
            auto intrinsicBorderBoxBlockSize = rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize;
            auto borderBoxBlockSize = rootRenderer().computeLogicalHeight(intrinsicBorderBoxBlockSize, rootRenderer().logicalTop()).extent;
            if (auto shift = Layout::BlockFormattingUtils::alignContentShift(rootRenderer(), borderBoxBlockSize - intrinsicBorderBoxBlockSize)) {
                blockLayoutState = WTF::move(initialBlockLayoutState);
                auto shiftedConstraints = Layout::ConstraintsForBlockContent { { constraints.horizontal(), constraints.logicalTop() + shift }, constraints.contentBoxBlockSize(), constraints.fillAvailableBlockSize(), constraints.replacedElementAvailableBlockSize(), constraints.relayoutChildren() };
                result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layout(shiftedConstraints, BlockIntegrationUtils::rootGeometry(rootRenderer()));
            }
        }
        LOG_WITH_STREAM(Layout, stream << "BlockLayout for root (" << &rootRenderer() << "): " << result);
        if (auto lineClamp = blockLayoutState.lineClamp())
            renderTreeLayoutState->setLineClamp(RenderLayoutState::LineClamp { lineClamp->maximumLines, lineClamp->shouldDiscardOverflow, blockLayoutState.clampAfterBox(), lineClamp->isStableTextWrap, lineClamp->hasConstrainedTextWrapContent });
        hasAutoScrollbarChanges |= updateRenderers(result, paintInvalidation);
        rootRenderer().m_floatingObjects = nullptr;
        if (blockLayoutState.placedFloats())
            layoutContext->addRootWithFloatsPlacedByBlockFormattingContext(rootRenderer());
        return result;
    };
    auto result = layoutContent({ });

    auto lastFormattedLineRoots = lastFormattedLineRootsForTrimEnd(&rootRenderer(), result);
    if (!lastFormattedLineRoots.isEmpty())
        result = layoutContent(Layout::BlockLayoutState::LastFormattedLineRoots { lastFormattedLineRoots });
    // RenderBlock::layout lays a scroll container out again, once, when its auto scrollbars come or go (RelayoutScopeForScrollbarChange). The
    // ones the block formatting context lays out take their new scrollbars when it lays out the content again, and the ones in it keep theirs
    // then. So do the boxes it gave a size from their content, which they change (SubtreeScrollbarChangesHandler).
    for (auto& change : integrationUtils.takeScrollbarChangesForContentSizes()) {
        if (change.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Block)) {
            change.renderer->setNeedsLayout(MarkingBehavior::OnlyThis);
            change.renderer->markContainingBlocksForLayout(&rootRenderer());
        }
        if (change.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Inline))
            change.renderer->invalidateContentLogicalWidths(MarkingBehavior::ContainingBlockChain, &rootRenderer());
        hasAutoScrollbarChanges = true;
    }
    if (hasAutoScrollbarChanges)
        result = layoutContent(WTF::move(lastFormattedLineRoots));
    layoutContext->setTextBoxTrim(renderTreeTextBoxTrim);

    // Like updateRenderers for the block containers in the root: the inline formatting context took the overflow of the block-level boxes in
    // the root's inline content from their renderers, before they had their new geometry.
    if (CheckedPtr lineLayout = rootRenderer().inlineLayout(); lineLayout && lineLayout->hasBlocks())
        lineLayout->updateOverflow();
    // RenderBlockFlow::layoutBlock resolves the used block size of the root from this.
    rootRenderer().setLogicalHeight(rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize);
    // Like RenderBlockFlow::layoutInlineContent for the inline content of the root.
    if (auto damagedInlineContent = paintInvalidation.damagedInlineContentLogicalTopAndBottom(rootRenderer(), rootRenderer().logicalHeight())) {
        repaintLogicalTop = damagedInlineContent->logicalTop;
        repaintLogicalBottom = damagedInlineContent->logicalBottom;
        if (CheckedPtr cache = protect(rootRenderer().document())->existingAXObjectCache())
            cache->onLaidOutInlineContent(rootRenderer());
    }
}

std::pair<LayoutUnit, LayoutUnit> BlockLayout::computeIntrinsicInlineSizes(const RenderBlockFlow& rootRenderer)
{
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    // A fieldset's legend, which sits in its border and not in its content, floors them (RenderBlock::computeBlockIntrinsicLogicalWidths). This
    // takes the legend out of the content too, like RenderBlock::layoutExcludedChildren.
    auto [legendMinimum, legendMaximum] = rootRenderer.computeIntrinsicLogicalWidthsForFieldsetLegend();
    auto sizes = Layout::BlockFormattingContext { rootRenderer, integrationUtils, blockLayoutState }.computeIntrinsicInlineSizes(rootRenderer, contentBoxBlockSizeForPercentages(rootRenderer));
    sizes.minimum = std::max(sizes.minimum, legendMinimum);
    sizes.maximum = std::max(sizes.maximum, legendMaximum);

    // The render tree stops invalidating intrinsic sizes at a box whose intrinsic sizes are already invalid, expecting its ancestors to be
    // invalid too. The block formatting context does not compute the render tree's intrinsic sizes of the boxes in the root, so those that
    // are invalid stay invalid, while the root's become valid: let invalidation go past them (RenderObject::invalidateContentLogicalWidths).
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableRootRenderer = const_cast<RenderBlockFlow&>(rootRenderer);
    for (CheckedRef descendant : descendantsOfType<RenderElement>(mutableRootRenderer.get())) {
        if (!descendant->hasInvalidContentLogicalWidths())
            continue;
        descendant->clearContentLogicalWidthsInvalidation();
        descendant->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);
    }
    return { sizes.minimum, sizes.maximum };
}

bool BlockLayout::contentDependsOnBlockSize(const RenderBlockFlow& rootRenderer)
{
    auto writingMode = rootRenderer.writingMode();
    // Percentage block sizes resolve against the content box of the root through inline boxes and anonymous block boxes (CSS 2.2 §9.2.1.1,
    // §10.5). Any other box gives its content a block size of its own, which depends on the root's only when it is a percentage itself.
    // Out-of-flow boxes resolve theirs against the padding box of their containing block, which is definite either way.
    auto hasPercentageBlockSizeContent = [&] {
        for (CheckedPtr<const RenderObject> descendant = rootRenderer.firstChild(); descendant;) {
            CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
            if (box && !box->isOutOfFlowPositioned() && Layout::BlockFormattingUtils::hasPercentageBlockSize(*box, writingMode))
                return true;
            auto passesBlockSizeThrough = descendant->isInlineBox() || (box && box->isAnonymousBlock());
            descendant = passesBlockSizeThrough ? descendant->nextInPreOrder(&rootRenderer) : descendant->nextInPreOrderAfterChildren(&rootRenderer);
        }
        return false;
    };
    return hasPercentageBlockSizeContent() && canUseForBlockLayout(rootRenderer);
}

void BlockLayout::prepareForRenderTreeLayout(RenderBlockFlow& rootRenderer)
{
    // The render tree's block layout takes the floats of the boxes it does not lay out again from its float lists, which the block formatting
    // context leaves empty (updateRenderers): it lays out all of the block formatting context the floats were placed in again.
    if (!rootRenderer.view().frameView().layoutContext().removeRootWithFloatsPlacedByBlockFormattingContext(rootRenderer))
        return;
    for (CheckedPtr<RenderObject> descendant = rootRenderer.firstChild(); descendant;) {
        if (CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*descendant); blockContainer && !blockContainer->isFloatingOrOutOfFlowPositioned() && !blockContainer->createsNewFormattingContext()) {
            blockContainer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
            descendant = descendant->nextInPreOrder(&rootRenderer);
            continue;
        }
        descendant = descendant->isInlineBox() ? descendant->nextInPreOrder(&rootRenderer) : descendant->nextInPreOrderAfterChildren(&rootRenderer);
    }
}

void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock)
{
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    auto paintInvalidation = PaintInvalidation { };
    layoutOutOfFlowBoxes(containingBlock, paintInvalidation);
}

// The root of the block formatting context a block container the block formatting context laid out takes part in.
static const RenderBlockFlow& blockFormattingContextRootOf(const RenderBlockFlow& blockContainer)
{
    if (blockContainer.createsNewFormattingContext())
        return blockContainer;
    CheckedPtr containingBlock = dynamicDowncast<RenderBlockFlow>(blockContainer.containingBlock());
    return containingBlock ? blockFormattingContextRootOf(*containingBlock) : blockContainer;
}

// The padding box of a block in the coordinates its children are positioned in (RenderBox::setLogicalLocation): from its left edge (top in
// vertical writing modes) in the inline axis, and from its block-start edge in the block axis, which is where the render tree measures block
// positions from in a flipped block flow direction too (RenderBox::blockStartRelativeToPhysicalForChild).
static LayoutRect logicalPaddingBoxRect(const RenderBox& box)
{
    auto writingMode = box.writingMode();
    auto paddingBox = box.paddingBoxRect();
    auto borderBoxSize = box.borderBoxSize();
    if (!writingMode.isHorizontal()) {
        paddingBox = paddingBox.transposedRect();
        borderBoxSize = borderBoxSize.transposedSize();
    }
    if (writingMode.isBlockFlipped())
        paddingBox.setY(borderBoxSize.height() - paddingBox.maxY());
    return paddingBox;
}

// How a flex container aligns an out-of-flow child in its content box in its inline and block axes, as if it were its only flex item
// (css-flexbox-1 §4.1): justify-content in the main axis and align-self in the cross axis, like FlexLayout::staticMainAxisPositionForPositionedFlexItem
// and staticCrossAxisPositionForPositionedFlexItem.
using StaticPositionAlignment = Layout::OutOfFlowGeometry::StaticPosition::Alignment;
static std::pair<StaticPositionAlignment, StaticPositionAlignment> flexStaticPositionAlignments(const RenderFlexibleBox& flexBox, const RenderBox& outOfFlowBox)
{
    using Position = StaticPositionAlignment::Position;
    CheckedRef style = flexBox.style();
    auto isColumn = style->isColumnFlexDirection();
    auto mainAxisAlignment = [&] -> StaticPositionAlignment {
        auto rectangleSize = isColumn ? flexBox.contentBoxLogicalHeight() : flexBox.contentBoxLogicalWidth();
        auto justifyContent = style->justifyContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior());
        // With no other flex items, space-around and space-evenly center it, overflowing or not.
        if (justifyContent.distribution() == ContentDistribution::SpaceAround || justifyContent.distribution() == ContentDistribution::SpaceEvenly)
            return { rectangleSize, Position::Center };
        // A reversed flex direction swaps the main-start and main-end edges, not start and end.
        auto isReverse = style->isReverseFlexDirection();
        auto isSafe = justifyContent.overflow() == OverflowAlignment::Safe;
        switch (FlexFormattingUtils::resolveLeftRightAlignment(justifyContent, style, isReverse)) {
        case ContentPosition::Center:
            return { rectangleSize, Position::Center, isSafe };
        case ContentPosition::Start:
            return { rectangleSize, Position::Start, isSafe };
        case ContentPosition::End:
            return { rectangleSize, Position::End, isSafe };
        case ContentPosition::FlexEnd:
            return { rectangleSize, isReverse ? Position::Start : Position::End, isSafe };
        default:
            return { rectangleSize, isReverse ? Position::End : Position::Start, isSafe };
        }
    };
    auto crossAxisAlignment = [&] -> StaticPositionAlignment {
        auto rectangleSize = isColumn ? flexBox.contentBoxLogicalWidth() : flexBox.contentBoxLogicalHeight();
        auto isSafe = outOfFlowBox.style().alignSelf().resolve(style.ptr()).overflow() == OverflowAlignment::Safe;
        // wrap-reverse makes the cross-start edge the end one, which alignmentForFlexItem accounts for in flex-start and flex-end.
        switch (FlexFormattingUtils::alignmentForFlexItem(outOfFlowBox)) {
        case ItemPosition::Center:
        case ItemPosition::AnchorCenter:
            return { rectangleSize, Position::Center, isSafe };
        case ItemPosition::FlexEnd:
        // Without a baseline-sharing group, last baseline alignment falls back to end alignment (css-align-3 §4.2).
        case ItemPosition::LastBaseline:
            return { rectangleSize, Position::End, isSafe };
        case ItemPosition::Stretch:
            return { rectangleSize, FlexFormattingUtils::isWrapReverse(flexBox) ? Position::End : Position::Start, isSafe };
        default:
            return { rectangleSize, Position::Start, isSafe };
        }
    };
    if (isColumn)
        return { crossAxisAlignment(), mainAxisAlignment() };
    return { mainAxisAlignment(), crossAxisAlignment() };
}

// The static position the in-flow layout of its parent gave the box (RenderLayer::staticInlinePosition, staticBlockPosition), relative to the
// border box of its block container, moved through the positions of the boxes in between, relative offsets included (like PositionedLayoutConstraints::computedInlineStaticDistance)
// to the padding box of its containing block. Coverage keeps those boxes in the containing block's writing mode.
static Layout::OutOfFlowGeometry::StaticPosition staticPosition(const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    CheckedPtr<const RenderElement> staticPositionBlock = outOfFlowBox.parent();
    while (staticPositionBlock->isInlineBox())
        staticPositionBlock = staticPositionBlock->parent();
    CheckedRef blockContainer = downcast<RenderBlock>(*staticPositionBlock);
    // A flex container's is the start of its content box, which the layer has until the render tree path aligns the box there
    // (FlexLayout::setStaticPositionForPositionedLayout).
    CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(blockContainer);
    CheckedRef layer = *outOfFlowBox.layer();
    auto staticInlinePosition = flexBox ? flexBox->startOffsetForContent() : layer->staticInlinePosition();
    auto staticBlockPosition = flexBox ? flexBox->borderAndPaddingBefore() : layer->staticBlockPosition();
    auto position = LayoutPoint { blockContainer->writingMode().isLogicalLeftInlineStart() ? staticInlinePosition : blockContainer->logicalWidth() - staticInlinePosition, staticBlockPosition };
    // An out-of-flow box between them is positioned in its own containing block, which is where container() goes, from the border box of the
    // first fragment of a positioned inline box (RenderBoxModelObject::blockStartRelativeFirstFragmentBorderBoxRect).
    for (CheckedPtr<const RenderElement> box = blockContainer.ptr(); box != &containingBlock; box = box->container()) {
        CheckedPtr renderBox = dynamicDowncast<RenderBox>(*box);
        if (!renderBox)
            continue;
        position.move(renderBox->logicalLeft(), renderBox->logicalTop());
        auto inFlowOffset = renderBox->offsetForInFlowPosition();
        position.move(renderBox->isHorizontalWritingMode() ? inFlowOffset : inFlowOffset.transposedSize());
        if (CheckedPtr inlineBox = renderBox->isOutOfFlowPositioned() ? dynamicDowncast<RenderBoxModelObject>(renderBox->container()) : nullptr; inlineBox && inlineBox->isInlineBox()) {
            auto topLeft = inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location();
            position.moveBy(inlineBox->writingMode().isHorizontal() ? topLeft : topLeft.transposedPoint());
        }
    }
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlinePosition = containingBlock.writingMode().isLogicalLeftInlineStart() ? position.x() - paddingBox.x() : paddingBox.maxX() - position.x();
    auto result = Layout::OutOfFlowGeometry::StaticPosition { inlinePosition, position.y() - paddingBox.y(), blockContainer->writingMode().isLogicalLeftInlineStart() != containingBlock.writingMode().isLogicalLeftInlineStart() };
    if (flexBox)
        std::tie(result.inlineAlignment, result.blockAlignment) = flexStaticPositionAlignments(*flexBox, outOfFlowBox);
    return result;
}

// The border box of the default anchor of a box that position-area or anchor-center positions against it, from the anchor's geometry in the
// render tree, which is committed by the time the containing block lays out its out-of-flow boxes (PositionedLayoutConstraints::captureAnchorGeometry).
static std::optional<Layout::OutOfFlowGeometry::DefaultAnchor> defaultAnchor(const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    if (!Style::AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(outOfFlowBox.style()))
        return { };
    CheckedPtr anchor = Style::AnchorPositionEvaluator::defaultAnchorForBox(outOfFlowBox);
    if (!anchor)
        return { };
    // Physical and relative to the border box of the containing block, scrolled to the start, but for a scrollbar on the left, which it takes
    // off for PositionedLayoutConstraints, whose containing block starts at the border.
    auto anchorRect = Style::AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(*anchor, containingBlock, outOfFlowBox);
    if (containingBlock.shouldPlaceVerticalScrollbarOnLeft())
        anchorRect.move(containingBlock.verticalScrollbarWidth(), 0);
    auto writingMode = containingBlock.writingMode();
    auto borderBoxSize = containingBlock.borderBoxSize();
    if (!writingMode.isHorizontal()) {
        anchorRect = anchorRect.transposedRect();
        borderBoxSize = borderBoxSize.transposedSize();
    }
    if (writingMode.isBlockFlipped())
        anchorRect.setY(borderBoxSize.height() - anchorRect.maxY());
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlineStart = writingMode.isLogicalLeftInlineStart() ? anchorRect.x() - paddingBox.x() : paddingBox.maxX() - anchorRect.maxX();
    return Layout::OutOfFlowGeometry::DefaultAnchor { inlineStart, anchorRect.width(), anchorRect.y() - paddingBox.y(), anchorRect.height() };
}

// The padding box of a scroll container extended to its scrollable overflow (css-position-4 §2.2), relative to its padding box. In the box's
// coordinates, which have the block start at the top or the left.
static std::optional<LayoutRect> scrollableContainingBlock(const RenderBlock& containingBlock)
{
    if (!containingBlock.hasRenderOverflow() || !containingBlock.hasPotentiallyScrollableOverflow())
        return { };
    auto rect = containingBlock.scrollablePaddingAreaOverflowRect();
    auto writingMode = containingBlock.writingMode();
    if (!writingMode.isHorizontal())
        rect = rect.transposedRect();
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlineStart = writingMode.isLogicalLeftInlineStart() ? rect.x() - paddingBox.x() : paddingBox.maxX() - rect.maxX();
    return LayoutRect { inlineStart, rect.y() - paddingBox.y(), rect.width(), rect.height() };
}

// Like RenderBlock::layoutOutOfFlowBoxes once the containing block has its size: the out-of-flow boxes BlockLayout::updateRenderers registered
// on it (RenderBlock::outOfFlowBoxes()).
void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock, PaintInvalidation& paintInvalidation)
{
    auto* outOfFlowBoxes = containingBlock.outOfFlowBoxes();
    if (!outOfFlowBoxes)
        return;
    auto writingMode = containingBlock.writingMode();
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto paddingBoxInlineStart = writingMode.isLogicalLeftInlineStart() ? paddingBox.x() : containingBlock.logicalWidth() - paddingBox.maxX();
    auto paddingBoxContainingBlock = Layout::OutOfFlowContainingBlock { writingMode, containingBlock.hasPotentiallyScrollableOverflow(), paddingBoxInlineStart, paddingBox.y(), paddingBox.width(), paddingBox.height(), scrollableContainingBlock(containingBlock) };
    // The containing block a positioned inline box forms (css-position-3 §2.1): from the start of the padding box of its first fragment to
    // the end of the padding box of its last one in the inline axis, which is the left one of the last line in right-to-left, and the padding
    // box of the bounding box of its fragments in the block axis (RenderBox::containingBlockRangeForPositioned, PositionedLayoutConstraints::fixupLogicalLeftPosition).
    auto inlineBoxContainingBlock = [&](const RenderBoxModelObject& inlineBox) {
        // The fragments are in the block container whose inline content the inline box is in, which may be an anonymous block inside the
        // containing block.
        CheckedRef inlineContentRoot = *inlineBox.containingBlock();
        auto inlineContentRootOffset = LayoutSize { };
        for (CheckedPtr<const RenderBlock> block = inlineContentRoot.ptr(); block && block != &containingBlock; block = block->containingBlock())
            inlineContentRootOffset += LayoutSize { block->logicalLeft(), block->logicalTop() };
        auto firstFragment = inlineBox.firstFragmentBorderBoxRect();
        if (writingMode.isBlockFlipped())
            inlineContentRoot->physicalToBlockStartRelative(firstFragment);
        auto firstFragmentLogicalTopLeft = writingMode.isHorizontal() ? firstFragment.location() : firstFragment.location().transposedPoint();
        auto leftFragmentLogicalLeft = firstFragmentLogicalTopLeft.x();
        if (!writingMode.isLogicalLeftInlineStart()) {
            if (auto fragment = InlineIterator::lineLeftmostInlineBoxFor(inlineBox)) {
                while (auto next = std::ranges::next(fragment))
                    fragment = next;
                leftFragmentLogicalLeft = fragment->logicalLeftIgnoringInlineDirection();
            }
        }
        auto inlineSize = inlineBox.paddingBoxLogicalWidth();
        auto logicalLeft = inlineContentRootOffset.width() + leftFragmentLogicalLeft + inlineBox.borderLogicalLeft();
        auto inlineStart = writingMode.isLogicalLeftInlineStart() ? logicalLeft : containingBlock.logicalWidth() - logicalLeft - inlineSize;
        auto blockStart = inlineContentRootOffset.height() + firstFragmentLogicalTopLeft.y() + inlineBox.borderBefore();
        return Layout::OutOfFlowContainingBlock { writingMode, false, inlineStart, blockStart, inlineSize, inlineBox.paddingBoxLogicalHeight() };
    };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    // Laying out a box registers the out-of-flow boxes in it, which can make the list grow while it is iterated (like in RenderBlock::layoutOutOfFlowBoxes).
    for (CheckedRef outOfFlowBox : *outOfFlowBoxes) {
        // Like RenderElement::layoutIfNeeded, which captures how far scrolling between an anchor-positioned box and its default anchor moves
        // it when it lays the box out. Capturing again compares with the previous capture, which tells style resolution to resolve anchor()
        // and position-visibility again.
        auto capturesScrollSnapshots = outOfFlowBox->needsLayout() && Style::AnchorPositionEvaluator::isAnchorPositioned(outOfFlowBox->style());
        auto outOfFlowContainingBlock = paddingBoxContainingBlock;
        auto boxStaticPosition = staticPosition(outOfFlowBox, containingBlock);
        auto boxDefaultAnchor = defaultAnchor(outOfFlowBox, containingBlock);
        if (CheckedPtr container = outOfFlowBox->container(); container && container->isInlineBox()) {
            outOfFlowContainingBlock = inlineBoxContainingBlock(downcast<RenderBoxModelObject>(*container));
            // The static position and the default anchor are relative to the padding box of the block.
            auto inlineOffset = outOfFlowContainingBlock.inlineStart - paddingBoxContainingBlock.inlineStart;
            auto blockOffset = outOfFlowContainingBlock.blockStart - paddingBoxContainingBlock.blockStart;
            boxStaticPosition.inlinePosition -= inlineOffset;
            boxStaticPosition.blockPosition -= blockOffset;
            if (boxDefaultAnchor) {
                boxDefaultAnchor->inlineStart -= inlineOffset;
                boxDefaultAnchor->blockStart -= blockOffset;
            }
        }
        // What -webkit-fill-available resolves to in the block axis: the available block size of the containing block, which is not the
        // inset-modified containing block (RenderBox::computeSizingKeywordLogicalContentHeightUsing).
        if (CheckedRef style = outOfFlowBox->style(); style->logicalHeight().isFillAvailable() || style->logicalMinHeight().isFillAvailable() || style->logicalMaxHeight().isFillAvailable())
            outOfFlowContainingBlock.fillAvailableBlockSize = containingBlock.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
        auto layoutOutOfFlowBox = [&] {
            auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layoutOutOfFlowBox(outOfFlowBox, outOfFlowContainingBlock, boxStaticPosition, boxDefaultAnchor);
            LOG_WITH_STREAM(Layout, stream << "BlockLayout for out-of-flow box (" << outOfFlowBox.ptr() << "): " << result);
            auto hasAutoScrollbarChanges = updateRenderers(result, paintInvalidation);
            return std::pair { hasAutoScrollbarChanges, WTF::move(result) };
        };
        blockLayoutState.setLastFormattedLineRoots({ });
        auto [hasAutoScrollbarChanges, result] = layoutOutOfFlowBox();
        if (auto lastFormattedLineRoots = lastFormattedLineRootsForTrimEnd(nullptr, result); !lastFormattedLineRoots.isEmpty()) {
            blockLayoutState.setLastFormattedLineRoots(WTF::move(lastFormattedLineRoots));
            hasAutoScrollbarChanges |= layoutOutOfFlowBox().first;
        }
        // Once more when its auto scrollbars, or those of the scroll containers in it, come or go (RelayoutScopeForScrollbarChange).
        if (hasAutoScrollbarChanges)
            layoutOutOfFlowBox();
        if (capturesScrollSnapshots)
            Style::AnchorPositionEvaluator::captureScrollSnapshots(outOfFlowBox);
    }
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableContainingBlock = const_cast<RenderBlock&>(containingBlock);
    // The transforms of the boxes that depend on the size of their containing block (offset-path), before the overflow they extend.
    mutableContainingBlock->updateOutOfFlowDescendantTransformsAfterLayout();
    // Fixed positioned boxes do not scroll with the content, so they add no scrollable overflow, and a positioned inline box places the boxes from
    // its first fragment (RenderBlock::addOverflowFromOutOfFlowBoxes).
    downcast<RenderBlockFlow>(mutableContainingBlock).addOverflowFromOutOfFlowBoxes();
}

bool BlockLayout::updateRenderers(const Layout::BlockLayoutResult& result, PaintInvalidation& paintInvalidation)
{
    auto hasAutoScrollbarChanges = false;
    // This is the only place where the block formatting context's renderers are written, and the only place where its logical
    // coordinates (from the inline start of each containing block) are mapped to the render tree's.
    auto mutableRenderer = [](const CheckedRef<const RenderBox>& renderer) -> CheckedRef<RenderBox> {
        SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableRenderer = const_cast<RenderBox&>(renderer.get());
        return mutableRenderer;
    };

    // The containing block of an in-flow block-level box is its parent, or the block container whose inline content it is in.
    auto containingBlockOf = [](const RenderBox& box) -> const RenderBox& {
        if (box.isOutOfFlowPositioned() || box.parent()->isInlineBox())
            return *box.containingBlock();
        return downcast<RenderBox>(*box.parent());
    };
    // The paint invalidation is what RenderBlockFlow::layoutBlockChild and layoutBlock do, from the geometry before anything is written.
    struct PreviousState {
        LayoutRect borderBoxRect;
        bool everHadLayout { false };
    };
    auto previousStates = Vector<PreviousState> { };
    auto layoutRepainters = Vector<LayoutRepainter> { };
    // What RenderBlockFlow::layoutBlockChild does for a box after the clamp point: it hides it and its content, which it does not lay out.
    for (auto& invisibleBox : result.invisibleBoxes) {
        CheckedRef renderer = mutableRenderer(invisibleBox);
        LineClampUpdater::setIsForcedHidden(renderer, true);
        LineClampUpdater::skipLayoutForForcedHidden(renderer);
    }

    previousStates.reserveInitialCapacity(result.boxes.size());
    layoutRepainters.reserveInitialCapacity(result.boxes.size());
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        previousStates.append({ renderer->borderBoxRectInContainer(), renderer->everHadLayout() });
        // The formatting context of a box laid out through the render tree invalidated it when it laid it out.
        layoutRepainters.append(LayoutRepainter { renderer, box.isLaidOutThroughRenderTree ? std::optional { LayoutRepainter::CheckForRepaint::No } : std::nullopt });
    }

    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        auto& geometry = box.geometry;
        // Margins follow the direction of the containing block.
        auto containingBlockWritingMode = containingBlockOf(renderer).writingMode();
        renderer->setMarginStart(geometry.marginStart(), containingBlockWritingMode);
        renderer->setMarginEnd(geometry.marginEnd(), containingBlockWritingMode);
        renderer->setMarginBefore(geometry.marginBefore(), containingBlockWritingMode);
        renderer->setMarginAfter(geometry.marginAfter(), containingBlockWritingMode);
        // Sizes and positions are in the writing mode of the containing block, which an orthogonal box does not share.
        auto borderBoxSize = LayoutSize { geometry.borderBoxWidth(), geometry.borderBoxHeight() };
        renderer->setBorderBoxSize(containingBlockWritingMode.isHorizontal() ? borderBoxSize : borderBoxSize.transposedSize());
        // What RenderBlockFlow::layoutBlock leaves for the parent to collapse with, when the parent is laid out by the render tree path
        // and this box is not laid out again. A box laid out through the render tree has them from its own layout, in its own writing
        // mode, which an orthogonal box does not share with the block formatting context.
        if (CheckedPtr blockFlow = box.isLaidOutThroughRenderTree ? nullptr : dynamicDowncast<RenderBlockFlow>(renderer)) {
            // The floats the render tree path listed from its last layout of the box: the block formatting context placed them this time, and
            // they paint from the tree (RenderBlockFlow::paintFloats).
            blockFlow->m_floatingObjects = nullptr;
            if (!blockFlow->floatsPlacedByBlockFormattingContext().isEmpty())
                renderer->view().frameView().layoutContext().addRootWithFloatsPlacedByBlockFormattingContext(blockFormattingContextRootOf(*blockFlow));
            blockFlow->setMaxMarginBeforeValues(box.collapsedMarginsBefore.positive, box.collapsedMarginsBefore.negative);
            blockFlow->setMaxMarginAfterValues(box.collapsedMarginsAfter.positive, box.collapsedMarginsAfter.negative);
            blockFlow->setHasMarginBeforeQuirk(box.marginBeforeIsQuirky);
            blockFlow->setHasMarginAfterQuirk(box.marginAfterIsQuirky);
        }
    }

    // The render tree measures the logical left from the left (top in vertical writing modes) edge of the containing block in
    // either direction, so placing a box in a right-to-left containing block needs the containing block's final width.
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        auto& geometry = box.geometry;
        CheckedRef containingBlock = containingBlockOf(renderer);
        auto logicalLeft = Layout::BoxGeometry::borderBoxLeft(geometry);
        if (!containingBlock->writingMode().isLogicalLeftInlineStart())
            logicalLeft = containingBlock->logicalWidth() - logicalLeft - geometry.borderBoxWidth();
        auto location = LayoutPoint { logicalLeft, Layout::BoxGeometry::borderBoxTop(geometry) };
        auto physicalLocation = containingBlock->writingMode().isHorizontal() ? location : location.transposedPoint();
        // The render tree places a box whose containing block is a positioned inline box from the border box of the first fragment of the
        // inline box, which is in the block container whose inline content the inline box is in
        // (RenderBoxModelObject::blockStartRelativeFirstFragmentBorderBoxRect).
        if (CheckedPtr container = renderer->isOutOfFlowPositioned() ? renderer->container() : nullptr; container && container->isInlineBox()) {
            for (CheckedPtr<const RenderBlock> block = container->containingBlock(); block && block != containingBlock.ptr(); block = block->containingBlock())
                physicalLocation -= toLayoutSize(block->location());
            physicalLocation -= toLayoutSize(downcast<RenderBoxModelObject>(*container).blockStartRelativeFirstFragmentBorderBoxRect().location());
        }
        renderer->setLocation(physicalLocation);
    }

    // What RenderBox::computePercentageLogicalHeight does for a box whose percentage block sizes resolve against a table cell: the cell registers
    // it, which makes the table lay the cell out again once the row has its height (RenderTableSection::relayoutCellIfFlexed).
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        if (box.isLaidOutThroughRenderTree || !Layout::BlockFormattingUtils::hasPercentageBlockSize(renderer, renderer->writingMode()))
            continue;
        CheckedPtr containingBlock = renderer->containingBlock();
        while (containingBlock && renderer->skipContainingBlockForPercentHeightCalculation(*containingBlock, renderer->isHorizontalWritingMode() != containingBlock->isHorizontalWritingMode()))
            containingBlock = containingBlock->containingBlock();
        if (CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(containingBlock))
            tableCell->addPercentHeightDescendant(renderer);
    }

    // What RenderBlockFlow::adjustOutOfFlowBlock and layoutBlockChildren do for an out-of-flow child: its layer keeps the static position, and its
    // containing block registers it, to lay it out once it has its size.
    for (auto& outOfFlowBox : result.outOfFlowBoxes) {
        CheckedRef renderer = mutableRenderer(outOfFlowBox.renderer);
        CheckedRef layer = *renderer->layer();
        if (layer->staticBlockPosition() != outOfFlowBox.staticBlockPosition) {
            layer->setStaticBlockPosition(outOfFlowBox.staticBlockPosition);
            CheckedRef style = renderer->style();
            if (style->hasStaticBlockPosition(renderer->parent()->isHorizontalWritingMode()))
                renderer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        }
        layer->setStaticInlinePosition(outOfFlowBox.staticInlinePosition);
        CheckedPtr containingBlock = renderer->containingBlock();
        containingBlock->addOutOfFlowBox(renderer);
    }

    // How far each box and its containing block moved within the root. While a box is laid out, the render tree's layout delta holds
    // that offset so that its paint invalidation sees it where it was. In reverse post-order the containing block comes before the box.
    auto locationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockLocationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockIndexes = Vector<size_t> { };
    for (auto boxIndex = result.boxes.size(); boxIndex--;) {
        CheckedRef renderer = result.boxes[boxIndex].renderer;
        while (!containingBlockIndexes.isEmpty() && result.boxes[containingBlockIndexes.last()].renderer.ptr() != &containingBlockOf(renderer))
            containingBlockIndexes.removeLast();
        containingBlockLocationChanges[boxIndex] = containingBlockIndexes.isEmpty() ? LayoutSize { } : locationChanges[containingBlockIndexes.last()];
        locationChanges[boxIndex] = containingBlockLocationChanges[boxIndex] + (previousStates[boxIndex].borderBoxRect.location() - renderer->location());
        containingBlockIndexes.append(boxIndex);
    }

    CheckedRef layoutContext = rootRenderer().view().frameView().layoutContext();
    // The layout state has the paint offset of the root only, not of the boxes inside.
    auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
    auto invalidateWithLayoutDelta = [&](LayoutSize layoutDelta, NOESCAPE auto&& invalidate) {
        layoutContext->addLayoutDelta(layoutDelta);
        invalidate();
        layoutContext->addLayoutDelta(-layoutDelta);
    };

    // Descendants come before their ancestors, which the overflow of the ancestors needs.
    for (size_t boxIndex = 0; boxIndex < result.boxes.size(); ++boxIndex) {
        auto& box = result.boxes[boxIndex];
        CheckedRef renderer = mutableRenderer(box.renderer);
        // The formatting context of a box laid out through the render tree did what follows for it.
        CheckedPtr blockFlow = box.isLaidOutThroughRenderTree ? nullptr : dynamicDowncast<RenderBlockFlow>(renderer);
        if (blockFlow) {
            blockFlow->clearOverflow();
            // Like RenderBlockFlow::layoutBlock, the content area ends where the content does, not where the box does.
            auto contentArea = blockFlow->blockStartRelativeContentBoxRect();
            auto contentBlockEnd = blockFlow->borderAndPaddingBefore() + box.intrinsicContentBlockSize;
            auto isHorizontalWritingMode = blockFlow->writingMode().isHorizontal();
            if (isHorizontalWritingMode)
                contentArea.shiftMaxYEdgeTo(contentBlockEnd);
            else
                contentArea.shiftMaxXEdgeTo(contentBlockEnd);
            // The transforms of the children that depend on the size of their containing block (offset-path), before the overflow they extend.
            blockFlow->updateInFlowDescendantTransformsAfterLayout();
            // The inline formatting context took the overflow of the block-level boxes in the inline content from their renderers, before they
            // had their new geometry.
            if (CheckedPtr lineLayout = blockFlow->inlineLayout(); lineLayout && lineLayout->hasBlocks())
                lineLayout->updateOverflow();
            blockFlow->computeInFlowOverflow(contentArea, isHorizontalWritingMode ? RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaX : RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaY);
            // Like RenderBlockFlow::layoutBlock, once the box has its size and its in-flow overflow.
            invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
                layoutOutOfFlowBoxes(*blockFlow, paintInvalidation);
            });
        }
        // Like RenderBlockFlow::layoutBlock: the transform depends on the size of the box (transform-origin).
        renderer->updateLayerTransform();

        // What RenderBlockFlow::layoutBlock invalidates: changes inside the box, at the place it had before it moved.
        auto damagedInlineContent = std::optional<PaintInvalidation::LogicalTopAndBottom> { };
        if (blockFlow) {
            // The geometry is in the writing mode of the containing block, whose inline axis is the box's block axis in an orthogonal flow.
            auto blockAxisBorderAndPadding = blockFlow->writingMode().isOrthogonal(containingBlockOf(*blockFlow).writingMode()) ? box.geometry.horizontalBorderAndPadding() : box.geometry.verticalBorderAndPadding();
            damagedInlineContent = paintInvalidation.damagedInlineContentLogicalTopAndBottom(*blockFlow, blockAxisBorderAndPadding + box.intrinsicContentBlockSize);
            if (CheckedPtr cache = protect(blockFlow->document())->existingAXObjectCache(); cache && damagedInlineContent)
                cache->onLaidOutInlineContent(*blockFlow);
        }
        invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
            auto didInvalidateWholeBox = layoutRepainters[boxIndex].repaintAfterLayout();
            if (damagedInlineContent && !didInvalidateWholeBox)
                downcast<RenderBlockFlow>(renderer).repaintInlineContentAfterLayout(damagedInlineContent->logicalTop, damagedInlineContent->logicalBottom);
        });
        // What RenderBlock::layout and RenderBlock::recomputeLogicalWidth do once the box is laid out. The scroll size and position of a
        // scroll container follow its new overflow, and so do its auto scrollbars, which lay it out again when they come or go, or the
        // ancestor whose size they change (RelayoutScopeForScrollbarChange).
        if (blockFlow) {
            if (auto scrollbarUpdate = blockFlow->updateScrollInfoAfterLayout(); scrollbarUpdate && !scrollbarUpdate->autoScrollbarChanges().isEmpty()) {
                auto& autoScrollbarChanges = scrollbarUpdate->autoScrollbarChanges();
                blockFlow->repaint();
                CheckedRef style = blockFlow->style();
                if (style->overflowX() == Overflow::Auto || style->overflowY() == Overflow::Auto) {
                    auto& subtreeScrollbarChangesState = blockFlow->layoutContext().subtreeScrollbarChangesState();
                    auto sizesAffected = subtreeScrollbarChangesState && subtreeScrollbarChangesState->isEligibleForScrollbarHandlingByAncestor(*blockFlow) ? subtreeScrollbarChangesState->sizesAffectedForSubtreeRootFromScrollbarChanges(*blockFlow, autoScrollbarChanges) : EnumSet<LogicalBoxAxis> { };
                    if (!sizesAffected.isEmpty())
                        subtreeScrollbarChangesState->addRendererWithScrollbarChange(*blockFlow, sizesAffected);
                    else
                        hasAutoScrollbarChanges = true;
                }
            }
            blockFlow->setShouldForceRelayoutChildren(false);
        }
        renderer->invalidateBackgroundObscurationStatus();
        renderer->setScrollAnchoringSuppressionStyleChanged(false);
        renderer->clearNeedsLayout();

        // What RenderBlockFlow::layoutBlockChild invalidates: the box if it is new or moved.
        if (!renderer->checkForRepaintDuringLayout())
            continue;
        auto& previousState = previousStates[boxIndex];
        invalidateWithLayoutDelta(containingBlockLocationChanges[boxIndex], [&] {
            if (!previousState.everHadLayout)
                renderer->repaint();
            else if (!renderer->parent()->selfNeedsLayout())
                renderer->repaintDuringLayoutIfMoved(previousState.borderBoxRect);
        });
    }
    return hasAutoScrollbarChanges;
}

} // namespace LayoutIntegration
} // namespace WebCore
