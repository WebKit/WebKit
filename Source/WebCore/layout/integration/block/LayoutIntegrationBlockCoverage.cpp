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

#include "BlockFormattingUtils.h"
#include "Document.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace LayoutIntegration {

enum class BlockAvoidanceReason : uint8_t {
    BlockFormattingContextIntegrationDisabled,
    NotABlockFormattingContextRoot,
    RootIsMultiColumnFlow,
    RootHasUnsupportedRenderer,
    RootIsPaginated,
    ContentHasOutOfFlowBox,
    ContentHasOutOfFlowNonBlockContainer,
    ContentHasUnsupportedRenderer,
    ContentHasBlockInInline,
    ContentHasUnsupportedInlineBox,
};


// The content of a root laid out one fragmentainer at a time, when the block formatting context knows how to break all of it
// (BlockLayout::layout).
static bool canFragmentContent(const RenderBlockFlow& rootContainer)
{
    for (CheckedPtr<const RenderObject> descendant = rootContainer.firstChild(); descendant;) {
        // Inline content of text, line breaks and inline boxes, whose lines break between them.
        if (is<RenderText>(*descendant) || is<RenderLineBreak>(*descendant)) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        if (descendant->isInlineBox()) {
            descendant = descendant->nextInPreOrder(&rootContainer);
            continue;
        }
        CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
        if (!box)
            return false;
        // The multi-column flow of a multi-column container, whose content breaks into its own columns only, also in pages or in other columns
        // (RenderLayoutState::computePaginationInformation), and its column sets, which have no content.
        if (box->isRenderMultiColumnFlow() || box->isRenderMultiColumnSet()) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // A frameset, which is always an in-flow block-level box (html.css), and whose frames the render tree places in it without breaking
        // them (RenderFrameSet::positionFrames), so the fragmentainers slice it where it is, like a column set.
        if (box->isRenderFrameSet()) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Out-of-flow boxes, among block-level boxes and in inline content, whose static position moves with its line (LineLayout's
        // moveLinesInBlockDirection). One whose containing block is outside the root is laid out there from the static position the block
        // formatting context gives it in the strip of fragmentainers (mergeFragments), like in the render tree, which breaks its content
        // across pages, and not across the columns of a multi-column flow it is outside of. The content of one whose containing block is
        // inside the root breaks across the fragmentainers too (BlockLayout::layoutOutOfFlowBoxes), when it is a block container the block
        // formatting context lays out, from where it goes, also when that depends on where its content breaks (css-break-3 §5.5,
        // BlockFormattingContext::layoutOutOfFlowBox). Those in the top layer have the view for their containing block.
        if (box->isOutOfFlowPositioned()) {
            if (!box->containingBlock()->isDescendantOf(&rootContainer) || box->isUnsplittableForPagination()) {
                descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
                continue;
            }
        }
        // Floats, among block-level boxes or in inline content, in a containing block of either direction: those whose content does not
        // break (RenderLayoutState::isPaginated), which go to the next page as a whole, and block containers whose content the block formatting
        // context lays out, which breaks across the fragmentainers from where the float goes, as a flow of its own (css-break-3 §2.1,
        // BlockFormattingContext::handleFloat, and layoutContentOfFloatInInlineContent once the inline formatting context placed it), or goes
        // to the next fragmentainer whole when it avoids breaks inside and fits in one (goesWholeToNextFragmentainer), also in a line grid, which
        // its content snaps to from where the float goes (BlockFormattingContext::setLineGridOfFloatContent), and tables, flex containers, grid
        // containers and the other block containers, multi-column containers included, whose content the render tree breaks from where the
        // float goes (BlockFormattingContext::handleFloat and layoutFloatInInlineContentThroughRenderTree), also in a line grid, which one that
        // goes to the next fragmentainer keeps there (PushedFloat::RenderTreeLayout), and beside lines that snap to it.
        if (box->isFloating()) {
            if (box->isUnsplittableForPagination()) {
                descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
                continue;
            }
            CheckedPtr floatBlockContainer = dynamicDowncast<RenderBlockFlow>(*box);
            if (floatBlockContainer && canUseForFloatContentLayout(*floatBlockContainer)) {
                descendant = descendant->nextInPreOrder(&rootContainer);
                continue;
            }
            auto isLaidOutByRenderTree = isLaidOutByOwnRenderTreeLayout(*box) || floatBlockContainer;
            if (!isLaidOutByRenderTree)
                return false;
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Atomic inline-level boxes, which their lines take along, and whose content does not break (RenderLayoutState::isPaginated).
        if (box->isInline()) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Block-level boxes in inline content, whose content breaks at the end of a fragmentainer, which ends with their line
        // (BlockFormattingContext::layoutBlockLevelBoxInInlineContent).
        // Column spanners' placeholders, where multi-column lines end (BlockFormattingContext::placeColumnSpannerPlaceholder), also in or as a
        // block-level box in inline content, which ends the multi-column line with its line (endMultiColumnLineInInlineContent), and in a block
        // container with a block size of its own, which takes no parallel flow for its overflow (BlockFormattingContext::endsInFragmentainer).
        if (box->isRenderReplaced() || box->isRenderOrLegacyRenderSVGRoot() || is<RenderMultiColumnSpannerPlaceholder>(*box)) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Other content that cannot be split, form controls and writing mode roots included, which goes to the next fragmentainer whole when it
        // fits there and does not break otherwise (css-break-3 §4.4), and whose own content does not break (RenderLayoutState::isPaginated).
        if (box->isUnsplittableForPagination()) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Tables, flex containers (-webkit-box included), grid containers, MathML boxes, and progress and meter elements, form control block
        // containers that are not form controls (RenderBox::isUnsplittableForPagination), which the render tree lays out and breaks the content
        // of across the fragmentainers from where the block formatting context puts them (BlockIntegrationUtils::layoutIndependentFormattingContext).
        if (isLaidOutByOwnRenderTreeLayout(*box) || Layout::BlockFormattingUtils::isFormControlBlockContainer(*box)) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*box);
        // Multi-column containers, which the render tree lays out with columns of their own, and breaks across the columns around them from where
        // the block formatting context puts them, like a table (BlockIntegrationUtils::layoutIndependentFormattingContext). In pages they stay
        // whole, like monolithic content: the render tree has no new line of column boxes to continue one with on the next page (css-multicol-1
        // §2), and prints their content in a single column (RenderBlockFlow::computeColumnCountAndWidth).
        if (blockContainer && blockContainer->multiColumnFlow()) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // Block containers, list items included, and block formatting context roots whose content the block formatting
        // context lays out, which breaks like that of a block container (BlockFormattingContext::handleIndependentFormattingContextRoot).
        auto isBlockContainer = blockContainer && (blockContainer->type() == RenderObject::Type::BlockFlow || blockContainer->type() == RenderObject::Type::ListItem);
        if (!isBlockContainer)
            return false;
        // Block formatting context roots the block formatting context lays out through the render tree, like a table, whose content breaks
        // across the fragmentainers from where the block formatting context puts them, laid out one fragmentainer at a time on their own, or
        // by the render tree (BlockIntegrationUtils::layoutIndependentFormattingContext), out-of-flow ones included, from where they go in their
        // containing block (BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext). A spanner in one belongs to a multi-column
        // container inside it, since a spanner spans the columns of a multi-column container in its block formatting context only
        // (css-multicol-1 §6.1), which the render tree lays out with the root.
        if (blockContainer->createsNewFormattingContext() && !canUseForNestedBlockLayout(*blockContainer)) {
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        // A block container with a block size of its own, whose content overflowing it breaks alongside the content after it
        // (BlockFormattingContext::endFragmentainer), also in a block formatting context root whose content breaks like that of a block
        // container, where it goes on alongside the content after the root (addParallelFlowOfRoot), and in the columns of a paginated view
        // (Pagination::Mode), before a column spanner, where it goes on in columns past the last one of the multi-column line, and in or as a
        // block-level box in inline content, where it goes on alongside the inline content after the box (BlockFormattingContext::layoutParallelFlow).
        descendant = descendant->nextInPreOrder(&rootContainer);
    }
    return true;
}

// Printing: a root laid out one page at a time, the view or one the render tree lays out in the pages, a table cell, a flex or grid item, or
// an out-of-flow box whose containing block is outside the root, from where it starts on its first page, but not a root in a fragmented flow,
// whose pages are its columns.
static bool canFragmentIntoPages(const RenderBlockFlow& rootContainer, const RenderLayoutState& layoutState)
{
    if (rootContainer.enclosingFragmentedFlow() || !layoutState.pageLogicalHeight())
        return false;
    return canFragmentContent(rootContainer);
}

// Content in a box that cannot be split, or one the block formatting context keeps whole, inside the fragmented flow or pages around it, which
// the render tree does not break either (RenderLayoutState::computePaginationInformation), and which the block formatting context lays out
// like content that is not fragmented.
static bool isInBoxThatDoesNotBreak(const RenderBlockFlow& rootContainer)
{
    CheckedPtr fragmentedFlow = rootContainer.enclosingFragmentedFlow();
    CheckedRef layoutContext = rootContainer.view().frameView().layoutContext();
    for (CheckedPtr<const RenderBlock> box = &rootContainer; box && box != fragmentedFlow && !box->isRenderView(); box = box->containingBlock()) {
        if (box->isUnsplittableForPagination() || layoutContext->isMonolithicBox(*box))
            return true;
    }
    return false;
}

// The -webkit-left, -webkit-right and -webkit-center values of text-align move block-level children too (RenderBox::computeInlineDirectionMargins).
static std::optional<BlockAvoidanceReason> rootAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    if (!rootContainer.document().settings().blockFormattingContextIntegrationEnabled())
        return BlockAvoidanceReason::BlockFormattingContextIntegrationDisabled;
    // The view is the initial containing block, which the root element and the out-of-flow boxes it contains are laid out in (CSS 2.2 §10.1),
    // the rendered legend of a fieldset, which the fieldset sizes and places in its border (RenderBlock::layoutExcludedChildren), and which
    // establishes a formatting context for its content (HTML, the fieldset and legend elements), the other boxes excluded from the normal
    // flow their parent lays out, like a text control's placeholder, form control block containers, whose content is their own, and floats,
    // which establish a block formatting context (CSS 2.2 §9.4.1), anonymous ones included, like a ::first-letter float.
    auto isRoot = rootContainer.createsNewFormattingContext() || rootContainer.isRenderView() || rootContainer.isExcludedFromNormalLayout()
        || Layout::BlockFormattingUtils::isFormControlBlockContainer(rootContainer) || rootContainer.isFloating();
    if (!isRoot)
        return BlockAvoidanceReason::NotABlockFormattingContextRoot;
    // Not multi-column flows whose content the block formatting context does not break into columns.
    CheckedRef layoutContext = rootContainer.view().frameView().layoutContext();
    auto* layoutState = layoutContext->layoutState();
    // A multi-column flow is laid out one column at a time. Its columns are its only fragmentainers, also in pages or in other columns, like
    // in the render tree, where a fragmented flow starts pagination of its own (RenderLayoutState::computePaginationInformation). The
    // multi-column container balances the columns from what the block formatting context reports (RenderMultiColumnSet::recalculateColumnHeight).
    // One with out-of-flow boxes only has no column set, and its columns no block size (BlockLayout::layoutContent).
    CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(rootContainer);
    if (multiColumnFlow && !canFragmentContent(*multiColumnFlow))
        return BlockAvoidanceReason::RootIsMultiColumnFlow;
    // A text control lays out its inner text with the block formatting context and adjusts it after (RenderTextControlSingleLine::layout), and
    // its placeholder itself (RenderTextControl::layoutExcludedChildren). A WebVTT cue box moves itself after (RenderVTTCue::layout). A table
    // caption, a file input, an SVG foreignObject, and progress and meter elements, which the theme sizes (RenderProgress::computeLogicalHeight,
    // RenderMeter::updateLogicalWidth), only size and place themselves their own way.
    auto isSupportedRenderer = [&] {
        switch (rootContainer.type()) {
        case RenderObject::Type::View:
        case RenderObject::Type::BlockFlow:
        case RenderObject::Type::MultiColumnFlow:
        case RenderObject::Type::ListItem:
        case RenderObject::Type::TableCell:
        case RenderObject::Type::TableCaption:
        case RenderObject::Type::FileUploadControl:
        case RenderObject::Type::Progress:
        case RenderObject::Type::Meter:
        case RenderObject::Type::VTTCue:
        case RenderObject::Type::SVGForeignObject:
        case RenderObject::Type::LegacySVGForeignObject:
            return true;
        default:
            return rootContainer.isRenderTextControl() || rootContainer.isRenderTextControlInnerBlock();
        }
    };
    if (!isSupportedRenderer())
        return BlockAvoidanceReason::RootHasUnsupportedRenderer;
    if (!multiColumnFlow && !isInBoxThatDoesNotBreak(rootContainer)) {
        // A root the render tree lays out in a multi-column flow, a table cell, a flex or grid item or an out-of-flow box, whose content breaks
        // across the columns of the flow from where it starts in them, also in the columns of a paginated view (Pagination::Mode).
        if (is<RenderMultiColumnFlow>(rootContainer.enclosingFragmentedFlow())) {
            if (!canFragmentContent(rootContainer))
                return BlockAvoidanceReason::RootIsPaginated;
        } else if (rootContainer.enclosingFragmentedFlow() || (layoutState && layoutState->isPaginated() && !canFragmentIntoPages(rootContainer, *layoutState)))
            return BlockAvoidanceReason::RootIsPaginated;
    }
    return { };
}

// Not a fieldset, whose legend sits in its border, nor a multi-column container, paged overflow (overflow-y: -webkit-paged-x or -webkit-paged-y)
// included, which makes columns of the box too (RenderBlockFlow::willCreateColumns), or a column spanner.
static bool hasSupportedBlockContainerStyle(const RenderBlockFlow& blockContainer)
{
    CheckedRef style = blockContainer.style();
    return !blockContainer.isFieldset() && !style->specifiesColumns() && !blockContainer.multiColumnFlow() && style->columnSpan() != ColumnSpan::All && style->overflowContinue() == OverflowContinue::Normal;
}

// Only the root element, display: flow-root, overflow other than visible (paint containment included, which clips the overflow, see
// RenderElement::effectiveOverflowX), align-content, out-of-flow boxes and layout containment (css-contain-2 §3.2), content-visibility
// included. The other ways to establish a block formatting context also contain or fragment the content. Not a form control, which lays
// out its content its own way.
static bool isSupportedBlockFormattingContextRoot(const RenderBlockFlow& blockContainer)
{
    if (Layout::BlockFormattingUtils::isFormControlBlockContainer(blockContainer))
        return false;
    CheckedRef style = blockContainer.style();
    auto isSupportedRoot = blockContainer.isDocumentElementRenderer() || blockContainer.hasNonVisibleOverflow() || style->display() == Style::DisplayType::BlockFlowRoot || !style->alignContent().isNormal() || blockContainer.isOutOfFlowPositioned()
        || blockContainer.shouldApplyLayoutContainment();
    // An out-of-flow box sizes itself in its own writing mode (OutOfFlowGeometry).
    auto isSupportedWritingMode = !blockContainer.isWritingModeRoot() || blockContainer.isOutOfFlowPositioned();
    return isSupportedRoot && isSupportedWritingMode && hasSupportedBlockContainerStyle(blockContainer);
}

static bool isLaidOutByBlockFormattingContext(const RenderElement& outOfFlowBox, const RenderBlockFlow& rootContainer);

static bool isAtomicInlineLevelBox(const RenderObject& renderer)
{
    return renderer.isInline() && is<RenderBox>(renderer) && !is<RenderLineBreak>(renderer);
}

static std::optional<BlockAvoidanceReason> outOfFlowBoxesInLeafAvoidanceReason(const RenderBox& leaf, const RenderBlockFlow& rootContainer);

// The inline formatting context lays out an atomic inline-level box through the render tree, with the containing block size the block
// formatting context gives it (BlockIntegrationUtils::layoutInlineContent).
static std::optional<BlockAvoidanceReason> atomicInlineLevelBoxAvoidanceReason(const RenderBox& box, const RenderBlockFlow& rootContainer)
{
    return outOfFlowBoxesInLeafAvoidanceReason(box, rootContainer);
}

static std::optional<BlockAvoidanceReason> inlineContentAvoidanceReason(const RenderElement& renderer, const RenderBlockFlow& rootContainer)
{
    if (is<RenderLineBreak>(renderer))
        return { };
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer))
        return atomicInlineLevelBoxAvoidanceReason(*box, rootContainer);
    // Inline boxes, ruby ones and ::first-letter included, which the inline formatting context lays out.
    auto display = renderer.style().display();
    auto isInlineBox = display == Style::DisplayType::InlineFlow || display == Style::DisplayType::InlineRuby || display == Style::DisplayType::RubyBase || display == Style::DisplayType::RubyText;
    if (renderer.type() != RenderObject::Type::Inline || !isInlineBox)
        return BlockAvoidanceReason::ContentHasUnsupportedInlineBox;
    return { };
}

