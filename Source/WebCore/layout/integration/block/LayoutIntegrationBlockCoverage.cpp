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
    RootHasUnsupportedRenderer,
    RootIsFlexOrGridItem,
    RootHasInlineContent,
    RootHasUnsupportedWritingMode,
    RootIsPaginated,
    RootIsInLineClampContext,
    RootIsInTextBoxTrimContext,
    RootIsInLineGridContext,
    RootHasUnsupportedStyle,
    ContentHasFloat,
    ContentIsHiddenByLineClamp,
    ContentHasOutOfFlowBox,
    ContentHasUnsupportedRenderer,
    ContentHasFormattingContextRoot,
    ContentHasLayer,
    ContentHasStickyPositioning,
    ContentHasBlockInInline,
    ContentHasAtomicInlineLevelBox,
    ContentHasUnsupportedInlineBox,
    ContentHasUnsupportedSize,
    ContentHasQuirksModePercentageHeight,
    ContentHasNestedQuirkContainer,
    ContentHasUnsupportedStyle
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
    if (rootContainer.type() != RenderObject::Type::BlockFlow || rootContainer.isFieldset() || rootContainer.isHTMLMarquee())
        return BlockAvoidanceReason::RootHasUnsupportedRenderer;
    if (rootContainer.isFlexItemIncludingDeprecated() || rootContainer.isGridItem())
        return BlockAvoidanceReason::RootIsFlexOrGridItem;
    if (rootContainer.childrenInline())
        return BlockAvoidanceReason::RootHasInlineContent;
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
    if (style->specifiesColumns() || rootContainer.multiColumnFlow() || !style->alignContent().isNormal() || !style->marginTrim().isNone()
        || style->scrollbarGutter().isStableBothEdges() || alignsBlockLevelChildren(style.get()))
        return BlockAvoidanceReason::RootHasUnsupportedStyle;
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

