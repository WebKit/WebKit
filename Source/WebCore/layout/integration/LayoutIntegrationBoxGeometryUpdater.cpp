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
#include "RenderTableCell.h"
#include "RenderTableInlines.h"
#include "RenderTextControlMultiLine.h"
#include "RenderTheme.h"

namespace WebCore {
namespace LayoutIntegration {

static LayoutUnit usedValueOrZero(const Length& length, std::optional<LayoutUnit> availableWidth)
{
    if (length.isFixed())
        return LayoutUnit { length.value() };

    if (length.isAuto() || !availableWidth)
        return { };

    return minimumValueForLength(length, *availableWidth);
}

static inline void adjustBorderForTableAndFieldset(const RenderBoxModelObject& renderer, LayoutUnit& borderLeft, LayoutUnit& borderRight, LayoutUnit& borderTop, LayoutUnit& borderBottom)
{
    if (auto* table = dynamicDowncast<RenderTable>(renderer); table && table->collapseBorders()) {
        borderLeft = table->borderLeft();
        borderRight = table->borderRight();
        borderTop = table->borderTop();
        borderBottom = table->borderBottom();
        return;
    }

    if (auto* tableCell = dynamicDowncast<RenderTableCell>(renderer); tableCell && tableCell->table()->collapseBorders()) {
        borderLeft = tableCell->borderLeft();
        borderRight = tableCell->borderRight();
        borderTop = tableCell->borderTop();
        borderBottom = tableCell->borderBottom();
        return;
    }

    if (renderer.isFieldset()) {
        auto adjustment = downcast<RenderBlock>(renderer).intrinsicBorderForFieldset();
        // Note that this adjustment is coming from _inside_ the fieldset so its own flow direction is what is relevant here.
        auto& style = renderer.style();
        switch (style.writingMode().blockDirection()) {
        case FlowDirection::TopToBottom:
            borderTop += adjustment;
            break;
        case FlowDirection::BottomToTop:
            borderBottom += adjustment;
            break;
        case FlowDirection::LeftToRight:
            borderLeft += adjustment;
            break;
        case FlowDirection::RightToLeft:
            borderRight += adjustment;
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
        return;
    }
}

static inline Layout::BoxGeometry::VerticalEdges intrinsicPaddingForTableCell(const RenderBox& renderer)
{
    if (auto* tableCell = dynamicDowncast<RenderTableCell>(renderer))
        return { tableCell->intrinsicPaddingBefore(), tableCell->intrinsicPaddingAfter() };
    return { };
}

BoxGeometryUpdater::BoxGeometryUpdater(Layout::LayoutState& layoutState, const Layout::ElementBox& rootLayoutBox)
    : m_layoutState(layoutState)
    , m_rootLayoutBox(rootLayoutBox)
{
}

void BoxGeometryUpdater::clear()
{
    m_rootLayoutBox = nullptr;
    m_nestedListMarkerOffsets.clear();
}

void BoxGeometryUpdater::setListMarkerOffsetForMarkerOutside(const RenderListMarker& listMarker)
{
    auto& layoutBox = *listMarker.layoutBox();
    ASSERT(layoutBox.isListMarkerOutside());
    auto* ancestor = listMarker.containingBlock();

    auto offsetFromParentListItem = [&] {
        auto hasAccountedForBorderAndPadding = false;
        auto offset = LayoutUnit { };
        for (; ancestor; ancestor = ancestor->containingBlock()) {
            if (!hasAccountedForBorderAndPadding)
                offset -= (ancestor->borderStart() + ancestor->paddingStart());
            if (is<RenderListItem>(*ancestor))
                break;
            offset -= (ancestor->marginStart());
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

static inline LayoutUnit contentLogicalWidthForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->writingMode().isHorizontal() ? renderer.contentWidth() : renderer.contentHeight();
}

static inline LayoutUnit contentLogicalHeightForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->writingMode().isHorizontal() ? renderer.contentHeight() : renderer.contentWidth();
}

Layout::BoxGeometry::HorizontalEdges BoxGeometryUpdater::horizontalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode, bool retainMarginStart, bool retainMarginEnd)
{
    auto& style = renderer.style();

    if (writingMode.isHorizontal()) {
        auto logicalLeftValue = retainMarginStart ? usedValueOrZero(writingMode.isInlineLeftToRight() ? style.marginLeft() : style.marginRight(), availableWidth) : 0_lu;
        auto logicalRightValue = retainMarginEnd ? usedValueOrZero(writingMode.isInlineLeftToRight() ? style.marginRight() : style.marginLeft(), availableWidth) : 0_lu;

        return { logicalLeftValue, logicalRightValue };
    }

    auto logicalLeftValue = retainMarginStart ? usedValueOrZero(writingMode.isInlineTopToBottom() ? style.marginTop() : style.marginBottom(), availableWidth) : 0_lu;
    auto logicalRightValue = retainMarginEnd ? usedValueOrZero(writingMode.isInlineTopToBottom() ? style.marginBottom() : style.marginTop(), availableWidth) : 0_lu;

    return { logicalLeftValue, logicalRightValue };
}

Layout::BoxGeometry::VerticalEdges BoxGeometryUpdater::verticalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode)
{
    auto& style = renderer.style();
    if (writingMode.isHorizontal())
        return { usedValueOrZero(style.marginTop(), availableWidth), usedValueOrZero(style.marginBottom(), availableWidth) };
    return { usedValueOrZero(style.marginRight(), availableWidth), usedValueOrZero(style.marginLeft(), availableWidth) };
}

Layout::BoxGeometry::Edges BoxGeometryUpdater::logicalBorder(const RenderBoxModelObject& renderer, WritingMode writingMode, bool isIntrinsicWidthMode, bool retainBorderStart, bool retainBorderEnd)
{
    auto& style = renderer.style();

    auto borderLeft = LayoutUnit { style.borderLeftWidth() };
    auto borderRight = LayoutUnit { style.borderRightWidth() };
    auto borderTop = LayoutUnit { style.borderTopWidth() };
    auto borderBottom = LayoutUnit { style.borderBottomWidth() };

    if (!isIntrinsicWidthMode)
        adjustBorderForTableAndFieldset(renderer, borderLeft, borderRight, borderTop, borderBottom);

    if (writingMode.isHorizontal()) {
        auto borderLogicalLeft = retainBorderStart ? writingMode.isInlineLeftToRight() ? borderLeft : borderRight : 0_lu;
        auto borderLogicalRight = retainBorderEnd ? writingMode.isInlineLeftToRight() ? borderRight : borderLeft : 0_lu;
        return { { borderLogicalLeft, borderLogicalRight }, { borderTop, borderBottom } };
    }

    auto borderLogicalLeft = retainBorderStart ? writingMode.isInlineTopToBottom() ? borderTop : borderBottom : 0_lu;
    auto borderLogicalRight = retainBorderEnd ? writingMode.isInlineTopToBottom() ? borderBottom : borderTop : 0_lu;
    return { { borderLogicalLeft, borderLogicalRight }, { borderRight, borderLeft } };
}

Layout::BoxGeometry::Edges BoxGeometryUpdater::logicalPadding(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode, bool retainPaddingStart, bool retainPaddingEnd)
{
    auto& style = renderer.style();

    auto paddingLeft = usedValueOrZero(style.paddingLeft(), availableWidth);
    auto paddingRight = usedValueOrZero(style.paddingRight(), availableWidth);
    auto paddingTop = usedValueOrZero(style.paddingTop(), availableWidth);
    auto paddingBottom = usedValueOrZero(style.paddingBottom(), availableWidth);

    if (writingMode.isHorizontal()) {
        auto paddingLogicalLeft = retainPaddingStart ? writingMode.isInlineLeftToRight() ? paddingLeft : paddingRight : 0_lu;
        auto paddingLogicalRight = retainPaddingEnd ? writingMode.isInlineLeftToRight() ? paddingRight : paddingLeft : 0_lu;
        return { { paddingLogicalLeft, paddingLogicalRight }, { paddingTop, paddingBottom } };
    }

    auto paddingLogicalLeft = retainPaddingStart ? writingMode.isInlineTopToBottom() ? paddingTop : paddingBottom : 0_lu;
    auto paddingLogicalRight = retainPaddingEnd ? writingMode.isInlineTopToBottom() ? paddingBottom : paddingTop : 0_lu;
    return { { paddingLogicalLeft, paddingLogicalRight }, { paddingRight, paddingLeft } };
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

static inline void setIntegrationBaseline(const RenderBox& renderBox, WritingMode writingMode)
{
    auto hasNonSyntheticBaseline = [&] {
        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(renderBox))
            return !renderListMarker->isImage();

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
        auto* blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox);
        if (!blockFlow)
            return false;
        auto hasAppareance = blockFlow->style().hasUsedAppearance() && !blockFlow->theme().isControlContainer(blockFlow->style().usedAppearance());
        return hasAppareance || !blockFlow->childrenInline() || blockFlow->hasLines() || blockFlow->hasLineIfEmpty();
    };