// BlockFormattingContext::computeIntrinsicInlineSizes measures inline content with the inline formatting context and the block-level boxes
// between them itself.
// A block-level independent formatting context root the block formatting context lays out through the render tree
// (BlockIntegrationUtils::layoutIndependentFormattingContext): a replaced element, a table, a button, a flex (-webkit-box included) or grid
// container, selects, range inputs and the text control's one around its inner text and decorations included, a form control block
// container, a frameset, which takes the size of the viewport (RenderFrameSet::layout), a MathML box or table, or a block formatting context
// root whose content the block formatting context cannot lay out.
static bool isLaidOutThroughRenderTree(const RenderObject& renderer)
{
    if (renderer.isInline() || renderer.isFloatingOrOutOfFlowPositioned())
        return false;
    if (renderer.isRenderReplaced() || renderer.isRenderOrLegacyRenderSVGRoot())
        return true;
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer); box && Layout::BlockFormattingUtils::isFormControlBlockContainer(*box))
        return true;
#if ENABLE(MATHML)
    if (renderer.isRenderMathMLBlock() || renderer.isRenderMathMLTable())
        return true;
#endif
    switch (renderer.type()) {
    case RenderObject::Type::Button:
    case RenderObject::Type::Table:
    case RenderObject::Type::FlexibleBox:
    case RenderObject::Type::MenuList:
    case RenderObject::Type::Slider:
    case RenderObject::Type::TextControlInnerContainer:
    case RenderObject::Type::DeprecatedFlexibleBox:
    case RenderObject::Type::Grid:
    case RenderObject::Type::FrameSet:
        return true;
    case RenderObject::Type::BlockFlow:
    case RenderObject::Type::ListItem:
    case RenderObject::Type::TextControlInnerBlock: {
        auto& blockContainer = downcast<RenderBlockFlow>(renderer);
        return blockContainer.createsNewFormattingContext() && !canUseForNestedBlockLayout(blockContainer);
    }
    default:
        return false;
    }
}

