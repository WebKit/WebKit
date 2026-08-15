/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutIntegrationFormattingContextLayout.h"

#include "BlockLayoutState.h"
#include "FloatingObjects.h"
#include "InlineLayoutState.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationUtils.h"
#include "RenderBlock.h"
#include "RenderBlockFlowInlines.h"
#include "RenderStyleConstants.h"
#include "RenderBoxInlines.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "TextBoxTrimmer.h"

namespace WebCore {
namespace LayoutIntegration {

static inline const Layout::ElementBox& rootLayoutBox(const Layout::ElementBox& child)
{
    SUPPRESS_UNCHECKED_LOCAL auto* ancestor = &child.parent();
    while (!ancestor->isInitialContainingBlock()) {
        if (ancestor->establishesFormattingContext())
            break;
        ancestor = &ancestor->parent();
    }
    return *ancestor;
}

static void layoutRendererWithOverridingBorderBoxSize(RenderBox& renderer, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight)
{
    if (overridingBorderBoxLogicalWidth) {
        renderer.setOverridingBorderBoxLogicalWidth(*overridingBorderBoxLogicalWidth);
        renderer.setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    if (overridingBorderBoxLogicalHeight) {
        renderer.setOverridingBorderBoxLogicalHeight(*overridingBorderBoxLogicalHeight);
        renderer.setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    renderer.layoutIfNeeded();

    if (overridingBorderBoxLogicalWidth)
        renderer.clearOverridingBorderBoxLogicalWidth();

    if (overridingBorderBoxLogicalHeight)
        renderer.clearOverridingBorderBoxLogicalHeight();
}

// Lay the renderer out at the given overriding border-box size, then feed the result back into modern
// layout's BoxGeometry. containingBlockInlineSize is the inline size positions resolve against.
static void layoutRendererAndUpdateBoxGeometry(const Layout::ElementBox& box, RenderBox& renderer, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight, LayoutUnit containingBlockInlineSize, Layout::LayoutState& layoutState)
{
    layoutRendererWithOverridingBorderBoxSize(renderer, overridingBorderBoxLogicalWidth, overridingBorderBoxLogicalHeight);

    auto updater = BoxGeometryUpdater { layoutState, rootLayoutBox(box) };
    updater.updateBoxGeometryAfterIntegrationLayout(box, containingBlockInlineSize);
}

void layoutWithFormattingContextForBox(const Layout::ElementBox& box, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight, Layout::LayoutState& layoutState)
{
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());
    auto containingBlockInlineSize = overridingBorderBoxLogicalWidth.value_or(renderer->containingBlock()->contentBoxLogicalWidth());
    layoutRendererAndUpdateBoxGeometry(box, renderer.get(), overridingBorderBoxLogicalWidth, overridingBorderBoxLogicalHeight, containingBlockInlineSize, layoutState);
}

void layoutGridItemWithFormattingContext(const Layout::ElementBox& box, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight, LayoutUnit gridAreaInlineSize, Layout::LayoutState& layoutState)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    // A grid item's containing block is its grid area. Keep the grid area's inline size set on the renderer so
    // descendants resolve percentage/calc sizes against it during layout, and resolve the item's own geometry
    // against it below rather than against the grid container's content box.
    renderer->setGridAreaContentLogicalWidth(gridAreaInlineSize);
    layoutRendererAndUpdateBoxGeometry(box, renderer.get(), overridingBorderBoxLogicalWidth, overridingBorderBoxLogicalHeight, gridAreaInlineSize, layoutState);
    renderer->clearGridAreaContentSize();
}

static inline void populateRootRendererWithFloatsFromIFC(auto& rootBlockContainer, auto& placedFloats)
{
    auto blockFormattingContextRootWritingMode = placedFloats.blockFormattingContextRoot().style().writingMode();
    for (auto& floatItem : placedFloats.list()) {
        CheckedPtr layoutBox = floatItem.layoutBox();
        if (!layoutBox) {
            // Floats inherited by IFC do not have associated layout boxes.
            continue;
        }
        auto& floatingObject = rootBlockContainer.insertFloatingBox(downcast<RenderBox>(*layoutBox->rendererForIntegration()));
        if (floatingObject.isPlaced()) {
            // We have already inserted this float when laying out a previous middle-block.
            continue;
        }

        auto [marginBoxVisualRect, borderBoxVisualRect] = Layout::IntegrationUtils::toMarginAndBorderBoxVisualRect(floatItem.boxGeometry(), rootBlockContainer.borderBoxSize(), blockFormattingContextRootWritingMode);
        floatingObject.setFrameRect(marginBoxVisualRect);
        floatingObject.setMarginOffset({ borderBoxVisualRect.x() - marginBoxVisualRect.x(), borderBoxVisualRect.y() - marginBoxVisualRect.y() });
        floatingObject.setIsPlaced(true);
    }
}

static inline void populateIFCWithNewlyPlacedFloats(auto& blockRenderer, auto& placedFloats, auto blockLogicalTopLeft)
{
    auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(blockRenderer);
    if (!renderBlockFlow)
        return;

    if (!renderBlockFlow->containsFloats() || renderBlockFlow->createsNewFormattingContext())
        return;

    for (auto& floatingObject : *renderBlockFlow->floatingObjectSet()) {
        if (!floatingObject->renderer())
            continue;
        if (!floatingObject->isDescendant())
            continue;

        auto floatRect = floatingObject->frameRect();

        auto boxGeometry = Layout::BoxGeometry { };
        boxGeometry.setTopLeft(blockLogicalTopLeft + floatRect.location());
        boxGeometry.setContentBoxWidth(floatRect.width());
        boxGeometry.setContentBoxHeight(floatRect.height());
        boxGeometry.setBorder({ });
        boxGeometry.setPadding({ });
        boxGeometry.setHorizontalMargin({ });
        boxGeometry.setVerticalMargin({ });

        auto shapeOutsideInfo = floatingObject->renderer()->shapeOutsideInfo();
        RefPtr shape = shapeOutsideInfo ? &shapeOutsideInfo->computedShape() : nullptr;

        auto usedPosition = Style::ComputedStyle::usedFloat(*floatingObject->renderer()) == UsedFloat::Left ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;
        placedFloats.add({ usedPosition, boxGeometry, floatRect.location(), WTF::move(shape) });
    }
}

static inline void NODELETE updateRenderTreeLineClampBeforeLayout(auto& inlineLayoutState, auto& renderTreeLayoutState)
{
    auto& parentBlockLayoutState = inlineLayoutState.parentBlockLayoutState();

    auto lineClamp = parentBlockLayoutState.lineClamp();
    if (!lineClamp)
        return;

    auto currentLineCount = inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks();

    if (auto legacyLineClamp = renderTreeLayoutState.legacyLineClamp()) {
        legacyLineClamp->currentLineCount += currentLineCount;
        renderTreeLayoutState.setLegacyLineClamp(legacyLineClamp);
        return;
    }

    // The lines we have already put on the parent's own lines are part of the clamp's budget, and the nested block
    // has to lay out within what is left of it. A block level sibling gets this from LineClampUpdater, which drops
    // each preceding sibling's line count from the budget as it goes.
    if (auto renderTreeLineClamp = renderTreeLayoutState.lineClamp()) {
        auto maximumLines = renderTreeLineClamp->maximumLines;
        renderTreeLayoutState.setLineClamp(RenderLayoutState::LineClamp { maximumLines - std::min(maximumLines, currentLineCount), renderTreeLineClamp->shouldDiscardOverflow });
    }
}

static inline void NODELETE updateIFCLineClampAfterLayout(auto& inlineLayoutState, auto& renderTreeLayoutState, const RenderBox& blockRenderer)
{
    auto& parentBlockLayoutState = inlineLayoutState.parentBlockLayoutState();

    if (!parentBlockLayoutState.lineClamp())
        return;

    auto currentLineCount = inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks();

    if (auto legacyLineClamp = renderTreeLayoutState.legacyLineClamp()) {
        auto newlyConstructedLineCount = legacyLineClamp->currentLineCount - currentLineCount;
        inlineLayoutState.setLineCountWithInlineContentIncludingNestedBlocks(currentLineCount + newlyConstructedLineCount);
        return;
    }

    // The lines the nested block just produced count towards the clamp for the lines that follow it, the way a block
    // level sibling's do.
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(blockRenderer);
    if (blockFlow && blockFlow->childrenInline())
        inlineLayoutState.setLineCountWithInlineContentIncludingNestedBlocks(currentLineCount + blockFlow->lineCount());
}

void layoutWithFormattingContextForBlockInInline(const Layout::ElementBox& block, LayoutPoint blockLineLogicalTopLeft, Layout::InlineLayoutState& inlineLayoutState, Layout::LayoutState& layoutState)
{
    auto& parentBlockLayoutState = inlineLayoutState.parentBlockLayoutState();
    auto& placedFloats = parentBlockLayoutState.placedFloats();
    CheckedRef blockRenderer = downcast<RenderBox>(*block.rendererForIntegration());
    CheckedRef rootBlockContainer = downcast<RenderBlockFlow>(*rootLayoutBox(block).rendererForIntegration());
    auto& renderTreeLayoutState = *rootBlockContainer->view().frameView().layoutContext().layoutState();

    auto updateRenderTreeBeforeLayout = [&] {
        populateRootRendererWithFloatsFromIFC(rootBlockContainer.get(), placedFloats);
        updateRenderTreeLineClampBeforeLayout(inlineLayoutState, renderTreeLayoutState);
    };
    updateRenderTreeBeforeLayout();

    auto positionAndMargin = RenderBlockFlow::BlockPositionAndMargin { };
    auto layoutBlockRenderer = [&] {
        if (inlineLayoutState.lineCount()) {
            auto textBoxTrimStartDisabler = TextBoxTrimStartDisabler { blockRenderer.get() };
            positionAndMargin = rootBlockContainer->layoutBlockChildFromInlineLayout(blockRenderer.get(), blockLineLogicalTopLeft.y(), Layout::IntegrationUtils::toMarginInfo(parentBlockLayoutState.marginState()));
            return;
        }
        positionAndMargin = rootBlockContainer->layoutBlockChildFromInlineLayout(blockRenderer.get(), blockLineLogicalTopLeft.y(), Layout::IntegrationUtils::toMarginInfo(parentBlockLayoutState.marginState()));
    };
    layoutBlockRenderer();
    ASSERT(!blockRenderer->needsLayout());

    auto updateIFCAfterLayout = [&] {
        auto updater = BoxGeometryUpdater { layoutState, rootLayoutBox(block) };
        updater.updateBoxGeometryAfterIntegrationLayout(block, rootBlockContainer->contentBoxLogicalWidth());

        auto& blockGeometry = layoutState.ensureGeometryForBox(block);
        auto borderBoxTop = LayoutUnit { };

        auto contentOffsetAfterSelfCollapsingBlock = blockRenderer->isSelfCollapsingBlock() ? std::max(0_lu, positionAndMargin.childLogicalTop - positionAndMargin.containerLogicalBottom) : 0_lu;
        if (contentOffsetAfterSelfCollapsingBlock) {
            // This is where "next line top position" diverges from "current line's bottom".
            // See the last paragraph at https://www.w3.org/TR/CSS22/box.html#collapsing-margins
            // Instead of stretching the line box (by setting margin on the box) let's simply offset the box.
            // In practical terms, this means the starting position of the next line may not align exactly with where the bottom of the block ends.
            borderBoxTop = contentOffsetAfterSelfCollapsingBlock;
            blockGeometry.setVerticalMargin({ { }, { } });
        } else {
            borderBoxTop = positionAndMargin.childLogicalTop - blockLineLogicalTopLeft.y();
            auto advanceAfter = LayoutUnit { };
            if (alwaysPageBreak(blockRenderer->style().breakAfter()) || blockRenderer->isSelfCollapsingBlock())
                advanceAfter = std::max(0_lu, positionAndMargin.containerLogicalBottom - positionAndMargin.childLogicalTop - blockRenderer->logicalHeight());
            blockGeometry.setVerticalMargin({ borderBoxTop, advanceAfter });
        }
        blockGeometry.setTopLeft(LayoutPoint { blockGeometry.marginStart(), borderBoxTop });

        updateIFCLineClampAfterLayout(inlineLayoutState, renderTreeLayoutState, blockRenderer.get());
        // Floats are positioned relative to their containing block's border box, which sits at borderBoxTop within the line (see setTopLeft above) and not at the line's top left.
        populateIFCWithNewlyPlacedFloats(blockRenderer.get(), placedFloats, blockLineLogicalTopLeft + LayoutSize { blockGeometry.marginStart(), borderBoxTop });
        auto marginState = Layout::IntegrationUtils::toMarginState(positionAndMargin.marginInfo);
        // This box's clearance sits above its margin before, so that margin is behind the position the content after
        // the box starts at, even though the box keeps it for that content (CSS 2.2 8.3.1, 9.5.2). A negative margin
        // before is not above the border box at all, which is why this is the positive part only.
        if (auto marginBeforeWithClearance = rootBlockContainer->selfCollapsingMarginBeforeWithClear(blockRenderer.ptr()))
            marginState.marginBeforeWithClearance = *marginBeforeWithClearance;
        parentBlockLayoutState.marginState() = marginState;
    };
    updateIFCAfterLayout();
}

LayoutUnit formattingContextRootLogicalWidthForType(const Layout::ElementBox& box, LogicalWidthType logicalWidthType)
{
    // Either a box inline layout treats as atomic, or a block level box on a line: the render tree lays both of them
    // out, so it is the render tree that knows what they cost (see LineBuilder::handleBlockContent).
    ASSERT(box.establishesFormattingContext() || box.isBlockLevelBox());

    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalWidthType) {
    case LogicalWidthType::MaxContentContribution:
        return renderer->maxContentLogicalWidthContribution();
    case LogicalWidthType::MinContentContribution:
        return renderer->minContentLogicalWidthContribution();
    case LogicalWidthType::MaxContent:
    case LogicalWidthType::MinContent: {
        auto [minimunLogicalWidth, maximumLogicalWidth] = renderer->computeIntrinsicLogicalWidths();
        return logicalWidthType == LogicalWidthType::MaxContent ? maximumLogicalWidth : minimunLogicalWidth;
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit formattingContextRootLogicalHeightForType(const Layout::ElementBox& box, LogicalHeightType logicalHeightType)
{
    UNUSED_PARAM(box);
    ASSERT(box.establishesFormattingContext());

    switch (logicalHeightType) {
    case LogicalHeightType::MinContent: {
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

}
}
