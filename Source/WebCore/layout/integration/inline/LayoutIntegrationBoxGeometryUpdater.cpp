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

#include "FormattingConstraints.h"
#include "InlineWalker.h"
#include "LayoutBoxGeometry.h"
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

static LayoutUnit fixedValueOrZero(const Length& length)
{
    return LayoutUnit { length.isFixed() ? length.value() : 0.f };
}

BoxGeometryUpdater::BoxGeometryUpdater(BoxTree& boxTree, Layout::LayoutState& layoutState)
    : m_boxTree(boxTree)
    , m_layoutState(layoutState)
{
}

void BoxGeometryUpdater::updateListMarkerDimensions(const RenderListMarker& listMarker, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    updateLayoutBoxDimensions(listMarker, intrinsicWidthMode);
    if (intrinsicWidthMode)
        return;

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

enum class UseComputedValues : bool { No, Yes };
static inline Layout::BoxGeometry::HorizontalEdges horizontalLogicalMargin(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, bool isHorizontalWritingMode, UseComputedValues useComputedValues = UseComputedValues::No, bool retainMarginStart = true, bool retainMarginEnd = true)
{
    auto& style = renderer.style();
    auto marginLeft = useComputedValues == UseComputedValues::No ? renderer.marginLeft() : fixedValueOrZero(style.marginLeft());
    auto marginRight = useComputedValues == UseComputedValues::No ? renderer.marginRight() : fixedValueOrZero(style.marginRight());
    if (isHorizontalWritingMode) {
        if (isLeftToRightInlineDirection)
            return { retainMarginStart ? marginLeft : 0_lu, retainMarginEnd ? marginRight : 0_lu };
        return { retainMarginStart ? marginRight : 0_lu, retainMarginEnd ? marginLeft : 0_lu };
    }

    auto marginTop = useComputedValues == UseComputedValues::No ? renderer.marginTop() : fixedValueOrZero(style.marginTop());
    auto marginBottom = useComputedValues == UseComputedValues::No ? renderer.marginBottom() : fixedValueOrZero(style.marginBottom());
    if (isLeftToRightInlineDirection)
        return { retainMarginStart ? marginTop : 0_lu, retainMarginEnd ? marginBottom : 0_lu };
    return { retainMarginStart ? marginBottom : 0_lu, retainMarginEnd ? marginTop : 0_lu };
}

static inline Layout::BoxGeometry::VerticalEdges verticalLogicalMargin(const RenderBoxModelObject& renderer, BlockFlowDirection blockFlowDirection)
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
static inline Layout::BoxGeometry::Edges logicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection, UseComputedValues useComputedValues = UseComputedValues::No, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainBorderStart = true, bool retainBorderEnd = true)
{
    auto& style = renderer.style();
    auto borderLeft = useComputedValues == UseComputedValues::No ? renderer.borderLeft() : LayoutUnit(style.borderLeft().width());
    auto borderRight = useComputedValues == UseComputedValues::No ? renderer.borderRight() : LayoutUnit(style.borderRight().width());
    auto borderTop = useComputedValues == UseComputedValues::No ? renderer.borderTop() : LayoutUnit(style.borderTop().width());
    auto borderBottom = useComputedValues == UseComputedValues::No ? renderer.borderBottom() : LayoutUnit(style.borderBottom().width());

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

static inline Layout::BoxGeometry::Edges logicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection, UseComputedValues useComputedValues = UseComputedValues::No, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainPaddingStart = true, bool retainPaddingEnd = true)
{
    auto& style = renderer.style();
    auto paddingLeft = useComputedValues == UseComputedValues::No ? renderer.paddingLeft() : fixedValueOrZero(style.paddingLeft());
    auto paddingRight = useComputedValues == UseComputedValues::No ? renderer.paddingRight() : fixedValueOrZero(style.paddingRight());
    auto paddingTop = useComputedValues == UseComputedValues::No ? renderer.paddingTop() : fixedValueOrZero(style.paddingTop());
    auto paddingBottom = useComputedValues == UseComputedValues::No ? renderer.paddingBottom() : fixedValueOrZero(style.paddingBottom());

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

void BoxGeometryUpdater::updateLayoutBoxDimensions(const RenderBox& renderBox, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& layoutBox = boxTree().layoutBoxForRenderer(renderBox);
    auto isLeftToRightInlineDirection = renderBox.parent()->style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(renderBox.parent()->style().writingMode());
    auto isHorizontalWritingMode = blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop;

    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    auto inlineMargin = horizontalLogicalMargin(renderBox, isLeftToRightInlineDirection, isHorizontalWritingMode, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No);
    auto border = logicalBorder(renderBox, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No);
    auto padding = logicalPadding(renderBox, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No);
    auto scrollbarSize = scrollbarLogicalSize(renderBox);

    if (intrinsicWidthMode) {
        boxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
        boxGeometry.setContentBoxWidth(*intrinsicWidthMode == Layout::IntrinsicWidthMode::Minimum ? renderBox.minPreferredLogicalWidth() : renderBox.maxPreferredLogicalWidth());
        boxGeometry.setHorizontalMargin(inlineMargin);
        boxGeometry.setHorizontalBorder(border.horizontal);
        boxGeometry.setHorizontalPadding(padding.horizontal);
        return;
    }

    boxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
    boxGeometry.setVerticalSpaceForScrollbar(scrollbarSize.height());

    boxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(renderBox));
    boxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(renderBox));

    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderBox, blockFlowDirection));
    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);

    auto hasNonSyntheticBaseline = [&] {
        if (is<RenderListMarker>(renderBox))
            return !downcast<RenderListMarker>(renderBox).isImage();

        if ((is<RenderReplaced>(renderBox) && renderBox.style().display() == DisplayType::Inline)
            || is<RenderListBox>(renderBox)
            || is<RenderSlider>(renderBox)
            || is<RenderTextControlMultiLine>(renderBox)
            || is<RenderTable>(renderBox)
            || is<RenderGrid>(renderBox)
            || is<RenderFlexibleBox>(renderBox)
            || is<RenderDeprecatedFlexibleBox>(renderBox)
#if ENABLE(ATTACHMENT_ELEMENT)
            || is<RenderAttachment>(renderBox)
#endif
#if ENABLE(MATHML)
            || is<RenderMathMLBlock>(renderBox)
#endif
            || is<RenderButton>(renderBox)) {
            // These are special RenderBlock renderers that override the default baseline position behavior of the inline block box.
            return true;
        }
        if (!is<RenderBlockFlow>(renderBox))
            return false;
        auto& blockFlow = downcast<RenderBlockFlow>(renderBox);
        auto hasAppareance = blockFlow.style().hasEffectiveAppearance() && !blockFlow.theme().isControlContainer(blockFlow.style().effectiveAppearance());
        return hasAppareance || !blockFlow.childrenInline() || blockFlow.hasLines() || blockFlow.hasLineIfEmpty();
    }();
    if (hasNonSyntheticBaseline) {
        auto baseline = renderBox.baselinePosition(AlphabeticBaseline, false /* firstLine */, blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop ? HorizontalLine : VerticalLine, PositionOnContainingLine);
        layoutBox.setBaselineForIntegration(roundToInt(baseline));
    }

    if (auto* shapeOutsideInfo = renderBox.shapeOutsideInfo())
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