// The block formatting context lays out the out-of-flow boxes whose containing block it lays out.
static bool isLaidOutByBlockFormattingContext(const RenderElement& outOfFlowBox, const RenderBlockFlow& rootContainer)
{
    CheckedPtr containingBlock = outOfFlowBox.containingBlock();
    return containingBlock == &rootContainer || containingBlock->isDescendantOf(&rootContainer);
}

// An out-of-flow flex container, buttons, range inputs and selects included, and the flex containers in form controls (a range input's slider
// container, a text control's container of its inner text and decorations), grid container, -webkit-box, WebVTT cue box, table, form control
// block container or MathML box or table, whose own formatting context lays it out (BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext),
// and its content like that of a leaf. A fieldset among them puts its legend in its border itself (RenderBlock::layoutExcludedChildren), like a
// block container one.
static bool isOutOfFlowLeafContainer(const RenderElement& outOfFlowBox)
{
#if ENABLE(MATHML)
    if (outOfFlowBox.isRenderMathMLBlock() || outOfFlowBox.isRenderMathMLTable())
        return true;
#endif
    switch (outOfFlowBox.type()) {
    case RenderObject::Type::FlexibleBox:
    case RenderObject::Type::Button:
    case RenderObject::Type::Slider:
    case RenderObject::Type::SliderContainer:
    case RenderObject::Type::TextControlInnerContainer:
    case RenderObject::Type::MenuList:
    case RenderObject::Type::Grid:
    case RenderObject::Type::DeprecatedFlexibleBox:
    case RenderObject::Type::VTTCue:
    case RenderObject::Type::Table:
    case RenderObject::Type::ListBox:
    case RenderObject::Type::TextControlSingleLine:
    case RenderObject::Type::TextControlMultiLine:
    case RenderObject::Type::SearchField:
    case RenderObject::Type::Progress:
    case RenderObject::Type::Meter:
    case RenderObject::Type::FileUploadControl:
        return true;
    default:
        return false;
    }
}

