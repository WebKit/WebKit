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
#include "LayoutIntegrationBlockCoverage.h"

#include "AnchorPositionEvaluator.h"
#include "BlockFormattingUtils.h"
#include "BlockMarginCollapse.h"
#include "Document.h"
#include "HTMLButtonElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderAncestorIterator.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace LayoutIntegration {

enum class BlockAvoidanceReason : uint8_t {
    BlockFormattingContextIntegrationDisabled,
    NotABlockFormattingContextRoot,
    RootIsRenderView,
    RootIsListItem,
    RootIsTableCell,
    RootIsMultiColumnFlow,
    RootIsFieldset,
    RootIsMarquee,
    RootHasUnsupportedRenderer,
    RootIsAnonymousFlexOrGridItem,
    RootHasUnsupportedWritingMode,
    RootIsPaginated,
    RootIsInLineClampContext,
    RootIsInTextBoxTrimContext,
    RootIsInLineGridContext,
    RootHasColumns,
    RootHasAlignContent,
    RootHasMarginTrim,
    RootHasStableBothEdgesScrollbarGutter,
    RootHasWebKitTextAlign,
    ContentHasFloat,
    ContentIsHiddenByLineClamp,
    ContentHasOutOfFlowBox,
    ContentHasOutOfFlowNonBlockContainer,
    ContentIsListItem,
    ContentIsFormControl,
    ContentHasStretchedQuirksModeBody,
    ContentHasUnsupportedRenderer,
    ContentHasFormattingContextRoot,
    ContentHasLayer,
    ContentHasBlockInInline,
    ContentHasUnsupportedInlineBox,
    ContentHasOrthogonalFlow,
    ContentHasSizingKeyword,
    ContentHasStretchBlockSize,
    ContentHasAspectRatio,
    ContentHasSizeContainment,
    ContentHasUnmeasurableIntrinsicInlineSize,
    ContentHasQuirksModePercentageHeight,
    ContentHasNestedQuirkContainer,
    ContentHasAnchorPositioning,
    ContentHasOutOfFlowSelfAlignment,
    ContentHasFlippedBlockWritingMode,
    ContentHasOutOfFlowScrollContainer,
    ContentHasMarginTrim,
    ContentHasWebKitTextAlign,
    ContentHasLineClamp,
    ContentHasTextBoxTrim,
    ContentHasLineGrid,
    ContentHasJustifySelf,
    ContentHasBlockStepSize
};

// The -webkit-left, -webkit-right and -webkit-center values of text-align move block-level children too (RenderBox::computeInlineDirectionMargins).
static bool alignsBlockLevelChildren(const Style::ComputedStyle& style)
{
    auto textAlign = style.textAlign();
    return textAlign == Style::TextAlign::WebKitLeft || textAlign == Style::TextAlign::WebKitRight || textAlign == Style::TextAlign::WebKitCenter;
}

static std::optional<BlockAvoidanceReason> rootAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    if (!rootContainer.document().settings().blockFormattingContextIntegrationEnabled())
        return BlockAvoidanceReason::BlockFormattingContextIntegrationDisabled;
    if (!rootContainer.createsNewFormattingContext())
        return BlockAvoidanceReason::NotABlockFormattingContextRoot;
    // Not RenderView, list items, table cells and captions, multi-column flows, fieldsets or form controls, which lay out their content their own way,
    // nor marquees, whose children fit their content (RenderBox::sizesLogicalWidthToFitContent).
    if (rootContainer.isRenderView())
        return BlockAvoidanceReason::RootIsRenderView;
    if (rootContainer.isRenderListItem())
        return BlockAvoidanceReason::RootIsListItem;
    if (rootContainer.isRenderTableCell())
        return BlockAvoidanceReason::RootIsTableCell;
    if (rootContainer.isRenderMultiColumnFlow())
        return BlockAvoidanceReason::RootIsMultiColumnFlow;
    if (rootContainer.isFieldset())
        return BlockAvoidanceReason::RootIsFieldset;
    if (rootContainer.isHTMLMarquee())
        return BlockAvoidanceReason::RootIsMarquee;
    if (rootContainer.type() != RenderObject::Type::BlockFlow)
        return BlockAvoidanceReason::RootHasUnsupportedRenderer;
    // Percentage block sizes inside look through an anonymous block to the flex or grid container (RenderBox::skipContainingBlockForPercentHeightCalculation).
    if ((rootContainer.isFlexItemIncludingDeprecated() || rootContainer.isGridItem()) && rootContainer.isAnonymous())
        return BlockAvoidanceReason::RootIsAnonymousFlexOrGridItem;
    // FIXME: Vertical writing modes, and horizontal-bt, whose inline content the inline formatting context places from the border and padding
    // at the top, its block-end side (BoxGeometryUpdater::formattingContextConstraints).
    if (!rootContainer.writingMode().isHorizontal() || rootContainer.writingMode().isBlockFlipped())
        return BlockAvoidanceReason::RootHasUnsupportedWritingMode;
    CheckedRef layoutContext = rootContainer.view().frameView().layoutContext();
    if (auto* layoutState = layoutContext->layoutState()) {
        if (layoutState->isPaginated())
            return BlockAvoidanceReason::RootIsPaginated;
        if (layoutState->lineClamp() || layoutState->legacyLineClamp())
            return BlockAvoidanceReason::RootIsInLineClampContext;
        // The layout state has no entry for the boxes inside the block formatting context, so their inline content would see the
        // root's line grid at the wrong offset.
        if (layoutState->lineGrid())
            return BlockAvoidanceReason::RootIsInLineGridContext;
    }
    if (rootContainer.enclosingFragmentedFlow())
        return BlockAvoidanceReason::RootIsPaginated;
    CheckedRef style = rootContainer.style();
    if (layoutContext->textBoxTrim() || style->textBoxTrim() != TextBoxTrim::None)
        return BlockAvoidanceReason::RootIsInTextBoxTrimContext;
    if (!style->lineGrid().isNone())
        return BlockAvoidanceReason::RootIsInLineGridContext;
    if (style->specifiesColumns() || rootContainer.multiColumnFlow())
        return BlockAvoidanceReason::RootHasColumns;
    if (!style->alignContent().isNormal())
        return BlockAvoidanceReason::RootHasAlignContent;
    if (!style->marginTrim().isNone())
        return BlockAvoidanceReason::RootHasMarginTrim;
    if (style->scrollbarGutter().isStableBothEdges())
        return BlockAvoidanceReason::RootHasStableBothEdgesScrollbarGutter;
    if (alignsBlockLevelChildren(style.get()))
        return BlockAvoidanceReason::RootHasWebKitTextAlign;
    return { };
}

