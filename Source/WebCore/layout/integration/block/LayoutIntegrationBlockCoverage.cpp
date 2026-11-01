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

static std::optional<BlockAvoidanceReason> inlineContentAvoidanceReason(const RenderElement& renderer)
{
    if (is<RenderLineBreak>(renderer))
        return { };
    if (is<RenderBox>(renderer))
        return BlockAvoidanceReason::ContentHasAtomicInlineLevelBox;
    CheckedRef style = renderer.style();
    if (renderer.type() != RenderObject::Type::Inline || style->display() != Style::DisplayType::InlineFlow || style->pseudoElementType() == PseudoElementType::FirstLetter)
        return BlockAvoidanceReason::ContentHasUnsupportedInlineBox;
    if (renderer.hasLayer())
        return BlockAvoidanceReason::ContentHasLayer;
    return { };
}

static std::optional<BlockAvoidanceReason> contentAvoidanceReason(const RenderElement& renderer)
{
    // A box a line-clamp container hid after its clamp point stays hidden until the render tree path lays it out again
    // (RenderBlockFlow::layoutBlockChild, markInlineContentDirtyForLayout).
    if (renderer.isForceHiddenByLineClamp())
        return BlockAvoidanceReason::ContentIsHiddenByLineClamp;
    if (renderer.isFloating())
        return BlockAvoidanceReason::ContentHasFloat;
    if (renderer.isOutOfFlowPositioned())
        return BlockAvoidanceReason::ContentHasOutOfFlowBox;
    if (renderer.isInline())
        return inlineContentAvoidanceReason(renderer);
    // Block boxes only: not list items, replaced elements, tables, flex or grid containers or form controls.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(renderer);
    if (!blockContainer || blockContainer->type() != RenderObject::Type::BlockFlow)
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    // Form controls and legends treat an auto width as fit-content (RenderBox::sizesLogicalWidthToFitContent).
    if (isAnyOf<HTMLButtonElement, HTMLInputElement, HTMLLegendElement, HTMLSelectElement, HTMLTextAreaElement>(blockContainer->element()))
        return BlockAvoidanceReason::ContentHasUnsupportedRenderer;
    if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(blockContainer->parent()); !parent || parent->childrenInline())
        return BlockAvoidanceReason::ContentHasBlockInInline;
    if (blockContainer->createsNewFormattingContext())
        return BlockAvoidanceReason::ContentHasFormattingContextRoot;
    if (blockContainer->hasLayer())
        return BlockAvoidanceReason::ContentHasLayer;

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
    if ((!isAuto(style->logicalWidth()) && !isSupportedSize(style->logicalWidth()))
        || (!isAuto(style->logicalMinWidth()) && !isSupportedSize(style->logicalMinWidth()))
        || (!style->logicalMaxWidth().isNone() && !isSupportedSize(style->logicalMaxWidth()))
        || (!isAuto(style->logicalHeight()) && !isSupportedBlockSize(style->logicalHeight(), true))
        || (!isAuto(style->logicalMinHeight()) && !isSupportedBlockSize(style->logicalMinHeight(), blockSizeIsAutomatic))
        || (!style->logicalMaxHeight().isNone() && !isSupportedBlockSize(style->logicalMaxHeight(), blockSizeIsAutomatic))
        || style->aspectRatio().hasRatio() || blockContainer->shouldApplySizeContainment())
        return BlockAvoidanceReason::ContentHasUnsupportedSize;
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

static std::optional<BlockAvoidanceReason> blockLayoutAvoidanceReason(const RenderBlockFlow& rootContainer)
{
    if (auto reason = rootAvoidanceReason(rootContainer))
        return reason;
    // Every descendant is a block box that takes part in the root's block formatting context, or inline content of one (any other box would be a leaf).
    for (CheckedRef descendant : descendantsOfType<RenderElement>(rootContainer)) {
        if (auto reason = contentAvoidanceReason(descendant))
            return reason;
    }
    return { };
}

bool canUseForBlockLayout(const RenderBlockFlow& rootContainer)
{
    return !blockLayoutAvoidanceReason(rootContainer);
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