// The block formatting context sizes and places an out-of-flow leaf container, or a block container whose content it cannot lay out as a
// nested root, and the render tree lays it out (BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext).
static bool isOutOfFlowLeaf(const RenderElement& outOfFlowBox)
{
    if (isOutOfFlowLeafContainer(outOfFlowBox))
        return true;
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(outOfFlowBox);
    return blockContainer && !canUseForNestedBlockLayout(*blockContainer);
}

static std::optional<BlockAvoidanceReason> outOfFlowBoxAvoidanceReason(const RenderBox& outOfFlowBox)
{
    // Block containers, which establish a block formatting context for their content, replaced elements, and the containers whose own
    // formatting context lays them out.
    auto isReplaced = outOfFlowBox.isRenderReplaced() || outOfFlowBox.isRenderOrLegacyRenderSVGRoot();
    auto isLeafContainer = isOutOfFlowLeafContainer(outOfFlowBox);
    auto isBlockContainer = outOfFlowBox.type() == RenderObject::Type::BlockFlow || outOfFlowBox.isRenderListItem();
    if (!isBlockContainer && !isReplaced && !isLeafContainer)
        return BlockAvoidanceReason::ContentHasOutOfFlowNonBlockContainer;
    return { };
}

// The render tree lays out the content of a leaf (an atomic inline-level box or a box laid out through the render tree), and registers the
// out-of-flow boxes it finds on their containing block (RenderBlock::addOutOfFlowBox). The block formatting context lays out the ones whose
// containing block it lays out, outside the leaf, from their static position (on their layer, or a flex container's), which it moves out of
// the leaf through the boxes in between (BlockLayout::layoutOutOfFlowBoxes): inline boxes, which add nothing to it, and boxes, out-of-flow ones
// it placed included, in any writing mode, multi-column flows included, where it is in the strip of the columns, like in the render tree
// (PositionedLayoutConstraints adds up the locations of the boxes in between).
static std::optional<BlockAvoidanceReason> outOfFlowBoxesInLeafAvoidanceReason(const RenderBox& leaf, const RenderBlockFlow& rootContainer)
{
    auto isSupportedBoxInBetween = [](const RenderElement& box) {
        return box.isInlineBox() || is<RenderBox>(box);
    };
    for (CheckedRef descendant : descendantsOfType<RenderBox>(leaf)) {
        if (!descendant->isOutOfFlowPositioned())
            continue;
        CheckedPtr containingBlock = descendant->containingBlock();
        if (containingBlock == &leaf || containingBlock->isDescendantOf(&leaf) || !isLaidOutByBlockFormattingContext(descendant, rootContainer))
            continue;
        if (auto reason = outOfFlowBoxAvoidanceReason(descendant))
            return reason;
        // Its static position comes from the block its parent is or is in, which puts it on its layer during its layout: a block container, a
        // grid container, which puts it at the start of its content box (RenderGrid::prepareGridItemForPositionedLayout), a flex container,
        // which aligns it in its content box, a -webkit-box, which puts it where its next child would go (RenderDeprecatedFlexibleBox::layoutVerticalBox,
        // layoutHorizontalBox), or a table or MathML box, which leaves it where it was (PositionedLayoutConstraints::computeStaticPosition).
        CheckedPtr staticPositionBlock = descendant->parent();
        while (staticPositionBlock->isInlineBox())
            staticPositionBlock = staticPositionBlock->parent();
        if (!is<RenderBlock>(*staticPositionBlock))
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
    // Block-level boxes in an inline box, anonymous ones included, like the block a ::checkmark of an input that is an inline box makes, but not
    // those of block containers that have inline children (AnonymousBlockGenerationDisabled).
    if (!box.parent()->isInlineBox()) {
        if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(box.parent()); !parent || parent->childrenInline())
            return BlockAvoidanceReason::ContentHasBlockInInline;
    }
    return outOfFlowBoxesInLeafAvoidanceReason(box, rootContainer);
}