// Only display: flow-root, overflow: hidden and out-of-flow boxes. The other ways to establish a block formatting context also contain, align or fragment the
// content, or give it scrollbars: those take space, and RenderBlock::layout lays the box out again when they come or go.
static bool isSupportedBlockFormattingContextRoot(const RenderBlockFlow& blockContainer)
{
    CheckedRef style = blockContainer.style();
    auto isScrollContainerWithoutScrollbars = [&] {
        // Marquees scroll their content and size their children differently (RenderBox::sizesLogicalWidthToFitContent).
        return style->overflowX() == Overflow::Hidden && style->overflowY() == Overflow::Hidden && style->scrollbarGutter().isAuto() && !blockContainer.isHTMLMarquee();
    };
    auto isSupportedRoot = blockContainer.hasNonVisibleOverflow() ? isScrollContainerWithoutScrollbars() : style->display() == Style::DisplayType::BlockFlowRoot || blockContainer.isOutOfFlowPositioned();
    return isSupportedRoot && !blockContainer.isWritingModeRoot() && !blockContainer.isFieldset()
        && !blockContainer.shouldApplyAnyContainment() && style->alignContent().isNormal()
        && !style->specifiesColumns() && style->columnSpan() != ColumnSpan::All && style->blockStepSize().isNone()
        && style->overflowContinue() == OverflowContinue::Normal && !style->containerType().hasSizeContainment();
}

static bool isLaidOutByBlockFormattingContext(const RenderElement& outOfFlowBox, const RenderBlockFlow& rootContainer);

static bool isAtomicInlineLevelBox(const RenderObject& renderer)
{
    return renderer.isInline() && is<RenderBox>(renderer) && !is<RenderLineBreak>(renderer);
}

// In quirks mode a percentage block size of the box or of a box in it looks through containing blocks with an auto height for one with a height
// (RenderBox::computePercentageLogicalHeight), past the containing block size the block formatting context gives a box it lays out through the
// render tree.
static bool hasPercentageBlockSizeInQuirksMode(const RenderBox& box)
{
    if (!box.document().inQuirksMode())
        return false;
    if (Layout::BlockFormattingUtils::hasPercentageBlockSize(box, box.writingMode()))
        return true;
    for (CheckedRef descendant : descendantsOfType<RenderBox>(box)) {
        if (Layout::BlockFormattingUtils::hasPercentageBlockSize(descendant, descendant->writingMode()))
            return true;
    }
    return false;
}

static std::optional<BlockAvoidanceReason> outOfFlowBoxesInLeafAvoidanceReason(const RenderBox& leaf, const RenderBlockFlow& rootContainer);

// The inline formatting context lays out an atomic inline-level box through the render tree, with the containing block size the block
// formatting context gives it (BlockIntegrationUtils::layoutInlineContent).
static std::optional<BlockAvoidanceReason> atomicInlineLevelBoxAvoidanceReason(const RenderBox& box, const RenderBlockFlow& rootContainer)
{
    // FIXME: An orthogonal box, whose inline size comes from the available block size (css-writing-modes-4 §7.3).
    if (box.isWritingModeRoot())
        return BlockAvoidanceReason::ContentHasOrthogonalFlow;
    if (hasPercentageBlockSizeInQuirksMode(box))
        return BlockAvoidanceReason::ContentHasQuirksModePercentageHeight;
    return outOfFlowBoxesInLeafAvoidanceReason(box, rootContainer);
}

static std::optional<BlockAvoidanceReason> inlineContentAvoidanceReason(const RenderElement& renderer, const RenderBlockFlow& rootContainer)
{
    if (is<RenderLineBreak>(renderer))
        return { };
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer))
        return atomicInlineLevelBoxAvoidanceReason(*box, rootContainer);
    CheckedRef style = renderer.style();
    if (renderer.type() != RenderObject::Type::Inline || style->display() != Style::DisplayType::InlineFlow || style->pseudoElementType() == PseudoElementType::FirstLetter)
        return BlockAvoidanceReason::ContentHasUnsupportedInlineBox;
    if (renderer.hasLayer())
        return BlockAvoidanceReason::ContentHasLayer;
    return { };
}

// BlockFormattingContext::computeIntrinsicInlineSizes measures inline content with the inline formatting context and the block-level boxes
// between them itself, without percentage padding, which RenderBox resolves against the containing block's current inline size.
// A block-level independent formatting context root the block formatting context lays out through the render tree
// (BlockIntegrationUtils::layoutIndependentFormattingContext): a replaced element, a table, a flex (-webkit-box included) or grid container,
// or a block formatting context root whose content the block formatting context cannot lay out.
static bool isLaidOutThroughRenderTree(const RenderObject& renderer)
{
    if (renderer.isInline() || renderer.isFloatingOrOutOfFlowPositioned())
        return false;
    if (renderer.isRenderReplaced() || renderer.isRenderOrLegacyRenderSVGRoot())
        return true;
    switch (renderer.type()) {
    case RenderObject::Type::Table:
    case RenderObject::Type::FlexibleBox:
    case RenderObject::Type::DeprecatedFlexibleBox:
    case RenderObject::Type::Grid:
        return true;
    case RenderObject::Type::BlockFlow: {
        auto& blockContainer = downcast<RenderBlockFlow>(renderer);
        return blockContainer.createsNewFormattingContext() && !canUseForNestedBlockLayout(blockContainer);
    }
    default:
        return false;
    }
}

