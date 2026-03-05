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
#include "InlineLayoutState.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationUtils.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
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

void layoutWithFormattingContextForBox(const Layout::ElementBox& box, std::optional<LayoutUnit> widthConstraint, std::optional<LayoutUnit> heightConstraint, Layout::LayoutState& layoutState)
{
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    if (widthConstraint) {
        renderer->setOverridingBorderBoxLogicalWidth(*widthConstraint);
        renderer->setNeedsLayout(MarkOnlyThis);
    }

    if (heightConstraint) {
        renderer->setOverridingBorderBoxLogicalHeight(*heightConstraint);
        renderer->setNeedsLayout(MarkOnlyThis);
    }

    renderer->layoutIfNeeded();

    if (widthConstraint)
        renderer->clearOverridingBorderBoxLogicalWidth();

    auto updater = BoxGeometryUpdater { layoutState, rootLayoutBox(box) };
    updater.updateBoxGeometryAfterIntegrationLayout(box, widthConstraint.value_or(renderer->containingBlock()->contentBoxLogicalWidth()));
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

        auto [marginBoxVisualRect, borderBoxVisualRect] = Layout::IntegrationUtils::toMarginAndBorderBoxVisualRect(floatItem.boxGeometry(), rootBlockContainer.size(), blockFormattingContextRootWritingMode);
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

        auto usedPosition = RenderStyle::usedFloat(*floatingObject->renderer()) == UsedFloat::Left ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;
        placedFloats.add({ usedPosition, boxGeometry, floatRect.location(), WTF::move(shape) });
    }
}

static inline void NODELETE updateRenderTreeLegacyLineClamp(auto& inlineLayoutState, auto& renderTreeLayoutState)
{
    auto& parentBlockLayoutState = inlineLayoutState.parentBlockLayoutState();

    if (!parentBlockLayoutState.lineClamp())
        return;
    auto legacyLineClamp = renderTreeLayoutState.legacyLineClamp();
    if (!legacyLineClamp)
        return;
    legacyLineClamp->currentLineCount += inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks();
    renderTreeLayoutState.setLegacyLineClamp(legacyLineClamp);
}

static inline void NODELETE udpdateIFCLineClamp(auto& inlineLayoutState, auto& renderTreeLayoutState)
{
    auto& parentBlockLayoutState = inlineLayoutState.parentBlockLayoutState();

    if (!parentBlockLayoutState.lineClamp())
        return;
    auto legacyLineClamp = renderTreeLayoutState.legacyLineClamp();
    if (!legacyLineClamp)
        return;
    auto newlyConstructedLineCount = legacyLineClamp->currentLineCount - inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks();
    inlineLayoutState.setLineCountWithInlineContentIncludingNestedBlocks(inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks() + newlyConstructedLineCount);
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
        updateRenderTreeLegacyLineClamp(inlineLayoutState, renderTreeLayoutState);
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

        auto contentOffsetAfterSelfCollapsingBlock = blockRenderer->isSelfCollapsingBlock() ? positionAndMargin.childLogicalTop - positionAndMargin.containerLogicalBottom : 0_lu;
        if (contentOffsetAfterSelfCollapsingBlock) {
            // This is where "next line top position" diverges from "current line's bottom".
            // See the last paragraph at https://www.w3.org/TR/CSS22/box.html#collapsing-margins
            // Instead of stretching the line box (by setting margin on the box) let's simply offset the box.
            // In practical terms, this means the starting position of the next line may not align exactly with where the bottom of the block ends.
            borderBoxTop = contentOffsetAfterSelfCollapsingBlock;
            blockGeometry.setVerticalMargin({ { }, { } });
        } else {
            borderBoxTop = positionAndMargin.childLogicalTop - blockLineLogicalTopLeft.y();
            blockGeometry.setVerticalMargin({ borderBoxTop, { } });
        }
        blockGeometry.setTopLeft(LayoutPoint { blockGeometry.marginStart(), borderBoxTop });

        udpdateIFCLineClamp(inlineLayoutState, renderTreeLayoutState);
        populateIFCWithNewlyPlacedFloats(blockRenderer.get(), placedFloats, blockLineLogicalTopLeft);
        parentBlockLayoutState.marginState() = Layout::IntegrationUtils::toMarginState(positionAndMargin.marginInfo);
    };
    updateIFCAfterLayout();
}

LayoutUnit formattingContextRootLogicalWidthForType(const Layout::ElementBox& box, LogicalWidthType logicalWidthType)
{
    ASSERT(box.establishesFormattingContext());

    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalWidthType) {
    case LogicalWidthType::PreferredMaximum:
        return renderer->maxPreferredLogicalWidth();
    case LogicalWidthType::PreferredMinimum:
        return renderer->minPreferredLogicalWidth();
    case LogicalWidthType::MaxContent:
    case LogicalWidthType::MinContent: {
        auto minimunLogicalWidth = LayoutUnit { };
        auto maximumLogicalWidth = LayoutUnit { };
        renderer->computeIntrinsicLogicalWidths(minimunLogicalWidth, maximumLogicalWidth);
        return logicalWidthType == LogicalWidthType::MaxContent ? maximumLogicalWidth : minimunLogicalWidth;
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit formattingContextRootLogicalHeightForType(const Layout::ElementBox& box, LogicalHeightType logicalHeightType)
{
    ASSERT(box.establishesFormattingContext());

    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalHeightType) {
    case LogicalHeightType::MinContent: {
        // Since currently we can't ask RenderBox for content height, this is limited to flex items
        // where the legacy flex layout "fixed" this by caching the content height in RenderBox::updateLogicalHeight
        // before additional height constraints applied.
        if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(renderer->parent()))
            return flexContainer->cachedFlexItemIntrinsicContentLogicalHeight(renderer.get());
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