// A float, which the inline formatting context places in inline content and the block formatting context among block-level boxes (CSS 2.2
// §9.5.1), and its own formatting context lays out through the render tree.
static std::optional<BlockAvoidanceReason> floatAvoidanceReason(const RenderBox& floatBox, const RenderBlockFlow& rootContainer)
{
    return atomicInlineLevelBoxAvoidanceReason(floatBox, rootContainer);
}

static std::optional<BlockAvoidanceReason> contentAvoidanceReason(const RenderElement& renderer, const RenderBlockFlow& rootContainer)
{
    // Floats outside the root stay out of its block formatting context.
    if (renderer.isFloating())
        return floatAvoidanceReason(downcast<RenderBox>(renderer), rootContainer);
    if (renderer.isOutOfFlowPositioned()) {
        // A containing block outside the root lays it out through the render tree path.
        if (!isLaidOutByBlockFormattingContext(renderer, rootContainer))
            return { };
        if (auto reason = outOfFlowBoxAvoidanceReason(downcast<RenderBox>(renderer)))
            return reason;
        // A replaced element has no block container checks to pass.
        if (renderer.isRenderReplaced() || renderer.isRenderOrLegacyRenderSVGRoot())
            return { };
        // Nor does a leaf container, whose content the render tree lays out.
        if (isOutOfFlowLeaf(renderer))
            return outOfFlowBoxesInLeafAvoidanceReason(downcast<RenderBox>(renderer), rootContainer);
    } else if (renderer.isInline())
        return inlineContentAvoidanceReason(renderer, rootContainer);
    // A column spanner's placeholder, where a multi-column line of a multi-column flow broken into columns ends
    // (BlockFormattingContext::placeColumnSpannerPlaceholder), which is only in the block formatting context of the flow (css-multicol-1 §6.1).
    if (is<RenderMultiColumnSpannerPlaceholder>(renderer))
        return { };
    // A column set of the multi-column container, which takes the block size of its columns (BlockFormattingContext::handleIndependentFormattingContextRoot).
    if (renderer.isRenderMultiColumnSet())
        return { };
    // Block boxes only: not replaced elements, tables, flex or grid containers or form control block containers.
    // A form control fits its content (BlockFormattingUtils::autoInlineSizeFitsContent). A WebVTT cue box in a region is a block container:
    // its own layout only moves the cues outside regions, which are out-of-flow (RenderVTTCue::layout).
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(renderer);
    if (!blockContainer || (blockContainer->type() != RenderObject::Type::BlockFlow && !blockContainer->isRenderListItem() && !blockContainer->isRenderTextControlInnerBlock() && !blockContainer->isRenderVTTCue()))
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    if (!blockContainer->isOutOfFlowPositioned()) {
        if (!blockContainer->parent()->isInlineBox()) {
            if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(blockContainer->parent()); !parent || parent->childrenInline())
                return BlockAvoidanceReason::ContentHasBlockInInline;
        }
    }
    // Its layer is positioned once layout is done, which also offsets a relatively or sticky positioned box without moving anything else
    // (CSS 2.2 §9.4.3, css-position-3 §3.4), and BlockLayout::updateRenderers updates its transform. The block formatting context sizes it for
    // every value of the sizing properties (BlockFormattingUtils::inlineSizeAndMargins, borderBoxBlockSize).
    return { };
}