static bool canComputeContentInlineSizes(const RenderBlockFlow& box)
{
    for (CheckedPtr<const RenderObject> descendant = &box; descendant;) {
        // Out-of-flow boxes add nothing to the intrinsic sizes of their parent.
        if (descendant != &box && descendant->isOutOfFlowPositioned()) {
            descendant = descendant->nextInPreOrderAfterChildren(&box);
            continue;
        }
        // FIXME: A box laid out through the render tree measures its content against its containing block's renderer, which the block
        // formatting context has not committed yet (RenderBox::minContentLogicalWidthContribution reads its block size through an aspect
        // ratio, for instance).
        if (descendant != &box && (isLaidOutThroughRenderTree(*descendant) || isAtomicInlineLevelBox(*descendant)))
            return false;
        if (CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*descendant); blockContainer && !blockContainer->isInline()) {
            CheckedRef style = blockContainer->style();
            auto writingMode = blockContainer->writingMode();
            if (style->paddingStart(writingMode).isPercentOrCalculated() || style->paddingEnd(writingMode).isPercentOrCalculated() || blockContainer->shouldApplyInlineSizeContainment())
                return false;
        }
        descendant = descendant->nextInPreOrder(&box);
    }
    return true;
}

// The block formatting context lays out the out-of-flow boxes whose containing block it lays out.
static bool isLaidOutByBlockFormattingContext(const RenderElement& outOfFlowBox, const RenderBlockFlow& rootContainer)
{
    CheckedPtr containingBlock = outOfFlowBox.containingBlock();
    return containingBlock == &rootContainer || containingBlock->isDescendantOf(&rootContainer);
}

static std::optional<BlockAvoidanceReason> outOfFlowBoxAvoidanceReason(const RenderBox& outOfFlowBox)
{
    // Block containers, which establish a block formatting context for their content, and replaced elements and flex containers, whose own
    // formatting context lays them out (BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext). Not a fieldset, whose legend sits
    // in its border.
    auto isReplaced = outOfFlowBox.isRenderReplaced() || outOfFlowBox.isRenderOrLegacyRenderSVGRoot();
    auto isFlexContainer = outOfFlowBox.type() == RenderObject::Type::FlexibleBox && !outOfFlowBox.isFieldset();
    if (outOfFlowBox.type() != RenderObject::Type::BlockFlow && !isReplaced && !isFlexContainer)
        return BlockAvoidanceReason::ContentHasOutOfFlowNonBlockContainer;
    // FIXME: An orthogonal replaced element, whose own formatting context gives its size in its writing mode, not in its containing block's.
    if (CheckedPtr containingBlock = outOfFlowBox.containingBlock(); isReplaced && containingBlock && outOfFlowBox.writingMode().isOrthogonal(containingBlock->writingMode()))
        return BlockAvoidanceReason::ContentHasOrthogonalFlow;
    // FIXME: An orthogonal flex container, whose inline size comes from the available block size (css-writing-modes-4 §7.3).
    if (isFlexContainer && outOfFlowBox.isWritingModeRoot())
        return BlockAvoidanceReason::ContentHasOrthogonalFlow;
    CheckedRef style = outOfFlowBox.style();
    // FIXME: Anchor positioning, position-area and position-try fallbacks, and alignment other than normal (css-position-3 §4.5).
    if (Style::AnchorPositionEvaluator::isAnchorPositioned(style.get()) || !style->positionTryFallbacks().isNone())
        return BlockAvoidanceReason::ContentHasAnchorPositioning;
    if (!(style->justifySelf().isAuto() || style->justifySelf().isNormal()) || !(style->alignSelf().isAuto() || style->alignSelf().isNormal()))
        return BlockAvoidanceReason::ContentHasOutOfFlowSelfAlignment;
    // FIXME: A flipped block flow direction (horizontal-bt), where the block-start inset is the physical bottom one and the render tree
    // stores flipped block positions.
    if (outOfFlowBox.writingMode().isBlockFlipped())
        return BlockAvoidanceReason::ContentHasFlippedBlockWritingMode;
    // A scroll container with a definite size is a layout boundary, which the render tree path lays out on its own (RenderObject::markContainingBlocksForLayout).
    if (outOfFlowBox.hasNonVisibleOverflow())
        return BlockAvoidanceReason::ContentHasOutOfFlowScrollContainer;
    // So is a box with layout and size containment.
    if (outOfFlowBox.shouldApplyLayoutContainment() && outOfFlowBox.shouldApplySizeContainment())
        return BlockAvoidanceReason::ContentHasSizeContainment;
    // Its own formatting context sizes a replaced element, from its natural size and aspect ratio.
    if (isReplaced)
        return { };
    // FIXME: The sizing keywords and calc-size() (css-sizing-3 §3.2); lengths, percentages and calc() otherwise.
    auto isSupportedSize = [](auto& size) {
        return size.isSpecified() && !size.isCalcSize();
    };
    // A calc-size() answers to the keyword of its basis, auto included.
    auto isAuto = [](auto& size) {
        return size.isAuto() && !size.isCalcSize();
    };
    if (!(isAuto(style->width()) || isSupportedSize(style->width())) || !(isAuto(style->height()) || isSupportedSize(style->height()))
        || !(isAuto(style->minWidth()) || isSupportedSize(style->minWidth())) || !(isAuto(style->minHeight()) || isSupportedSize(style->minHeight()))
        || !(style->maxWidth().isNone() || isSupportedSize(style->maxWidth())) || !(style->maxHeight().isNone() || isSupportedSize(style->maxHeight())))
        return BlockAvoidanceReason::ContentHasSizingKeyword;
    // An auto inline size with an auto inset is the fit-content size (css-position-3 §4.3), from the inline sizes of the content.
    auto writingMode = outOfFlowBox.writingMode();
    auto hasFitContentInlineSize = style->logicalWidth(writingMode).isAuto() && (style->insetStart(writingMode).isAuto() || style->insetEnd(writingMode).isAuto());
    // The render tree measures a flex container (BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes).
    if (hasFitContentInlineSize && !isFlexContainer && !canComputeContentInlineSizes(downcast<RenderBlockFlow>(outOfFlowBox)))
        return BlockAvoidanceReason::ContentHasUnmeasurableIntrinsicInlineSize;
    // FIXME: The automatic size a preferred aspect ratio gives in the other axis (css-sizing-4 §5.1).
    if (style->aspectRatio().hasRatio())
        return BlockAvoidanceReason::ContentHasAspectRatio;
    return { };
}