    if (!hasNonSyntheticBaseline())
        return;

    auto baseline = renderBox.baselinePosition(AlphabeticBaseline, false /* firstLine */, writingMode.isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
    const_cast<Layout::ElementBox&>(*renderBox.layoutBox()).setBaselineForIntegration(baseline);
}

void BoxGeometryUpdater::updateLayoutBoxDimensions(const RenderBox& renderBox, std::optional<LayoutUnit> availableWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& layoutBox = const_cast<Layout::ElementBox&>(*renderBox.layoutBox());
    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    auto writingMode = renderBox.parent()->writingMode();

    auto inlineMargin = horizontalLogicalMargin(renderBox, availableWidth, writingMode);
    auto border = logicalBorder(renderBox, writingMode, intrinsicWidthMode.has_value());
    auto padding = logicalPadding(renderBox, availableWidth, writingMode);
    if (!intrinsicWidthMode)
        padding.vertical += intrinsicPaddingForTableCell(renderBox);

    auto scrollbarSize = scrollbarLogicalSize(renderBox);

    if (intrinsicWidthMode) {
        boxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
        auto contentBoxLogicalWidth = [&] {
            auto preferredWidth = *intrinsicWidthMode == Layout::IntrinsicWidthMode::Minimum ? renderBox.minPreferredLogicalWidth() : renderBox.maxPreferredLogicalWidth();
            return preferredWidth - (border.horizontal.start + border.horizontal.end + padding.horizontal.start + padding.horizontal.end);
        };
        boxGeometry.setContentBoxWidth(contentBoxLogicalWidth());
        boxGeometry.setHorizontalMargin(inlineMargin);
        boxGeometry.setHorizontalBorder(border.horizontal);
        boxGeometry.setHorizontalPadding(padding.horizontal);
        return;
    }

    boxGeometry.setSpaceForScrollbar(scrollbarSize);

    boxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(renderBox));
    boxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(renderBox));

    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderBox, availableWidth, writingMode));
    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);
}