static std::optional<BlockAvoidanceReason> descendantsAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    // Every descendant is a block box that takes part in the root's block formatting context, or inline content of one. The content of an
    // out-of-flow box laid out by a containing block outside the root, of a float, or of a box laid out through the render tree, is not part
    // of it.
    for (CheckedPtr<const RenderObject> descendant = rootContainer.firstChild(); descendant;) {
        CheckedPtr renderer = dynamicDowncast<RenderElement>(*descendant);
        // Its parent lays out a box excluded from normal layout, like a text control's placeholder, the multi-column flow of a multi-column
        // container (RenderBlock::layoutExcludedChildren) or the outside marker of a list item (RenderListItem::layoutOutsideMarker), but not
        // the out-of-flow boxes in the flow whose containing block is the container.
        if (CheckedPtr box = dynamicDowncast<RenderBox>(*descendant); box && (box->isExcludedFromNormalLayout() || box->isRenderMultiColumnFlow() || box->isOutsideListMarker())) {
            if (box->isRenderMultiColumnFlow()) {
                if (auto reason = outOfFlowBoxesInLeafAvoidanceReason(*box, rootContainer))
                    return reason;
            }
            descendant = descendant->nextInPreOrderAfterChildren(&rootContainer);
            continue;
        }
        auto laidOutThroughRenderTree = renderer && isLaidOutThroughRenderTree(*renderer);
        if (renderer) {
            if (auto reason = laidOutThroughRenderTree ? independentFormattingContextAvoidanceReason(downcast<RenderBox>(*renderer), rootContainer) : contentAvoidanceReason(*renderer, rootContainer))
                return reason;
        }
        auto isOutOfFlowBox = renderer && renderer->isOutOfFlowPositioned();
        // The content of an atomic inline-level box or of an out-of-flow leaf container is not part of it either, nor is the content
        // content-visibility skips, which no one lays out.
        CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
        auto skipsContent = (isOutOfFlowBox && (!isLaidOutByBlockFormattingContext(*renderer, rootContainer) || isOutOfFlowLeaf(*renderer)))
            || laidOutThroughRenderTree || isAtomicInlineLevelBox(*descendant) || descendant->isRenderReplaced() || (renderer && renderer->isFloating())
            || (box && rootContainer.layoutContext().isSkippedContentRootForLayout(*box));
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

static SingleThreadWeakHashMap<const RenderBlockFlow, bool>* nestedBlockLayoutAnswers;

NestedBlockLayoutCoverageScope::NestedBlockLayoutCoverageScope()
{
    ASSERT(isMainThread());
    if (nestedBlockLayoutAnswers)
        return;
    m_canUseForNestedBlockLayout.emplace();
    nestedBlockLayoutAnswers = &*m_canUseForNestedBlockLayout;
}

NestedBlockLayoutCoverageScope::~NestedBlockLayoutCoverageScope()
{
    if (m_canUseForNestedBlockLayout)
        nestedBlockLayoutAnswers = nullptr;
}

static bool canUseForContentOfRoot(const RenderBlockFlow& blockContainer)
{
    auto scope = NestedBlockLayoutCoverageScope { };
    auto& answers = *nestedBlockLayoutAnswers;
    if (auto answer = answers.find(blockContainer); answer != answers.end())
        return answer->value;
    auto answer = !descendantsAvoidanceReason(blockContainer);
    answers.set(blockContainer, answer);
    return answer;
}

bool canUseForNestedBlockLayout(const RenderBlockFlow& blockContainer)
{
    return isSupportedBlockFormattingContextRoot(blockContainer) && canUseForContentOfRoot(blockContainer);
}

bool isLaidOutByOwnRenderTreeLayout(const RenderBox& box)
{
#if ENABLE(MATHML)
    if (box.isRenderMathMLBlock() || box.isRenderMathMLTable())
        return true;
#endif
    auto type = box.type();
    return type == RenderObject::Type::Table || type == RenderObject::Type::FlexibleBox || type == RenderObject::Type::DeprecatedFlexibleBox || type == RenderObject::Type::Grid;
}

bool canUseForFloatContentLayout(const RenderBlockFlow& floatBox)
{
    if (!floatBox.isFloating() || floatBox.type() != RenderObject::Type::BlockFlow || Layout::BlockFormattingUtils::isFormControlBlockContainer(floatBox) || floatBox.isWritingModeRoot() || !hasSupportedBlockContainerStyle(floatBox))
        return false;
    return canUseForContentOfRoot(floatBox);
}

bool canUseForBlockLayout(const RenderBlockFlow& rootContainer)
{
    auto scope = NestedBlockLayoutCoverageScope { };
    return !blockLayoutAvoidanceReason(rootContainer);
}

CheckedPtr<RenderBlockFlow> coveredBlockFormattingContextRoot(RenderBlock& blockContainer)
{
    auto scope = NestedBlockLayoutCoverageScope { };
    for (CheckedPtr<RenderElement> ancestor = &blockContainer; ancestor; ancestor = ancestor->parent()) {
        CheckedPtr ancestorBlockContainer = dynamicDowncast<RenderBlockFlow>(*ancestor);
        if (!ancestorBlockContainer)
            return nullptr;
        if (!ancestorBlockContainer->createsNewFormattingContext() && !ancestorBlockContainer->isRenderView())
            continue;
        if (canUseForBlockLayout(*ancestorBlockContainer))
            return ancestorBlockContainer;
        // An in-flow root whose content the block formatting context it is in lays out.
        if (ancestorBlockContainer->isOutOfFlowPositioned() || !canUseForNestedBlockLayout(*ancestorBlockContainer))
            return nullptr;
    }
    return nullptr;
}

// A flex or grid container or a table gives a root it measures the block size the percentage block sizes in the root's content resolve against
// (GridTrackSizingAlgorithm, RenderTableSection::relayoutCellIfFlexed), which the root's intrinsic inline sizes depend on where a preferred aspect
// ratio turns such a percentage into an inline size: through the content box block sizes of the boxes in between, up to the root, unless one of
// them has a block size of its own.
bool intrinsicInlineSizesDependOnContainerGivenBlockSize(const RenderBlockFlow& root)
{
    if (!root.isFlexItemIncludingDeprecated() && !root.isGridItem() && !root.isRenderTableCell())
        return false;
    auto resolvesAgainstRoot = [&](const RenderBox& box) {
        for (CheckedPtr containingBlock = box.containingBlock(); containingBlock; containingBlock = containingBlock->containingBlock()) {
            if (containingBlock == &root)
                return true;
            CheckedRef style = containingBlock->style();
            auto writingMode = containingBlock->writingMode();
            if (!containingBlock->isAnonymous() && !Layout::BlockFormattingUtils::hasPercentageBlockSize(*containingBlock, writingMode) && (!style->logicalHeight(writingMode).isAuto() || !containingBlock->document().inQuirksMode()))
                return false;
        }
        return false;
    };
    for (CheckedPtr<const RenderObject> descendant = root.firstChild(); descendant;) {
        // Its own formatting context measures a leaf, an out-of-flow box adds nothing, and a float measures itself.
        if (descendant->isOutOfFlowPositioned() || descendant->isFloating() || isLaidOutThroughRenderTree(*descendant) || isAtomicInlineLevelBox(*descendant)) {
            descendant = descendant->nextInPreOrderAfterChildren(&root);
            continue;
        }
        if (CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*descendant); blockContainer && !blockContainer->isInline()) {
            if (blockContainer->style().aspectRatio().hasRatio() && Layout::BlockFormattingUtils::hasPercentageBlockSize(*blockContainer, blockContainer->writingMode()) && resolvesAgainstRoot(*blockContainer))
                return true;
        }
        descendant = descendant->nextInPreOrder(&root);
    }
    return false;
}