// The render tree lays out the content of a leaf (an atomic inline-level box or a box laid out through the render tree), and registers the
// out-of-flow boxes it finds on their containing block (RenderBlock::addOutOfFlowBox). The block formatting context lays out the ones whose
// containing block it lays out, outside the leaf, from the static position on their layer, which it moves out of the leaf through the boxes
// in between (BlockLayout::layoutOutOfFlowBoxes): inline boxes, which add nothing to it, and in-flow boxes in a horizontal, unflipped writing
// mode outside fragmented flows.
static std::optional<BlockAvoidanceReason> outOfFlowBoxesInLeafAvoidanceReason(const RenderBox& leaf, const RenderBlockFlow& rootContainer)
{
    auto isSupportedBoxInBetween = [](const RenderElement& box) {
        if (box.isInlineBox())
            return true;
        return is<RenderBox>(box) && !box.isOutOfFlowPositioned() && box.writingMode().isHorizontal() && !box.writingMode().isBlockFlipped() && !box.isRenderFragmentedFlow();
    };
    for (CheckedRef descendant : descendantsOfType<RenderBox>(leaf)) {
        if (!descendant->isOutOfFlowPositioned())
            continue;
        CheckedPtr containingBlock = descendant->containingBlock();
        if (containingBlock == &leaf || containingBlock->isDescendantOf(&leaf) || !isLaidOutByBlockFormattingContext(descendant, rootContainer))
            continue;
        if (auto reason = outOfFlowBoxAvoidanceReason(descendant))
            return reason;
        // Its static position comes from its block container (a flex or grid container would align it instead), and the block formatting
        // context lays out its content too.
        CheckedPtr staticPositionBlock = descendant->parent();
        while (staticPositionBlock->isInlineBox())
            staticPositionBlock = staticPositionBlock->parent();
        CheckedPtr outOfFlowBlockContainer = dynamicDowncast<RenderBlockFlow>(descendant);
        if (!is<RenderBlockFlow>(*staticPositionBlock) || (outOfFlowBlockContainer && !canUseForNestedBlockLayout(*outOfFlowBlockContainer)))
            return BlockAvoidanceReason::ContentHasOutOfFlowBox;
        for (CheckedPtr box = staticPositionBlock; box != containingBlock; box = box->parent()) {
            if (!isSupportedBoxInBetween(*box))
                return BlockAvoidanceReason::ContentHasOutOfFlowBox;
        }
    }
    return { };
}

static std::optional<BlockAvoidanceReason> independentFormattingContextAvoidanceReason(const RenderBox& box, const RenderBlockFlow& rootContainer)
{
    if (box.isForceHiddenByLineClamp())
        return BlockAvoidanceReason::ContentIsHiddenByLineClamp;
    // Form controls are flex containers with content of their own.
    if (isAnyOf<HTMLButtonElement, HTMLInputElement, HTMLSelectElement, HTMLTextAreaElement>(box.element()))
        return BlockAvoidanceReason::ContentIsFormControl;
    if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(box.parent()); !parent || parent->childrenInline())
        return BlockAvoidanceReason::ContentHasBlockInInline;
    if (hasPercentageBlockSizeInQuirksMode(box))
        return BlockAvoidanceReason::ContentHasQuirksModePercentageHeight;
    // Quirks mode: a body laid out through the render tree stretches to the viewport from the size of its renderer's ancestors
    // (RenderBox::computeLogicalHeight), which the block formatting context has not committed yet.
    if (box.stretchesToViewport())
        return BlockAvoidanceReason::ContentHasStretchedQuirksModeBody;
    // FIXME: An orthogonal box, whose inline size comes from the available block size (css-writing-modes-4 §7.3).
    if (box.isWritingModeRoot())
        return BlockAvoidanceReason::ContentHasOrthogonalFlow;
    CheckedRef style = box.style();
    auto writingMode = box.writingMode();
    // The block formatting context decides the inline size from lengths, percentages and calc() (BlockFormattingUtils::inlineSizeAndMargins).
    // A replaced element's own formatting context takes its aspect ratio into account (CSS 2.2 §10.3.4).
    // FIXME: The sizing keywords, calc-size(), the stretch block size, which reads the containing block's renderer, and a preferred aspect ratio.
    auto isSupportedInlineSize = [](auto& size) {
        return (size.isAuto() || size.isSpecified()) && !size.isCalcSize();
    };
    auto isStretch = [](auto& size) {
        return size.isStretch();
    };
    auto isReplaced = box.isRenderReplaced() || box.isRenderOrLegacyRenderSVGRoot();
    if (!isSupportedInlineSize(style->logicalWidth(writingMode)) || !isSupportedInlineSize(style->logicalMinWidth(writingMode))
        || !(style->logicalMaxWidth(writingMode).isNone() || (style->logicalMaxWidth(writingMode).isSpecified() && !style->logicalMaxWidth(writingMode).isCalcSize())))
        return BlockAvoidanceReason::ContentHasSizingKeyword;
    if (isStretch(style->logicalHeight(writingMode)) || isStretch(style->logicalMinHeight(writingMode)) || isStretch(style->logicalMaxHeight(writingMode)))
        return BlockAvoidanceReason::ContentHasStretchBlockSize;
    if (style->aspectRatio().hasRatio() && !isReplaced)
        return BlockAvoidanceReason::ContentHasAspectRatio;
    // The block-step-size rounding of the margin box is the parent's job on the render tree path (RenderBlockFlow::performBlockStepSizing).
    CheckedRef parentStyle = box.parent()->style();
    if (!style->justifySelf().resolve(parentStyle.ptr()).isNormalStretchOrLegacy())
        return BlockAvoidanceReason::ContentHasJustifySelf;
    if (!style->blockStepSize().isNone())
        return BlockAvoidanceReason::ContentHasBlockStepSize;
    return outOfFlowBoxesInLeafAvoidanceReason(box, rootContainer);
}

