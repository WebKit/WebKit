/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationBoxGeometryUpdater.h"

#include "InlineWalker.h"
#include "RenderAttachment.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderButton.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderFrameSet.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMathMLBlock.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTextControlMultiLine.h"
#include "RenderTheme.h"

namespace WebCore {
namespace LayoutIntegration {

BoxGeometryUpdater::BoxGeometryUpdater(BoxTree& boxTree, Layout::LayoutState& layoutState)
    : m_boxTree(boxTree)
    , m_layoutState(layoutState)
{
}

void BoxGeometryUpdater::updateReplacedDimensions(const RenderBox& replaced)
{
    updateLayoutBoxDimensions(replaced);
}

void BoxGeometryUpdater::updateInlineBlockDimensions(const RenderBlock& inlineBlock)
{
    updateLayoutBoxDimensions(inlineBlock);
}

void BoxGeometryUpdater::updateInlineTableDimensions(const RenderTable& inlineTable)
{
    updateLayoutBoxDimensions(inlineTable);
}

void BoxGeometryUpdater::updateListItemDimensions(const RenderListItem& listItem)
{
    updateLayoutBoxDimensions(listItem);
}

void BoxGeometryUpdater::updateListMarkerDimensions(const RenderListMarker& listMarker)
{
    updateLayoutBoxDimensions(listMarker);

    auto& layoutBox = boxTree().layoutBoxForRenderer(listMarker);
    if (layoutBox.isListMarkerOutside()) {
        auto* ancestor = listMarker.containingBlock();
        auto offsetFromParentListItem = [&] {
            auto hasAccountedForBorderAndPadding = false;
            auto offset = LayoutUnit { };
            for (; ancestor; ancestor = ancestor->containingBlock()) {
                if (!hasAccountedForBorderAndPadding)
                    offset -= (ancestor->borderStart() + ancestor->paddingStart());
                if (is<RenderListItem>(*ancestor))
                    break;
                if (ancestor->isFlexItem()) {
                    offset -= ancestor->logicalLeft();
                    hasAccountedForBorderAndPadding = true;
                    continue;
                }
                hasAccountedForBorderAndPadding = false;
            }
            return offset;
        }();
        auto offsetFromAssociatedListItem = [&] {
            auto* associatedListItem = listMarker.listItem();
            if (ancestor == associatedListItem || !ancestor) {
                // FIXME: Handle column spanner case when ancestor is null_ptr here.
                return offsetFromParentListItem;
            }
            auto offset = offsetFromParentListItem;
            for (ancestor = ancestor->containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
                offset -= (ancestor->borderStart() + ancestor->paddingStart());
                if (ancestor == associatedListItem)
                    break;
            }
            return offset;
        }();
        if (offsetFromAssociatedListItem) {
            auto& listMarkerGeometry = layoutState().ensureGeometryForBox(layoutBox);
            // Make sure that the line content does not get pulled in to logical left direction due to
            // the large negative margin (i.e. this ensures that logical left of the list content stays at the line start)
            listMarkerGeometry.setHorizontalMargin({ listMarkerGeometry.marginStart() + offsetFromParentListItem, listMarkerGeometry.marginEnd() - offsetFromParentListItem });
            if (auto nestedOffset = offsetFromAssociatedListItem - offsetFromParentListItem)
                m_nestedListMarkerOffsets.set(&layoutBox, nestedOffset);
        }
    }
}

static inline LayoutUnit contentLogicalWidthForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentWidth() : renderer.contentHeight();
}

static inline LayoutUnit contentLogicalHeightForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentHeight() : renderer.contentWidth();
}

static inline Layout::BoxGeometry::HorizontalMargin horizontalLogicalMargin(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, bool isHorizontalWritingMode, bool retainMarginStart = true, bool retainMarginEnd = true)
{
    auto marginLeft = renderer.marginLeft();
    auto marginRight = renderer.marginRight();
    if (isHorizontalWritingMode) {
        if (isLeftToRightInlineDirection)
            return { retainMarginStart ? marginLeft : 0_lu, retainMarginEnd ? marginRight : 0_lu };
        return { retainMarginStart ? marginRight : 0_lu, retainMarginEnd ? marginLeft : 0_lu };
    }

    auto marginTop = renderer.marginTop();
    auto marginBottom = renderer.marginBottom();
    if (isLeftToRightInlineDirection)
        return { retainMarginStart ? marginTop : 0_lu, retainMarginEnd ? marginBottom : 0_lu };
    return { retainMarginStart ? marginBottom : 0_lu, retainMarginEnd ? marginTop : 0_lu };
}