bool canUseForIntrinsicInlineSizes(const RenderBlockFlow& rootContainer)
{
    auto scope = NestedBlockLayoutCoverageScope { };
    return canUseForBlockLayout(rootContainer);
}

static ASCIILiteral description(BlockAvoidanceReason reason)
{
    switch (reason) {
    case BlockAvoidanceReason::BlockFormattingContextIntegrationDisabled:
        return "disabled"_s;
    case BlockAvoidanceReason::NotABlockFormattingContextRoot:
        return "not a root"_s;
    case BlockAvoidanceReason::RootIsMultiColumnFlow:
        return "root: multi-column flow"_s;
    case BlockAvoidanceReason::RootHasUnsupportedRenderer:
        return "root: other renderer"_s;
    case BlockAvoidanceReason::RootIsPaginated:
        return "root: paginated"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowBox:
        return "out-of-flow box in inline or leaf content"_s;
    case BlockAvoidanceReason::ContentHasOutOfFlowNonBlockContainer:
        return "out-of-flow table or other renderer"_s;
    case BlockAvoidanceReason::ContentHasUnsupportedRenderer:
        return "other renderer"_s;
    case BlockAvoidanceReason::ContentHasBlockInInline:
        return "block-in-inline"_s;
    case BlockAvoidanceReason::ContentHasUnsupportedInlineBox:
        return "unsupported inline box"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

BlockLayoutCoverage computeBlockLayoutCoverage(const RenderView& renderView)
{
    auto scope = NestedBlockLayoutCoverageScope { };
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