static std::optional<BlockAvoidanceReason> contentAvoidanceReason(const RenderElement& renderer, const RenderBlockFlow& rootContainer)
{
    // A box a line-clamp container hid after its clamp point stays hidden until the render tree path lays it out again
    // (RenderBlockFlow::layoutBlockChild, markInlineContentDirtyForLayout).
    if (renderer.isForceHiddenByLineClamp())
        return BlockAvoidanceReason::ContentIsHiddenByLineClamp;
    // Without floats, clear introduces no clearance (CSS 2.2 §9.5.2), and floats outside the root stay out of its block formatting context.
    if (renderer.isFloating())
        return BlockAvoidanceReason::ContentHasFloat;
    if (renderer.isOutOfFlowPositioned()) {
        // Its static position comes from the block formatting context its parent takes part in, or from the inline formatting context of the
        // inline content it is in, which registers it on its containing block too (RenderBlockFlow::markInlineContentDirtyForLayout).
        if (CheckedPtr parent = renderer.parent(); !is<RenderBlockFlow>(*parent) && !parent->isInlineBox())
            return BlockAvoidanceReason::ContentHasOutOfFlowBox;
        // A containing block outside the root lays it out through the render tree path.
        if (!isLaidOutByBlockFormattingContext(renderer, rootContainer))
            return { };
        if (auto reason = outOfFlowBoxAvoidanceReason(downcast<RenderBox>(renderer)))
            return reason;
        // A replaced element has no block container checks to pass.
        if (renderer.isRenderReplaced() || renderer.isRenderOrLegacyRenderSVGRoot())
            return { };
        // Nor does a flex container, whose content the render tree lays out like that of a leaf.
        if (renderer.type() == RenderObject::Type::FlexibleBox)
            return outOfFlowBoxesInLeafAvoidanceReason(downcast<RenderBox>(renderer), rootContainer);
    } else if (renderer.isInline())
        return inlineContentAvoidanceReason(renderer, rootContainer);
    // Block boxes only: not list items, replaced elements, tables, flex or grid containers or form controls.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(renderer);
    if (renderer.isRenderListItem())
        return BlockAvoidanceReason::ContentIsListItem;
    if (!blockContainer || blockContainer->type() != RenderObject::Type::BlockFlow)
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    // Form controls and legends treat an auto width as fit-content (RenderBox::sizesLogicalWidthToFitContent).
    if (isAnyOf<HTMLButtonElement, HTMLInputElement, HTMLLegendElement, HTMLSelectElement, HTMLTextAreaElement>(blockContainer->element()))
        return BlockAvoidanceReason::ContentIsFormControl;
    if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(blockContainer->parent()); !blockContainer->isOutOfFlowPositioned() && (!parent || parent->childrenInline()))
        return BlockAvoidanceReason::ContentHasBlockInInline;
    if (blockContainer->createsNewFormattingContext() && !isSupportedBlockFormattingContextRoot(*blockContainer))
        return BlockAvoidanceReason::ContentHasFormattingContextRoot;
    // Its layer is positioned once layout is done, which also offsets a relatively or sticky positioned box without moving anything else
    // (CSS 2.2 §9.4.3, css-position-3 §3.4), and BlockLayout::updateRenderers updates its transform.

    CheckedRef style = blockContainer->style();
    // Lengths, percentages and calc(); not the sizing keywords or calc-size().
    auto isSupportedSize = [](auto& size) {
        return size.isSpecified() && !size.isCalcSize();
    };
    // A calc-size() answers to the keyword of its basis, auto included.
    auto isAuto = [](auto& size) {
        return size.isAuto() && !size.isCalcSize();
    };
    // Block sizes may also be min-content, max-content or fit-content, which are the automatic block size (css-sizing-3 §3.2). Min and max
    // block sizes resolve those to the content's block size, which only an automatic block size reaches.
    auto isSupportedBlockSize = [&](auto& size, bool resolvesToContentBlockSize) {
        return isSupportedSize(size) || (size.isIntrinsic() && !size.isCalcSize() && resolvesToContentBlockSize);
    };
    auto blockSizeIsAutomatic = isAuto(style->logicalHeight()) || (style->logicalHeight().isIntrinsic() && !style->logicalHeight().isCalcSize());
    // Inline sizes may also be min-content, max-content or fit-content, from the inline sizes of the content (css-sizing-3 §3.2).
    auto isSupportedInlineSize = [&](auto& size) {
        return isSupportedSize(size) || (size.isIntrinsic() && !size.isCalcSize());
    };
    if ((!isAuto(style->logicalWidth()) && !isSupportedInlineSize(style->logicalWidth()))
        || (!isAuto(style->logicalMinWidth()) && !isSupportedInlineSize(style->logicalMinWidth()))
        || (!style->logicalMaxWidth().isNone() && !isSupportedInlineSize(style->logicalMaxWidth()))
        || (!isAuto(style->logicalHeight()) && !isSupportedBlockSize(style->logicalHeight(), true))
        || (!isAuto(style->logicalMinHeight()) && !isSupportedBlockSize(style->logicalMinHeight(), blockSizeIsAutomatic))
        || (!style->logicalMaxHeight().isNone() && !isSupportedBlockSize(style->logicalMaxHeight(), blockSizeIsAutomatic)))
        return BlockAvoidanceReason::ContentHasSizingKeyword;
    if (Layout::BlockFormattingUtils::hasIntrinsicInlineSize(*blockContainer, blockContainer->writingMode()) && !canComputeContentInlineSizes(*blockContainer))
        return BlockAvoidanceReason::ContentHasUnmeasurableIntrinsicInlineSize;
    if (blockContainer->shouldApplySizeContainment())
        return BlockAvoidanceReason::ContentHasSizeContainment;
    // A preferred aspect ratio gives an auto block size from the inline size.
    // FIXME: An inline size from a definite block size and the min and max sizes the ratio transfers between the axes (css-sizing-4 §5.1), and
    // percentage block sizes of the children, which the render tree path resolves against the size the ratio gives.
    if (style->aspectRatio().hasRatio()) {
        if (!style->logicalHeight().isAuto() || !style->logicalMinWidth().isAuto() || !style->logicalMaxWidth().isNone() || !style->logicalMinHeight().isAuto() || !style->logicalMaxHeight().isNone())
            return BlockAvoidanceReason::ContentHasAspectRatio;
        for (CheckedRef child : childrenOfType<RenderBox>(*blockContainer)) {
            if (Layout::BlockFormattingUtils::hasPercentageBlockSize(child, blockContainer->writingMode()))
                return BlockAvoidanceReason::ContentHasAspectRatio;
        }
    }
    // In quirks mode a percentage height resolves against the nearest ancestor with a height (RenderBox::computePercentageLogicalHeight).
    if (blockContainer->document().inQuirksMode() && (style->logicalHeight().isPercentOrCalculated() || style->logicalMinHeight().isPercentOrCalculated() || style->logicalMaxHeight().isPercentOrCalculated()))
        return BlockAvoidanceReason::ContentHasQuirksModePercentageHeight;
    // Quirks mode: every body element is a quirk container (RenderBlockFlow::MarginInfo), but the block formatting context keeps track of only one.
    if (Layout::BlockMarginCollapse::isQuirkContainer(*blockContainer)) {
        for (CheckedRef ancestor : ancestorsOfType<RenderBlockFlow>(*blockContainer)) {
            if (Layout::BlockMarginCollapse::isQuirkContainer(ancestor))
                return BlockAvoidanceReason::ContentHasNestedQuirkContainer;
        }
    }

    // The layout state has no entry for the boxes inside the block formatting context, which is where line-clamp, text-box-trim and line
    // grids reach the inline content below them.
    if (!style->marginTrim().isNone())
        return BlockAvoidanceReason::ContentHasMarginTrim;
    if (alignsBlockLevelChildren(style.get()))
        return BlockAvoidanceReason::ContentHasWebKitTextAlign;
    if (!style->maxLines().isAuto())
        return BlockAvoidanceReason::ContentHasLineClamp;
    if (style->textBoxTrim() != TextBoxTrim::None)
        return BlockAvoidanceReason::ContentHasTextBoxTrim;
    if (!style->lineGrid().isNone())
        return BlockAvoidanceReason::ContentHasLineGrid;
    CheckedRef parentStyle = blockContainer->parent()->style();
    if (!style->justifySelf().resolve(parentStyle.ptr()).isNormalStretchOrLegacy())
        return BlockAvoidanceReason::ContentHasJustifySelf;
    return { };
}