void BoxGeometryUpdater::updateLineBreakBoxDimensions(const RenderLineBreak& lineBreakBox)
{
    // This is just a box geometry reset (see InlineFormattingContext::layoutInFlowContent).
    layoutState().ensureGeometryForBox(*lineBreakBox.layoutBox()).reset();
}

void BoxGeometryUpdater::updateInlineBoxDimensions(const RenderInline& renderInline, std::optional<LayoutUnit> availableWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& boxGeometry = layoutState().ensureGeometryForBox(*renderInline.layoutBox());

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();
    auto writingMode = renderInline.writingMode();

    auto inlineMargin = horizontalLogicalMargin(renderInline, availableWidth, writingMode, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto border = logicalBorder(renderInline, writingMode, intrinsicWidthMode.has_value(), !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto padding = logicalPadding(renderInline, availableWidth, writingMode, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);

    if (intrinsicWidthMode) {
        boxGeometry.setHorizontalMargin(inlineMargin);
        boxGeometry.setHorizontalBorder(border.horizontal);
        boxGeometry.setHorizontalPadding(padding.horizontal);
        return;
    }

    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderInline, availableWidth, writingMode));
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);
}

void BoxGeometryUpdater::setFormattingContextContentGeometry(std::optional<LayoutUnit> availableLogicalWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    ASSERT(availableLogicalWidth || intrinsicWidthMode);

    if (rootLayoutBox().establishesInlineFormattingContext()) {
        for (auto walker = InlineWalker(downcast<RenderBlockFlow>(rootRenderer())); !walker.atEnd(); walker.advance()) {
            if (!is<RenderText>(walker.current()))
                updateBoxGeometry(downcast<RenderElement>(*walker.current()), availableLogicalWidth, intrinsicWidthMode);
        }
        return;
    }

    if (rootLayoutBox().establishesFlexFormattingContext()) {
        for (auto* flexItemOrOutOfFlowPositionedChild = rootLayoutBox().firstChild(); flexItemOrOutOfFlowPositionedChild; flexItemOrOutOfFlowPositionedChild = flexItemOrOutOfFlowPositionedChild->nextSibling())
            updateBoxGeometry(downcast<RenderElement>(*flexItemOrOutOfFlowPositionedChild->rendererForIntegration()), availableLogicalWidth, intrinsicWidthMode);
        return;
    }

    ASSERT_NOT_IMPLEMENTED_YET();
}