static inline Layout::BoxGeometry::VerticalMargin verticalLogicalMargin(const RenderBoxModelObject& renderer, BlockFlowDirection blockFlowDirection)
{
    switch (blockFlowDirection) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        return { renderer.marginTop(), renderer.marginBottom() };
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft:
        return { renderer.marginRight(), renderer.marginLeft() };
    default:
        ASSERT_NOT_REACHED();
        return { renderer.marginTop(), renderer.marginBottom() };
    }
}

enum class IsPartOfFormattingContext : bool { No, Yes };
static inline Layout::Edges logicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainBorderStart = true, bool retainBorderEnd = true)
{
    auto borderLeft = renderer.borderLeft();
    auto borderRight = renderer.borderRight();
    auto borderTop = renderer.borderTop();
    auto borderBottom = renderer.borderBottom();

    if (blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop) {
        if (isLeftToRightInlineDirection)
            return { { retainBorderStart ? borderLeft : 0_lu, retainBorderEnd ? borderRight : 0_lu }, { borderTop, borderBottom } };
        return { { retainBorderStart ? borderRight : 0_lu, retainBorderEnd ? borderLeft : 0_lu }, { borderTop, borderBottom } };
    }

    auto borderLogicalLeft = retainBorderStart ? isLeftToRightInlineDirection ? borderTop : borderBottom : 0_lu;
    auto borderLogicalRight = retainBorderEnd ? isLeftToRightInlineDirection ? borderBottom : borderTop : 0_lu;
    // For boxes inside the formatting context, right border (padding) always points up, while when converting the formatting context root's border (padding) the directionality matters.
    auto borderLogicalTop = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? borderRight : blockFlowDirection == BlockFlowDirection::LeftToRight ? borderLeft : borderRight;
    auto borderLogicalBottom = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? borderLeft : blockFlowDirection == BlockFlowDirection::LeftToRight ? borderRight : borderLeft;
    return { { borderLogicalLeft, borderLogicalRight }, { borderLogicalTop, borderLogicalBottom } };
}

static inline Layout::Edges logicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainPaddingStart = true, bool retainPaddingEnd = true)
{
    auto paddingLeft = renderer.paddingLeft();
    auto paddingRight = renderer.paddingRight();
    auto paddingTop = renderer.paddingTop();
    auto paddingBottom = renderer.paddingBottom();

    if (blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop) {
        if (isLeftToRightInlineDirection)
            return { { retainPaddingStart ? paddingLeft : 0_lu, retainPaddingEnd ? paddingRight : 0_lu }, { paddingTop, paddingBottom } };
        return { { retainPaddingStart ? paddingRight : 0_lu, retainPaddingEnd ? paddingLeft : 0_lu }, { paddingTop, paddingBottom } };
    }

    auto paddingLogicalLeft = retainPaddingStart ? isLeftToRightInlineDirection ? paddingTop : paddingBottom : 0_lu;
    auto paddingLogicalRight = retainPaddingEnd ? isLeftToRightInlineDirection ? paddingBottom : paddingTop : 0_lu;
    // For boxes inside the formatting context, right padding (border) always points up, while when converting the formatting context root's padding (border) the directionality matters.
    auto paddingLogicalTop = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? paddingRight : blockFlowDirection == BlockFlowDirection::LeftToRight ? paddingLeft : paddingRight;
    auto paddingLogicalBottom = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? paddingLeft : blockFlowDirection == BlockFlowDirection::LeftToRight ? paddingRight : paddingLeft;
    return { { paddingLogicalLeft, paddingLogicalRight }, { paddingLogicalTop, paddingLogicalBottom } };
}

static inline LayoutSize scrollbarLogicalSize(const RenderBox& renderer)
{
    // Scrollbars eat into the padding box area. They never stretch the border box but they may shrink the padding box.
    // In legacy render tree, RenderBox::contentWidth/contentHeight values are adjusted to accommodate the scrollbar width/height.
    // e.g. <div style="width: 10px; overflow: scroll;">content</div>, RenderBox::contentWidth() won't be returning the value of 10px but instead 0px (10px - 15px).
    auto horizontalSpaceReservedForScrollbar = std::max(0_lu, renderer.paddingBoxRectIncludingScrollbar().width() - renderer.paddingBoxWidth());
    auto verticalSpaceReservedForScrollbar = std::max(0_lu, renderer.paddingBoxRectIncludingScrollbar().height() - renderer.paddingBoxHeight());
    return { horizontalSpaceReservedForScrollbar, verticalSpaceReservedForScrollbar };
}