static std::optional<BlockAvoidanceReason> descendantsAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    // Every descendant is a block box that takes part in the root's block formatting context, or inline content of one. The content of an
    // out-of-flow box laid out by a containing block outside the root, or of a box laid out through the render tree, is not part of it.
    for (CheckedPtr<const RenderObject> descendant = rootContainer.firstChild(); descendant;) {
        CheckedPtr renderer = dynamicDowncast<RenderElement>(*descendant);
        auto laidOutThroughRenderTree = renderer && isLaidOutThroughRenderTree(*renderer);
        if (renderer) {
            if (auto reason = laidOutThroughRenderTree ? independentFormattingContextAvoidanceReason(downcast<RenderBox>(*renderer), rootContainer) : contentAvoidanceReason(*renderer, rootContainer))
                return reason;
        }
        auto isOutOfFlowBox = renderer && renderer->isOutOfFlowPositioned();
        // The content of an atomic inline-level box or of an out-of-flow flex container is not part of it either.
        auto skipsContent = (isOutOfFlowBox && (!isLaidOutByBlockFormattingContext(*renderer, rootContainer) || renderer->type() == RenderObject::Type::FlexibleBox))
            || laidOutThroughRenderTree || isAtomicInlineLevelBox(*descendant) || descendant->isRenderReplaced();
        descendant = skipsContent ? descendant->nextInPreOrderAfterChildren(&rootContainer) : descendant->nextInPreOrder(&rootContainer);
    }
    return { };
}