// The inline formatting context lays out an atomic inline-level box through the render tree, with the containing block size the block
// formatting context gives it (BlockIntegrationUtils::layoutInlineContent).
static std::optional<BlockAvoidanceReason> atomicInlineLevelBoxAvoidanceReason(const RenderBox& box, const RenderBlockFlow& rootContainer)
{
    // In quirks mode a percentage height inside it looks through it for an ancestor with a height (RenderBox::computePercentageLogicalHeight),
    // past the containing block size the block formatting context gives it.
    if (box.document().inQuirksMode())
        return BlockAvoidanceReason::ContentHasAtomicInlineLevelBox;
    // Scrollbars that come or go in a scroll container lay its ancestors out again (RenderObject::markContainingBlocksForLayout).
    CheckedRef style = box.style();
    auto isScrollable = [](Overflow overflow) {
        return overflow == Overflow::Auto || overflow == Overflow::Scroll;
    };
    if (box.isWritingModeRoot() || isScrollable(style->overflowX()) || isScrollable(style->overflowY()))
        return BlockAvoidanceReason::ContentHasAtomicInlineLevelBox;
    // An out-of-flow box inside whose containing block is outside it, in the block formatting context, would be registered on a box the
    // block formatting context has not committed yet.
    for (CheckedRef descendant : descendantsOfType<RenderBox>(box)) {
        if (!descendant->isOutOfFlowPositioned())
            continue;
        CheckedPtr containingBlock = descendant->containingBlock();
        if (containingBlock != &box && !containingBlock->isDescendantOf(&box) && isLaidOutByBlockFormattingContext(descendant, rootContainer))
            return BlockAvoidanceReason::ContentHasOutOfFlowBox;
    }
    return { };
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
// (BlockIntegrationUtils::layoutIndependentFormattingContext): a table, a flex or grid container, or a block formatting context root whose
// content the block formatting context cannot lay out.
static bool isLaidOutThroughRenderTree(const RenderObject& renderer)
{
    if (renderer.isInline() || renderer.isFloatingOrOutOfFlowPositioned())
        return false;
    switch (renderer.type()) {
    case RenderObject::Type::Table:
    case RenderObject::Type::FlexibleBox:
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
    // Block containers only, which establish a block formatting context for their content.
    if (outOfFlowBox.type() != RenderObject::Type::BlockFlow)
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    CheckedRef style = outOfFlowBox.style();
    // FIXME: Anchor positioning, position-area and position-try fallbacks, and alignment other than normal (css-position-3 §4.5).
    if (Style::AnchorPositionEvaluator::isAnchorPositioned(style.get()) || !style->positionTryFallbacks().isNone()
        || !(style->justifySelf().isAuto() || style->justifySelf().isNormal()) || !(style->alignSelf().isAuto() || style->alignSelf().isNormal()))
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    // FIXME: A flipped block flow direction (horizontal-bt), where the block-start inset is the physical bottom one and the render tree
    // stores flipped block positions.
    if (outOfFlowBox.writingMode().isBlockFlipped())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    // A scroll container with a definite size is a layout boundary, which the render tree path lays out on its own (RenderObject::markContainingBlocksForLayout).
    if (outOfFlowBox.hasNonVisibleOverflow())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    // FIXME: The sizing keywords and calc-size() (css-sizing-3 §3.2); lengths, percentages and calc() otherwise.
    auto isSupportedSize = [](auto& size) {
        return size.isSpecified() && !size.isCalcSize();
    };
    if (!(style->width().isAuto() || isSupportedSize(style->width())) || !(style->height().isAuto() || isSupportedSize(style->height()))
        || !(style->minWidth().isAuto() || isSupportedSize(style->minWidth())) || !(style->minHeight().isAuto() || isSupportedSize(style->minHeight()))
        || !(style->maxWidth().isNone() || isSupportedSize(style->maxWidth())) || !(style->maxHeight().isNone() || isSupportedSize(style->maxHeight())))
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
    // An auto inline size with an auto inset is the fit-content size (css-position-3 §4.3), from the inline sizes of the content.
    auto writingMode = outOfFlowBox.writingMode();
    auto hasFitContentInlineSize = style->logicalWidth(writingMode).isAuto() && (style->insetStart(writingMode).isAuto() || style->insetEnd(writingMode).isAuto());
    if (hasFitContentInlineSize && !canComputeContentInlineSizes(downcast<RenderBlockFlow>(outOfFlowBox)))
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
    // FIXME: The automatic size a preferred aspect ratio gives in the other axis (css-sizing-4 §5.1).
    if (style->aspectRatio().hasRatio())
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
    return { };
}

static std::optional<BlockAvoidanceReason> independentFormattingContextAvoidanceReason(const RenderBox& box, const RenderBlockFlow& rootContainer)
{
    if (box.isForceHiddenByLineClamp())
        return BlockAvoidanceReason::ContentIsHiddenByLineClamp;
    // Form controls are flex containers with content of their own.
    if (isAnyOf<HTMLButtonElement, HTMLInputElement, HTMLSelectElement, HTMLTextAreaElement>(box.element()))
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(box.parent()); !parent || parent->childrenInline())
        return BlockAvoidanceReason::ContentHasBlockInInline;
    // In quirks mode a percentage height inside it looks through it for an ancestor with a height (RenderBox::computePercentageLogicalHeight),
    // past the containing block size the block formatting context gives it.
    if (box.document().inQuirksMode())
        return BlockAvoidanceReason::ContentHasQuirksModePercentageHeight;
    // Scrollbars that come or go in a scroll container lay its ancestors out again (RenderObject::markContainingBlocksForLayout).
    if (box.isWritingModeRoot() || box.hasNonVisibleOverflow())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    CheckedRef style = box.style();
    auto writingMode = box.writingMode();
    // The block formatting context decides the inline size from lengths, percentages and calc() (BlockFormattingUtils::inlineSizeAndMargins).
    // FIXME: The sizing keywords, calc-size(), the stretch block size, which reads the containing block's renderer, and a preferred aspect ratio.
    auto isSupportedInlineSize = [](auto& size) {
        return (size.isAuto() || size.isSpecified()) && !size.isCalcSize();
    };
    auto isStretch = [](auto& size) {
        return size.isStretch();
    };
    if (!isSupportedInlineSize(style->logicalWidth(writingMode)) || !isSupportedInlineSize(style->logicalMinWidth(writingMode))
        || !(style->logicalMaxWidth(writingMode).isNone() || (style->logicalMaxWidth(writingMode).isSpecified() && !style->logicalMaxWidth(writingMode).isCalcSize()))
        || isStretch(style->logicalHeight(writingMode)) || isStretch(style->logicalMinHeight(writingMode)) || isStretch(style->logicalMaxHeight(writingMode))
        || style->aspectRatio().hasRatio())
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
    // The block-step-size rounding of the margin box is the parent's job on the render tree path (RenderBlockFlow::performBlockStepSizing).
    CheckedRef parentStyle = box.parent()->style();
    if (style->clear() != Clear::None || !style->justifySelf().resolve(parentStyle.ptr()).isNormalStretchOrLegacy() || !style->blockStepSize().isNone())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    // An out-of-flow box inside whose containing block is outside it, in the block formatting context, would be registered on a box the
    // block formatting context has not committed yet.
    for (CheckedRef descendant : descendantsOfType<RenderBox>(box)) {
        if (!descendant->isOutOfFlowPositioned())
            continue;
        CheckedPtr containingBlock = descendant->containingBlock();
        if (containingBlock != &box && !containingBlock->isDescendantOf(&box) && isLaidOutByBlockFormattingContext(descendant, rootContainer))
            return BlockAvoidanceReason::ContentHasOutOfFlowBox;
    }
    return { };
}

static std::optional<BlockAvoidanceReason> contentAvoidanceReason(const RenderElement& renderer, const RenderBlockFlow& rootContainer)
{
    // A box a line-clamp container hid after its clamp point stays hidden until the render tree path lays it out again
    // (RenderBlockFlow::layoutBlockChild, markInlineContentDirtyForLayout).
    if (renderer.isForceHiddenByLineClamp())
        return BlockAvoidanceReason::ContentIsHiddenByLineClamp;
    if (renderer.isFloating())
        return BlockAvoidanceReason::ContentHasFloat;
    if (renderer.isOutOfFlowPositioned()) {
        // FIXME: An inline-level out-of-flow box gets its static position from the inline formatting context.
        if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(renderer.parent()); !parent || parent->childrenInline())
            return BlockAvoidanceReason::ContentHasOutOfFlowBox;
        // A containing block outside the root lays it out through the render tree path.
        if (!isLaidOutByBlockFormattingContext(renderer, rootContainer))
            return { };
        if (auto reason = outOfFlowBoxAvoidanceReason(downcast<RenderBox>(renderer)))
            return reason;
    } else if (renderer.isInline())
        return inlineContentAvoidanceReason(renderer, rootContainer);
    // Block boxes only: not list items, replaced elements, tables, flex or grid containers or form controls.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(renderer);
    if (!blockContainer || blockContainer->type() != RenderObject::Type::BlockFlow)
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    // Form controls and legends treat an auto width as fit-content (RenderBox::sizesLogicalWidthToFitContent).
    if (isAnyOf<HTMLButtonElement, HTMLInputElement, HTMLLegendElement, HTMLSelectElement, HTMLTextAreaElement>(blockContainer->element()))
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(blockContainer->parent()); !parent || parent->childrenInline())
        return BlockAvoidanceReason::ContentHasBlockInInline;
    if (blockContainer->createsNewFormattingContext() && !isSupportedBlockFormattingContextRoot(*blockContainer))
        return BlockAvoidanceReason::ContentHasFormattingContextRoot;
    // Its layer is positioned once layout is done, which also offsets a relatively positioned box without moving anything else (CSS 2.2 §9.4.3),
    // and BlockLayout::updateRenderers updates its transform.
    // FIXME: Sticky positioning.
    if (blockContainer->isStickilyPositioned())
        return BlockAvoidanceReason::ContentHasStickyPositioning;

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
        || (Layout::BlockFormattingUtils::hasIntrinsicInlineSize(*blockContainer, blockContainer->writingMode()) && !canComputeContentInlineSizes(*blockContainer))
        || (!isAuto(style->logicalHeight()) && !isSupportedBlockSize(style->logicalHeight(), true))
        || (!isAuto(style->logicalMinHeight()) && !isSupportedBlockSize(style->logicalMinHeight(), blockSizeIsAutomatic))
        || (!style->logicalMaxHeight().isNone() && !isSupportedBlockSize(style->logicalMaxHeight(), blockSizeIsAutomatic))
        || blockContainer->shouldApplySizeContainment())
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
    // A preferred aspect ratio gives an auto block size from the inline size.
    // FIXME: An inline size from a definite block size and the min and max sizes the ratio transfers between the axes (css-sizing-4 §5.1), and
    // percentage block sizes of the children, which the render tree path resolves against the size the ratio gives.
    if (style->aspectRatio().hasRatio()) {
        if (!style->logicalHeight().isAuto() || !style->logicalMinWidth().isAuto() || !style->logicalMaxWidth().isNone() || !style->logicalMinHeight().isAuto() || !style->logicalMaxHeight().isNone())
            return BlockAvoidanceReason::ContentHasUnsupportedSize;
        for (CheckedRef child : childrenOfType<RenderBox>(*blockContainer)) {
            if (Layout::BlockFormattingUtils::hasPercentageBlockSize(child, blockContainer->writingMode()))
                return BlockAvoidanceReason::ContentHasUnsupportedSize;
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

    // Empty editable boxes get a line (RenderBlock::hasLineIfEmpty). The layout state has no entry for the boxes inside the block
    // formatting context, which is where line-clamp, text-box-trim and line grids reach the inline content below them.
    if (style->clear() != Clear::None || !style->marginTrim().isNone() || alignsBlockLevelChildren(style.get()) || blockContainer->hasLineIfEmpty()
        || !style->maxLines().isAuto() || style->textBoxTrim() != TextBoxTrim::None || !style->lineGrid().isNone())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
    CheckedRef parentStyle = blockContainer->parent()->style();
    if (!style->justifySelf().resolve(parentStyle.ptr()).isNormalStretchOrLegacy())
        return BlockAvoidanceReason::ContentHasUnsupportedStyle;
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
        // The content of an atomic inline-level box is not part of it either.
        auto skipsContent = (isOutOfFlowBox && !isLaidOutByBlockFormattingContext(*renderer, rootContainer)) || laidOutThroughRenderTree || isAtomicInlineLevelBox(*descendant);
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

BlockLayoutCoverage computeBlockLayoutCoverage(const RenderView& renderView)
{
    // A block-level box is placed by the block formatting context of its block container, which the new block formatting context lays
    // out when the root of that block formatting context is covered.
    auto coverage = BlockLayoutCoverage { };
    auto renderersToVisit = Vector<std::pair<CheckedRef<const RenderElement>, bool>> { { renderView, false } };
    while (!renderersToVisit.isEmpty()) {
        auto [renderer, isCovered] = renderersToVisit.takeLast();
        for (CheckedRef child : childrenOfType<RenderElement>(renderer)) {
            if (is<RenderBlockFlow>(renderer) && is<RenderBox>(child) && !child->isInline()) {
                ++coverage.boxes;
                if (isCovered)
                    ++coverage.coveredBoxes;
            }
            CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(child);
            auto childrenAreCovered = blockContainer && blockContainer->createsNewFormattingContext() ? canUseForBlockLayout(*blockContainer) : isCovered;
            renderersToVisit.append({ child, childrenAreCovered });
        }
    }
    return coverage;
}

} // namespace LayoutIntegration
} // namespace WebCore