void BoxGeometryUpdater::updateLayoutBoxDimensions(const RenderBox& replacedOrInlineBlock)
{
    auto& layoutBox = boxTree().layoutBoxForRenderer(replacedOrInlineBlock);

    auto& replacedBoxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    auto scrollbarSize = scrollbarLogicalSize(replacedOrInlineBlock);
    replacedBoxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
    replacedBoxGeometry.setVerticalSpaceForScrollbar(scrollbarSize.height());

    replacedBoxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(replacedOrInlineBlock));
    replacedBoxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(replacedOrInlineBlock));

    auto isLeftToRightInlineDirection = replacedOrInlineBlock.parent()->style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(replacedOrInlineBlock.parent()->style().writingMode());

    replacedBoxGeometry.setVerticalMargin(verticalLogicalMargin(replacedOrInlineBlock, blockFlowDirection));
    replacedBoxGeometry.setHorizontalMargin(horizontalLogicalMargin(replacedOrInlineBlock, isLeftToRightInlineDirection, blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop));
    replacedBoxGeometry.setBorder(logicalBorder(replacedOrInlineBlock, isLeftToRightInlineDirection, blockFlowDirection));
    replacedBoxGeometry.setPadding(logicalPadding(replacedOrInlineBlock, isLeftToRightInlineDirection, blockFlowDirection));

    auto hasNonSyntheticBaseline = [&] {
        if (is<RenderListMarker>(replacedOrInlineBlock))
            return !downcast<RenderListMarker>(replacedOrInlineBlock).isImage();

        if ((is<RenderReplaced>(replacedOrInlineBlock) && replacedOrInlineBlock.style().display() == DisplayType::Inline)
            || is<RenderListBox>(replacedOrInlineBlock)
            || is<RenderSlider>(replacedOrInlineBlock)
            || is<RenderTextControlMultiLine>(replacedOrInlineBlock)
            || is<RenderTable>(replacedOrInlineBlock)
            || is<RenderGrid>(replacedOrInlineBlock)
            || is<RenderFlexibleBox>(replacedOrInlineBlock)
            || is<RenderDeprecatedFlexibleBox>(replacedOrInlineBlock)
#if ENABLE(ATTACHMENT_ELEMENT)
            || is<RenderAttachment>(replacedOrInlineBlock)
#endif
#if ENABLE(MATHML)
            || is<RenderMathMLBlock>(replacedOrInlineBlock)
#endif
            || is<RenderButton>(replacedOrInlineBlock)) {
            // These are special RenderBlock renderers that override the default baseline position behavior of the inline block box.
            return true;
        }
        if (!is<RenderBlockFlow>(replacedOrInlineBlock))
            return false;
        auto& blockFlow = downcast<RenderBlockFlow>(replacedOrInlineBlock);
        auto hasAppareance = blockFlow.style().hasEffectiveAppearance() && !blockFlow.theme().isControlContainer(blockFlow.style().effectiveAppearance());
        return hasAppareance || !blockFlow.childrenInline() || blockFlow.hasLines() || blockFlow.hasLineIfEmpty();
    }();
    if (hasNonSyntheticBaseline) {
        auto baseline = replacedOrInlineBlock.baselinePosition(AlphabeticBaseline, false /* firstLine */, blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop ? HorizontalLine : VerticalLine, PositionOnContainingLine);
        layoutBox.setBaselineForIntegration(roundToInt(baseline));
    }

    if (auto* shapeOutsideInfo = replacedOrInlineBlock.shapeOutsideInfo())
        layoutBox.setShape(&shapeOutsideInfo->computedShape());
}

void BoxGeometryUpdater::updateLineBreakBoxDimensions(const RenderLineBreak& lineBreakBox)
{
    // This is just a box geometry reset (see InlineFormattingContext::layoutInFlowContent).
    auto& boxGeometry = layoutState().ensureGeometryForBox(boxTree().layoutBoxForRenderer(lineBreakBox));

    boxGeometry.setHorizontalMargin({ });
    boxGeometry.setBorder({ });
    boxGeometry.setPadding({ });
    boxGeometry.setContentBoxWidth({ });
    boxGeometry.setVerticalMargin({ });
    if (lineBreakBox.style().hasOutOfFlowPosition())
        boxGeometry.setContentBoxHeight({ });
}