static std::optional<BlockAvoidanceReason> blockLayoutAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    if (auto reason = rootAvoidanceReason(rootContainer))
        return reason;
    return descendantsAvoidanceReason(rootContainer);
}

bool canUseForNestedBlockLayout(const RenderBlockFlow& blockContainer)
{
    return isSupportedBlockFormattingContextRoot(blockContainer) && !descendantsAvoidanceReason(blockContainer);
}

bool canUseForBlockLayout(const RenderBlockFlow& rootContainer)
{
    return !blockLayoutAvoidanceReason(rootContainer);
}

CheckedPtr<RenderBlockFlow> coveredBlockFormattingContextRoot(RenderBlock& blockContainer)
{
    for (CheckedPtr<RenderElement> ancestor = &blockContainer; ancestor; ancestor = ancestor->parent()) {
        CheckedPtr ancestorBlockContainer = dynamicDowncast<RenderBlockFlow>(*ancestor);
        if (!ancestorBlockContainer)
            return nullptr;
        if (!ancestorBlockContainer->createsNewFormattingContext())
            continue;
        if (canUseForBlockLayout(*ancestorBlockContainer))
            return ancestorBlockContainer;
        // An in-flow root whose content the block formatting context it is in lays out.
        if (ancestorBlockContainer->isOutOfFlowPositioned() || !canUseForNestedBlockLayout(*ancestorBlockContainer))
            return nullptr;
    }
    return nullptr;
}

bool canUseForIntrinsicInlineSizes(const RenderBlockFlow& rootContainer)
{
    return canUseForBlockLayout(rootContainer) && canComputeContentInlineSizes(rootContainer);
}