void BoxGeometryUpdater::setFormattingContextRootGeometry(LayoutUnit availableWidth)
{
    // FIXME: BFC should be responsible for creating the box geometry for this block box (IFC root) as part of the block layout.
    // This is really only required by float layout as IFC does not consult the root geometry directly.
    auto& rootRenderer = this->rootRenderer();
    auto writingMode = this->writingMode();

    auto padding = logicalPadding(rootRenderer, availableWidth, writingMode);
    auto border = logicalBorder(rootRenderer, writingMode);
    if (writingMode.isVertical() && !rootLayoutBox().writingMode().isBlockFlipped()) {
        padding.vertical = { padding.vertical.after, padding.vertical.before };
        border.vertical = { border.vertical.after, border.vertical.before };
    }

    auto& rootGeometry = layoutState().ensureGeometryForBox(rootLayoutBox());
    rootGeometry.setContentBoxWidth(writingMode.isHorizontal() ? rootRenderer.contentWidth() : rootRenderer.contentHeight());
    rootGeometry.setPadding(padding);
    rootGeometry.setBorder(border);
    rootGeometry.setSpaceForScrollbar(scrollbarLogicalSize(rootRenderer));
    rootGeometry.setHorizontalMargin(horizontalLogicalMargin(rootRenderer, availableWidth, writingMode));
    rootGeometry.setVerticalMargin(verticalLogicalMargin(rootRenderer, availableWidth, writingMode));
}

Layout::ConstraintsForInlineContent BoxGeometryUpdater::formattingContextConstraints(LayoutUnit availableWidth)
{
    auto& rootRenderer = this->rootRenderer();
    auto writingMode = this->writingMode();

    auto padding = logicalPadding(rootRenderer, availableWidth, writingMode);
    auto border = logicalBorder(rootRenderer, writingMode);
    if (writingMode.isVertical() && !writingMode.isBlockFlipped()) {
        padding.vertical = { padding.vertical.after, padding.vertical.before };
        border.vertical = { border.vertical.after, border.vertical.before };
    }
    padding.vertical += intrinsicPaddingForTableCell(rootRenderer);

    auto scrollbarSize = scrollbarLogicalSize(rootRenderer);
    auto shouldPlaceVerticalScrollbarOnLeft = rootRenderer.shouldPlaceVerticalScrollbarOnLeft();

    auto contentBoxWidth = writingMode.isHorizontal() ? rootRenderer.contentWidth() : rootRenderer.contentHeight();
    auto contentBoxLeft = border.horizontal.start + padding.horizontal.start;
    auto contentBoxTop = border.vertical.before + padding.vertical.before;
    if (writingMode.isInlineLeftToRight())
        contentBoxLeft += shouldPlaceVerticalScrollbarOnLeft ? scrollbarSize.width() : 0_lu;
    else if (writingMode.isBlockLeftToRight())
        contentBoxTop += shouldPlaceVerticalScrollbarOnLeft ? scrollbarSize.width() : 0_lu;

    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLeft, contentBoxWidth };
    auto visualLeft = writingMode.isBidiRTL() || shouldPlaceVerticalScrollbarOnLeft
        ? border.horizontal.end + scrollbarSize.width() + padding.horizontal.end
        : contentBoxLeft;

    return { { horizontalConstraints, contentBoxTop }, visualLeft };
}