void BoxGeometryUpdater::updateInlineBoxDimensions(const RenderInline& renderInline)
{
    auto& boxGeometry = layoutState().ensureGeometryForBox(boxTree().layoutBoxForRenderer(renderInline));

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();

    boxGeometry.setVerticalMargin({ });
    auto isLeftToRightInlineDirection = renderInline.style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(renderInline.style().writingMode());

    boxGeometry.setHorizontalMargin(horizontalLogicalMargin(renderInline, isLeftToRightInlineDirection, blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderInline, blockFlowDirection));
    boxGeometry.setBorder(logicalBorder(renderInline, isLeftToRightInlineDirection, blockFlowDirection, IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setPadding(logicalPadding(renderInline, isLeftToRightInlineDirection, blockFlowDirection, IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
}

void BoxGeometryUpdater::setGeometriesForLayout()
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(boxTree().rootRenderer())); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        if (is<RenderReplaced>(renderer)) {
            updateReplacedDimensions(downcast<RenderReplaced>(renderer));
            continue;
        }
        if (is<RenderTable>(renderer)) {
            updateInlineTableDimensions(downcast<RenderTable>(renderer));
            continue;
        }
        if (is<RenderListMarker>(renderer)) {
            updateListMarkerDimensions(downcast<RenderListMarker>(renderer));
            continue;
        }
        if (is<RenderListItem>(renderer)) {
            updateListItemDimensions(downcast<RenderListItem>(renderer));
            continue;
        }
        if (is<RenderBlock>(renderer)) {
            updateInlineBlockDimensions(downcast<RenderBlock>(renderer));
            continue;
        }
        if (is<RenderLineBreak>(renderer)) {
            updateLineBreakBoxDimensions(downcast<RenderLineBreak>(renderer));
            continue;
        }
        if (is<RenderInline>(renderer)) {
            updateInlineBoxDimensions(downcast<RenderInline>(renderer));
            continue;
        }
        if (is<RenderFrameSet>(renderer)) {
            updateLayoutBoxDimensions(downcast<RenderBox>(renderer));
            continue;
        }
    }
}

Layout::ConstraintsForInlineContent BoxGeometryUpdater::updateInlineContentConstraints()
{
    auto& rootRenderer = boxTree().rootRenderer();
    auto isLeftToRightInlineDirection = rootRenderer.style().isLeftToRightDirection();
    auto writingMode = rootRenderer.style().writingMode();
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    auto padding = logicalPadding(rootRenderer, isLeftToRightInlineDirection, blockFlowDirection, IsPartOfFormattingContext::No);
    auto border = logicalBorder(rootRenderer, isLeftToRightInlineDirection, blockFlowDirection, IsPartOfFormattingContext::No);
    auto scrollbarSize = scrollbarLogicalSize(rootRenderer);
    auto shouldPlaceVerticalScrollbarOnLeft = rootRenderer.shouldPlaceVerticalScrollbarOnLeft();

    auto contentBoxWidth = WebCore::isHorizontalWritingMode(writingMode) ? rootRenderer.contentWidth() : rootRenderer.contentHeight();
    auto contentBoxLeft = border.horizontal.left + padding.horizontal.left + (isLeftToRightInlineDirection && shouldPlaceVerticalScrollbarOnLeft ? scrollbarSize.width() : 0_lu);
    auto contentBoxTop = border.vertical.top + padding.vertical.top;

    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLeft, contentBoxWidth };
    auto visualLeft = !isLeftToRightInlineDirection || shouldPlaceVerticalScrollbarOnLeft ? border.horizontal.right + scrollbarSize.width() + padding.horizontal.right : contentBoxLeft;

    auto createRootGeometryIfNeeded = [&] {
        // FIXME: BFC should be responsible for creating the box geometry for this block box (IFC root) as part of the block layout.
        auto& rootGeometry = layoutState().ensureGeometryForBox(rootLayoutBox());
        rootGeometry.setContentBoxWidth(contentBoxWidth);
        rootGeometry.setPadding(padding);
        rootGeometry.setBorder(border);
        rootGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
        rootGeometry.setVerticalSpaceForScrollbar(scrollbarSize.height());
        rootGeometry.setHorizontalMargin({ });
        rootGeometry.setVerticalMargin({ });
    };
    createRootGeometryIfNeeded();

    return { { horizontalConstraints, contentBoxTop }, visualLeft };
}

const Layout::ElementBox& BoxGeometryUpdater::rootLayoutBox() const
{
    return boxTree().rootLayoutBox();
}

Layout::ElementBox& BoxGeometryUpdater::rootLayoutBox()
{
    return boxTree().rootLayoutBox();
}

}
}