void BoxGeometryUpdater::updateInlineBoxDimensions(const RenderInline& renderInline, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& boxGeometry = layoutState().ensureGeometryForBox(boxTree().layoutBoxForRenderer(renderInline));

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();

    auto isLeftToRightInlineDirection = renderInline.style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(renderInline.style().writingMode());
    auto isHorizontalWritingMode = blockFlowDirection == BlockFlowDirection::TopToBottom || blockFlowDirection == BlockFlowDirection::BottomToTop;

    auto inlineMargin = horizontalLogicalMargin(renderInline, isLeftToRightInlineDirection, isHorizontalWritingMode, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto border = logicalBorder(renderInline, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No, IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto padding = logicalPadding(renderInline, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode ? UseComputedValues::Yes : UseComputedValues::No, IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);

    if (intrinsicWidthMode) {
        boxGeometry.setHorizontalMargin(inlineMargin);
        boxGeometry.setHorizontalBorder(border.horizontal);
        boxGeometry.setHorizontalPadding(padding.horizontal);
        return;
    }

    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderInline, blockFlowDirection));
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);
}

void BoxGeometryUpdater::setGeometriesForLayout()
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(boxTree().rootRenderer())); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        if (is<RenderReplaced>(renderer) || is<RenderTable>(renderer) || is<RenderListItem>(renderer) || is<RenderBlock>(renderer) || is<RenderFrameSet>(renderer)) {
            updateLayoutBoxDimensions(downcast<RenderBox>(renderer));
            continue;
        }
        if (is<RenderListMarker>(renderer)) {
            updateListMarkerDimensions(downcast<RenderListMarker>(renderer));
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
    }
}

void BoxGeometryUpdater::setGeometriesForIntrinsicWidth(Layout::IntrinsicWidthMode intrinsicWidthMode)
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(boxTree().rootRenderer())); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        if (is<RenderLineBreak>(renderer)) {
            updateLineBreakBoxDimensions(downcast<RenderLineBreak>(renderer));
            continue;
        }
        if (is<RenderInline>(renderer)) {
            updateInlineBoxDimensions(downcast<RenderInline>(renderer), intrinsicWidthMode);
            continue;
        }
        ASSERT(is<RenderText>(renderer));
    }
}

Layout::ConstraintsForInlineContent BoxGeometryUpdater::updateInlineContentConstraints()
{
    auto& rootRenderer = boxTree().rootRenderer();
    auto isLeftToRightInlineDirection = rootRenderer.style().isLeftToRightDirection();
    auto writingMode = rootRenderer.style().writingMode();
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    auto padding = logicalPadding(rootRenderer, isLeftToRightInlineDirection, blockFlowDirection, UseComputedValues::No, IsPartOfFormattingContext::No);
    auto border = logicalBorder(rootRenderer, isLeftToRightInlineDirection, blockFlowDirection, UseComputedValues::No, IsPartOfFormattingContext::No);
    auto scrollbarSize = scrollbarLogicalSize(rootRenderer);
    auto shouldPlaceVerticalScrollbarOnLeft = rootRenderer.shouldPlaceVerticalScrollbarOnLeft();

    auto contentBoxWidth = WebCore::isHorizontalWritingMode(writingMode) ? rootRenderer.contentWidth() : rootRenderer.contentHeight();
    auto contentBoxLeft = border.horizontal.start + padding.horizontal.start + (isLeftToRightInlineDirection && shouldPlaceVerticalScrollbarOnLeft ? scrollbarSize.width() : 0_lu);
    auto contentBoxTop = border.vertical.before + padding.vertical.before;

    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLeft, contentBoxWidth };
    auto visualLeft = !isLeftToRightInlineDirection || shouldPlaceVerticalScrollbarOnLeft ? border.horizontal.end + scrollbarSize.width() + padding.horizontal.end : contentBoxLeft;

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