static ASCIILiteral description(BlockAvoidanceReason reason)
{
    switch (reason) {
    case BlockAvoidanceReason::BlockFormattingContextIntegrationDisabled:
        return "disabled"_s;
    case BlockAvoidanceReason::NotABlockFormattingContextRoot:
        return "not a root"_s;
    case BlockAvoidanceReason::RootIsRenderView:
        return "root: render view"_s;
    case BlockAvoidanceReason::RootIsListItem:
        return "root: list item"_s;
    case BlockAvoidanceReason::RootIsTableCell:
        return "root: table cell"_s;
    case BlockAvoidanceReason::RootIsMultiColumnFlow:
        return "root: multi-column flow"_s;
    case BlockAvoidanceReason::RootIsFieldset:
        return "root: fieldset"_s;
    case BlockAvoidanceReason::RootIsMarquee:
        return "root: marquee"_s;
    case BlockAvoidanceReason::RootHasUnsupportedRenderer:
        return "root: other renderer"_s;
    case BlockAvoidanceReason::RootIsAnonymousFlexOrGridItem:
        return "root: anonymous flex or grid item"_s;
    case BlockAvoidanceReason::RootHasUnsupportedWritingMode:
        return "root: vertical or horizontal-bt writing mode"_s;
    case BlockAvoidanceReason::RootIsPaginated:
        return "root: paginated"_s;
    case BlockAvoidanceReason::RootIsInLineClampContext:
        return "root: in line-clamp"_s;
    case BlockAvoidanceReason::RootIsInTextBoxTrimContext:
        return "root: in text-box-trim"_s;
    case BlockAvoidanceReason::RootIsInLineGridContext:
        return "root: in line grid"_s;
    case BlockAvoidanceReason::RootHasColumns:
        return "root: columns"_s;
    case BlockAvoidanceReason::RootHasAlignContent:
        return "root: align-content"_s;
    case BlockAvoidanceReason::RootHasMarginTrim:
        return "root: margin-trim"_s;
    case BlockAvoidanceReason::RootHasStableBothEdgesScrollbarGutter:
        return "root: scrollbar-gutter both-edges"_s;
    case BlockAvoidanceReason::RootHasWebKitTextAlign:
        return "root: -webkit- text-align"_s;
    case BlockAvoidanceReason::ContentHasFloat:
        return "float"_s;
    case BlockAvoidanceReason::ContentIsHiddenByLineClamp:
        return "hidden by line-clamp"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowBox:
        return "out-of-flow box in inline or leaf content"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowNonBlockContainer:
        return "out-of-flow grid, table or other renderer"_s;
    case BlockAvoidanceReason::ContentIsListItem:
        return "list item"_s;
    case BlockAvoidanceReason::ContentIsFormControl:
        return "form control or legend"_s;
    case BlockAvoidanceReason::ContentHasStretchedQuirksModeBody:
        return "quirks mode body leaf"_s;
    case BlockAvoidanceReason::ContentHasUnsupportedRenderer:
        return "other renderer"_s;
    case BlockAvoidanceReason::ContentHasFormattingContextRoot:
        return "formatting context root (contain, columns, ...)"_s;
    case BlockAvoidanceReason::ContentHasLayer:
        return "inline box with a layer"_s;
    case BlockAvoidanceReason::ContentHasBlockInInline:
        return "block-in-inline"_s;
    case BlockAvoidanceReason::ContentHasUnsupportedInlineBox:
        return "ruby, first-letter or other inline box"_s;
    case BlockAvoidanceReason::ContentHasOrthogonalFlow:
        return "orthogonal writing mode"_s;
    case BlockAvoidanceReason::ContentHasSizingKeyword:
        return "sizing keyword or calc-size()"_s;
    case BlockAvoidanceReason::ContentHasStretchBlockSize:
        return "stretch block size"_s;
    case BlockAvoidanceReason::ContentHasAspectRatio:
        return "aspect-ratio"_s;
    case BlockAvoidanceReason::ContentHasSizeContainment:
        return "size containment"_s;
    case BlockAvoidanceReason::ContentHasUnmeasurableIntrinsicInlineSize:
        return "intrinsic size of unmeasurable content"_s;
    case BlockAvoidanceReason::ContentHasQuirksModePercentageHeight:
        return "quirks mode percentage height"_s;
    case BlockAvoidanceReason::ContentHasNestedQuirkContainer:
        return "nested quirk container"_s;
    case BlockAvoidanceReason::ContentHasAnchorPositioning:
        return "anchor positioning"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowSelfAlignment:
        return "out-of-flow self alignment"_s;
    case BlockAvoidanceReason::ContentHasFlippedBlockWritingMode:
        return "horizontal-bt out-of-flow box"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowScrollContainer:
        return "out-of-flow scroll container"_s;
    case BlockAvoidanceReason::ContentHasMarginTrim:
        return "margin-trim"_s;
    case BlockAvoidanceReason::ContentHasWebKitTextAlign:
        return "-webkit- text-align"_s;
    case BlockAvoidanceReason::ContentHasLineClamp:
        return "line-clamp"_s;
    case BlockAvoidanceReason::ContentHasTextBoxTrim:
        return "text-box-trim"_s;
    case BlockAvoidanceReason::ContentHasLineGrid:
        return "line grid"_s;
    case BlockAvoidanceReason::ContentHasJustifySelf:
        return "justify-self"_s;
    case BlockAvoidanceReason::ContentHasBlockStepSize:
        return "block-step-size"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

BlockLayoutCoverage computeBlockLayoutCoverage(const RenderView& renderView)
{
    // A block-level box is placed by the block formatting context of its block container, which the new block formatting context lays
    // out when the root of that block formatting context is covered.
    struct BlockFormattingContext {
        // Why its root stays on the render tree path, or the root when the new block formatting context lays it out.
        std::optional<BlockAvoidanceReason> avoidanceReason;
        CheckedPtr<const RenderBlockFlow> coveredRoot;
        size_t areaIndex { 0 };
    };
    // The area of each block formatting context: its root's border box without the roots of the block formatting contexts inside it, which
    // may overflow it. RenderView's is the canvas around the document, not block content.
    auto areas = Vector<std::pair<double, bool>> { };
    auto area = [](const LayoutRect& rect) {
        return rect.width().toDouble() * rect.height().toDouble();
    };
    auto blockFormattingContextOf = [&](const RenderBlockFlow& root, std::optional<size_t> enclosingAreaIndex) {
        auto avoidanceReason = blockLayoutAvoidanceReason(root);
        auto rootArea = is<RenderView>(root) ? 0. : area(root.borderBoxRect());
        if (enclosingAreaIndex)
            areas[*enclosingAreaIndex].first -= rootArea;
        areas.append({ rootArea, !avoidanceReason });
        return BlockFormattingContext { avoidanceReason, avoidanceReason ? nullptr : &root, areas.size() - 1 };
    };
    auto coverage = BlockLayoutCoverage { };
    auto uncoveredBoxes = Vector<std::pair<BlockAvoidanceReason, unsigned>> { };
    auto renderersToVisit = Vector<std::pair<CheckedRef<const RenderElement>, BlockFormattingContext>> { { renderView, blockFormattingContextOf(renderView, { }) } };
    while (!renderersToVisit.isEmpty()) {
        auto [renderer, blockFormattingContext] = renderersToVisit.takeLast();
        auto& avoidanceReason = blockFormattingContext.avoidanceReason;
        for (CheckedRef child : childrenOfType<RenderElement>(renderer)) {
            if (is<RenderBlockFlow>(renderer) && is<RenderBox>(child) && !child->isInline()) {
                ++coverage.boxes;
                if (!avoidanceReason)
                    ++coverage.coveredBoxes;
                else {
                    auto index = uncoveredBoxes.findIf([&](auto& entry) {
                        return entry.first == *avoidanceReason;
                    });
                    if (index != notFound)
                        ++uncoveredBoxes[index].second;
                    else
                        uncoveredBoxes.append({ *avoidanceReason, 1 });
                }
            }
            CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(child);
            if (!blockContainer || !blockContainer->createsNewFormattingContext()) {
                renderersToVisit.append({ child, blockFormattingContext });
                continue;
            }
            // The block formatting context a root is in lays out its content when it supports the root: an in-flow block-level box of its
            // own, or an out-of-flow box whose containing block it lays out.
            auto isLaidOutByEnclosingBlockFormattingContext = [&] {
                if (!blockFormattingContext.coveredRoot || !canUseForNestedBlockLayout(*blockContainer))
                    return false;
                if (blockContainer->isOutOfFlowPositioned()) {
                    CheckedRef coveredRoot = *blockFormattingContext.coveredRoot;
                    return isLaidOutByBlockFormattingContext(*blockContainer, coveredRoot);
                }
                return is<RenderBlockFlow>(renderer) && !blockContainer->isInline() && !blockContainer->isFloating();
            };
            renderersToVisit.append({ child, isLaidOutByEnclosingBlockFormattingContext() ? blockFormattingContext : blockFormattingContextOf(*blockContainer, blockFormattingContext.areaIndex) });
        }
    }
    for (auto [contextArea, isCovered] : areas) {
        contextArea = std::max(0., contextArea);
        coverage.area += contextArea;
        if (isCovered)
            coverage.coveredArea += contextArea;
    }
    std::ranges::sort(uncoveredBoxes, [](auto& a, auto& b) {
        return a.second > b.second;
    });
    coverage.uncoveredBoxes = uncoveredBoxes.map([](auto& entry) {
        return std::pair { description(entry.first), entry.second };
    });
    return coverage;
}

} // namespace LayoutIntegration
} // namespace WebCore