void BoxGeometryUpdater::updateBoxGeometryAfterIntegrationLayout(const Layout::ElementBox& layoutBox, LayoutUnit availableWidth)
{
    auto* renderBox = dynamicDowncast<RenderBox>(layoutBox.rendererForIntegration());
    if (!renderBox) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    boxGeometry.setContentBoxSize(renderBox->contentLogicalSize());
    boxGeometry.setSpaceForScrollbar(scrollbarLogicalSize(*renderBox));

    auto integrationAdjustments = [&] {
        // FIXME: These should eventually be all absorbed by LFC layout.
        setIntegrationBaseline(*renderBox, writingMode());

        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(*renderBox)) {
            auto& style = layoutBox.parent().style();
            boxGeometry.setHorizontalMargin(horizontalLogicalMargin(*renderListMarker, { }, style.writingMode()));
            if (!renderListMarker->isInside())
                setListMarkerOffsetForMarkerOutside(*renderListMarker);
        }

        if (is<RenderTable>(*renderBox)) {
            // Tables have their special collapsed border values (updated at layout).
            auto& style = layoutBox.parent().style();
            boxGeometry.setBorder(logicalBorder(*renderBox, style.writingMode()));
        }

        auto needsFullGeometryUpdate = [&] {
            if (renderBox->isFieldset()) {
                // Fieldsets with legends have intrinsic padding values.
                return true;
            }
            if (renderBox->isWritingModeRoot()) {
                // Currently we've got one BoxGeometry for a layout box, but it represents geometry when
                // it is a root but also when it is an inline level box (e.g. floats, inline-blocks).
                return true;
            }
            if (!layoutBox.isInitialContainingBlock() && layoutBox.establishesFormattingContext()
                && layoutBox.writingMode().isInlineOpposing(layoutBox.parent().writingMode()))
                return true;
            return false;
        };
        if (needsFullGeometryUpdate())
            updateLayoutBoxDimensions(*renderBox, availableWidth);

        if (auto* shapeOutsideInfo = renderBox->shapeOutsideInfo())
            const_cast<Layout::ElementBox&>(layoutBox).setShape(&shapeOutsideInfo->computedShape());
    };
    integrationAdjustments();
}

void BoxGeometryUpdater::updateBoxGeometry(const RenderElement& renderer, std::optional<LayoutUnit> availableWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    ASSERT(availableWidth || intrinsicWidthMode);

    if (auto* renderBox = dynamicDowncast<RenderBox>(renderer)) {
        updateLayoutBoxDimensions(*renderBox, availableWidth, intrinsicWidthMode);
        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(renderer); renderListMarker && !renderListMarker->isInside())
            setListMarkerOffsetForMarkerOutside(*renderListMarker);
        return;
    }

    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer))
        return updateLineBreakBoxDimensions(*renderLineBreak);

    if (auto* renderInline = dynamicDowncast<RenderInline>(renderer))
        return updateInlineBoxDimensions(*renderInline, availableWidth, intrinsicWidthMode);
}

const Layout::ElementBox& BoxGeometryUpdater::rootLayoutBox() const
{
    return *m_rootLayoutBox;
}

const RenderBlock& BoxGeometryUpdater::rootRenderer() const
{
    return downcast<RenderBlock>(*rootLayoutBox().rendererForIntegration());
}

inline WritingMode BoxGeometryUpdater::writingMode() const
{
    return rootRenderer().writingMode();
}

}
}

