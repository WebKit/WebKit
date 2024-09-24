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
        switch (style.blockFlowDirection()) {
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

static inline std::pair<LayoutUnit, LayoutUnit> intrinsicPaddingForTable(const RenderBoxModelObject& renderer)
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
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentWidth() : renderer.contentHeight();
}

static inline LayoutUnit contentLogicalHeightForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentHeight() : renderer.contentWidth();
}

static inline Layout::BoxGeometry::HorizontalEdges horizontalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, bool isLeftToRightInlineDirection, bool isHorizontalWritingMode, bool retainMarginStart = true, bool retainMarginEnd = true)
{
    auto& style = renderer.style();

    if (isHorizontalWritingMode) {
        auto logicalLeftValue = retainMarginStart ? usedValueOrZero(isLeftToRightInlineDirection ? style.marginLeft() : style.marginRight(), availableWidth) : 0_lu;
        auto logicalRightValue = retainMarginEnd ? usedValueOrZero(isLeftToRightInlineDirection ? style.marginRight() : style.marginLeft(), availableWidth) : 0_lu;

        return { logicalLeftValue, logicalRightValue };
    }

    auto logicalLeftValue = retainMarginStart ? usedValueOrZero(isLeftToRightInlineDirection ? style.marginTop() : style.marginBottom(), availableWidth) : 0_lu;
    auto logicalRightValue = retainMarginEnd ? usedValueOrZero(isLeftToRightInlineDirection ? style.marginBottom() : style.marginTop(), availableWidth) : 0_lu;

    return { logicalLeftValue, logicalRightValue };
}

static inline Layout::BoxGeometry::VerticalEdges verticalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, FlowDirection blockFlowDirection)
{
    auto& style = renderer.style();
    switch (blockFlowDirection) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        return { usedValueOrZero(style.marginTop(), availableWidth), usedValueOrZero(style.marginBottom(), availableWidth) };
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        return { usedValueOrZero(style.marginRight(), availableWidth), usedValueOrZero(style.marginLeft(), availableWidth) };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

enum class IsPartOfFormattingContext : bool { No, Yes };
static inline Layout::BoxGeometry::Edges logicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, FlowDirection blockFlowDirection, bool isIntrinsicWidthMode, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainBorderStart = true, bool retainBorderEnd = true)
{
    auto& style = renderer.style();

    auto borderLeft = LayoutUnit { style.borderLeftWidth() };
    auto borderRight = LayoutUnit { style.borderRightWidth() };
    auto borderTop = LayoutUnit { style.borderTopWidth() };
    auto borderBottom = LayoutUnit { style.borderBottomWidth() };

    if (!isIntrinsicWidthMode)
        adjustBorderForTableAndFieldset(renderer, borderLeft, borderRight, borderTop, borderBottom);

    if (blockFlowDirection == FlowDirection::TopToBottom || blockFlowDirection == FlowDirection::BottomToTop) {
        if (isLeftToRightInlineDirection)
            return { { retainBorderStart ? borderLeft : 0_lu, retainBorderEnd ? borderRight : 0_lu }, { borderTop, borderBottom } };
        return { { retainBorderStart ? borderRight : 0_lu, retainBorderEnd ? borderLeft : 0_lu }, { borderTop, borderBottom } };
    }

    auto borderLogicalLeft = retainBorderStart ? isLeftToRightInlineDirection ? borderTop : borderBottom : 0_lu;
    auto borderLogicalRight = retainBorderEnd ? isLeftToRightInlineDirection ? borderBottom : borderTop : 0_lu;
    // For boxes inside the formatting context, right border (padding) always points up, while when converting the formatting context root's border (padding) the directionality matters.
    auto borderLogicalTop = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? borderRight : blockFlowDirection == FlowDirection::LeftToRight ? borderLeft : borderRight;
    auto borderLogicalBottom = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? borderLeft : blockFlowDirection == FlowDirection::LeftToRight ? borderRight : borderLeft;
    return { { borderLogicalLeft, borderLogicalRight }, { borderLogicalTop, borderLogicalBottom } };
}

static inline Layout::BoxGeometry::Edges logicalPadding(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, bool isLeftToRightInlineDirection, FlowDirection blockFlowDirection, bool isIntrinsicWidthMode = false, IsPartOfFormattingContext isPartOfFormattingContext = IsPartOfFormattingContext::No, bool retainPaddingStart = true, bool retainPaddingEnd = true)
{
    auto& style = renderer.style();

    auto paddingLeft = usedValueOrZero(style.paddingLeft(), availableWidth);
    auto paddingRight = usedValueOrZero(style.paddingRight(), availableWidth);
    auto paddingTop = usedValueOrZero(style.paddingTop(), availableWidth);
    auto paddingBottom = usedValueOrZero(style.paddingBottom(), availableWidth);
    auto [intrinsicPaddingBefore, intrinsicPaddingAfter] = !isIntrinsicWidthMode ? intrinsicPaddingForTable(renderer) : std::pair<LayoutUnit, LayoutUnit>();

    if (blockFlowDirection == FlowDirection::TopToBottom || blockFlowDirection == FlowDirection::BottomToTop) {
        if (isLeftToRightInlineDirection)
            return { { retainPaddingStart ? paddingLeft : 0_lu, retainPaddingEnd ? paddingRight : 0_lu }, { paddingTop + intrinsicPaddingBefore, paddingBottom + intrinsicPaddingAfter } };
        return { { retainPaddingStart ? paddingRight : 0_lu, retainPaddingEnd ? paddingLeft : 0_lu }, { paddingTop + intrinsicPaddingBefore, paddingBottom + intrinsicPaddingAfter } };
    }

    auto paddingLogicalLeft = retainPaddingStart ? isLeftToRightInlineDirection ? paddingTop : paddingBottom : 0_lu;
    auto paddingLogicalRight = retainPaddingEnd ? isLeftToRightInlineDirection ? paddingBottom : paddingTop : 0_lu;
    // For boxes inside the formatting context, right padding (border) always points up, while when converting the formatting context root's padding (border) the directionality matters.
    auto paddingLogicalTop = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? paddingRight : blockFlowDirection == FlowDirection::LeftToRight ? paddingLeft : paddingRight;
    auto paddingLogicalBottom = isPartOfFormattingContext == IsPartOfFormattingContext::Yes ? paddingLeft : blockFlowDirection == FlowDirection::LeftToRight ? paddingRight : paddingLeft;
    return { { paddingLogicalLeft, paddingLogicalRight }, { paddingLogicalTop + intrinsicPaddingBefore, paddingLogicalBottom + intrinsicPaddingAfter } };
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

static inline void setIntegrationBaseline(const RenderBox& renderBox)
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

    auto blockFlowDirection = writingModeToBlockFlowDirection(renderBox.parent()->style().writingMode());
    auto baseline = renderBox.baselinePosition(AlphabeticBaseline, false /* firstLine */, blockFlowDirection == FlowDirection::TopToBottom || blockFlowDirection == FlowDirection::BottomToTop ? HorizontalLine : VerticalLine, PositionOnContainingLine);
    const_cast<Layout::ElementBox&>(*renderBox.layoutBox()).setBaselineForIntegration(baseline);
}

void BoxGeometryUpdater::updateLayoutBoxDimensions(const RenderBox& renderBox, std::optional<LayoutUnit> availableWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& layoutBox = const_cast<Layout::ElementBox&>(*renderBox.layoutBox());
    auto isLeftToRightInlineDirection = renderBox.parent()->style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(renderBox.parent()->style().writingMode());
    auto isHorizontalWritingMode = blockFlowDirection == FlowDirection::TopToBottom || blockFlowDirection == FlowDirection::BottomToTop;

    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    auto inlineMargin = horizontalLogicalMargin(renderBox, availableWidth, isLeftToRightInlineDirection, isHorizontalWritingMode);
    auto border = logicalBorder(renderBox, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode.has_value());
    auto padding = logicalPadding(renderBox, availableWidth, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode.has_value());
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

    boxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
    boxGeometry.setVerticalSpaceForScrollbar(scrollbarSize.height());

    boxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(renderBox));
    boxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(renderBox));

    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderBox, availableWidth, blockFlowDirection));
    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);
}

void BoxGeometryUpdater::updateLineBreakBoxDimensions(const RenderLineBreak& lineBreakBox)
{
    // This is just a box geometry reset (see InlineFormattingContext::layoutInFlowContent).
    auto& boxGeometry = layoutState().ensureGeometryForBox(*lineBreakBox.layoutBox());

    boxGeometry.setHorizontalMargin({ });
    boxGeometry.setBorder({ });
    boxGeometry.setPadding({ });
    boxGeometry.setContentBoxWidth({ });
    boxGeometry.setVerticalMargin({ });
    if (lineBreakBox.style().hasOutOfFlowPosition())
        boxGeometry.setContentBoxHeight({ });
}

void BoxGeometryUpdater::updateInlineBoxDimensions(const RenderInline& renderInline, std::optional<LayoutUnit> availableWidth, std::optional<Layout::IntrinsicWidthMode> intrinsicWidthMode)
{
    auto& boxGeometry = layoutState().ensureGeometryForBox(*renderInline.layoutBox());

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();

    auto isLeftToRightInlineDirection = renderInline.style().isLeftToRightDirection();
    auto blockFlowDirection = writingModeToBlockFlowDirection(renderInline.style().writingMode());
    auto isHorizontalWritingMode = blockFlowDirection == FlowDirection::TopToBottom || blockFlowDirection == FlowDirection::BottomToTop;

    auto inlineMargin = horizontalLogicalMargin(renderInline, availableWidth, isLeftToRightInlineDirection, isHorizontalWritingMode, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto border = logicalBorder(renderInline, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode.has_value(), IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);
    auto padding = logicalPadding(renderInline, availableWidth, isLeftToRightInlineDirection, blockFlowDirection, intrinsicWidthMode.has_value(), IsPartOfFormattingContext::Yes, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd);

    if (intrinsicWidthMode) {
        boxGeometry.setHorizontalMargin(inlineMargin);
        boxGeometry.setHorizontalBorder(border.horizontal);
        boxGeometry.setHorizontalPadding(padding.horizontal);
        return;
    }

    boxGeometry.setHorizontalMargin(inlineMargin);
    boxGeometry.setVerticalMargin(verticalLogicalMargin(renderInline, availableWidth, blockFlowDirection));
    boxGeometry.setBorder(border);
    boxGeometry.setPadding(padding);
}

void BoxGeometryUpdater::setGeometriesForLayout(LayoutUnit availableLogicalWidth)
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(rootRenderer())); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        if (is<RenderText>(renderer))
            continue;

        updateBoxGeometry(downcast<RenderElement>(renderer), availableLogicalWidth);
    }
}

void BoxGeometryUpdater::setGeometriesForIntrinsicWidth(Layout::IntrinsicWidthMode intrinsicWidthMode)
{
    for (auto walker = InlineWalker(downcast<RenderBlockFlow>(rootRenderer())); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        if (is<RenderText>(renderer))
            continue;

        if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer)) {
            updateLineBreakBoxDimensions(*renderLineBreak);
            continue;
        }

        if (auto* renderInline = dynamicDowncast<RenderInline>(renderer)) {
            updateInlineBoxDimensions(*renderInline, { }, intrinsicWidthMode);
            continue;
        }

        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer)) {
            ASSERT(renderBox->style().logicalWidth().isFixed() || is<RenderListMarker>(*renderBox));
            updateLayoutBoxDimensions(*renderBox, { }, intrinsicWidthMode);
        }
    }
}

Layout::ConstraintsForInlineContent BoxGeometryUpdater::updateInlineContentConstraints(LayoutUnit availableWidth)
{
    auto& rootRenderer = this->rootRenderer();
    auto isLeftToRightInlineDirection = rootRenderer.style().isLeftToRightDirection();
    auto writingMode = rootRenderer.style().writingMode();
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);

    auto padding = logicalPadding(rootRenderer, availableWidth, isLeftToRightInlineDirection, blockFlowDirection, false, IsPartOfFormattingContext::No);
    auto border = logicalBorder(rootRenderer, isLeftToRightInlineDirection, blockFlowDirection, false, IsPartOfFormattingContext::No);
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
        rootGeometry.setHorizontalMargin(horizontalLogicalMargin(rootRenderer, availableWidth, isLeftToRightInlineDirection, WebCore::isHorizontalWritingMode(writingMode)));
        rootGeometry.setVerticalMargin(verticalLogicalMargin(rootRenderer, availableWidth, blockFlowDirection));
    };
    createRootGeometryIfNeeded();

    return { { horizontalConstraints, contentBoxTop }, visualLeft };
}

void BoxGeometryUpdater::updateGeometryAfterLayout(const Layout::ElementBox& layoutBox, LayoutUnit availableWidth)
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
        setIntegrationBaseline(*renderBox);

        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(*renderBox)) {
            auto& style = layoutBox.parent().style();
            boxGeometry.setHorizontalMargin(horizontalLogicalMargin(*renderListMarker, { }, style.isLeftToRightDirection(), style.isHorizontalWritingMode()));
            if (!renderListMarker->isInside())
                setListMarkerOffsetForMarkerOutside(*renderListMarker);
        }

        if (is<RenderTable>(*renderBox)) {
            // Tables have their special collapsed border values (updated at layout).
            auto& style = layoutBox.parent().style();
            boxGeometry.setBorder(logicalBorder(*renderBox, style.isLeftToRightDirection(), style.blockFlowDirection(), false));
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
            if (!layoutBox.isInitialContainingBlock() && layoutBox.establishesFormattingContext() && layoutBox.style().isLeftToRightDirection() != layoutBox.parent().style().isLeftToRightDirection())
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

void BoxGeometryUpdater::updateBoxGeometry(const RenderElement& renderer, LayoutUnit availableWidth)
{
    if (auto* renderBox = dynamicDowncast<RenderBox>(renderer)) {
        updateLayoutBoxDimensions(*renderBox, availableWidth);
        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(renderer); renderListMarker && !renderListMarker->isInside())
            setListMarkerOffsetForMarkerOutside(*renderListMarker);
        return;
    }

    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer))
        return updateLineBreakBoxDimensions(*renderLineBreak);

    if (auto* renderInline = dynamicDowncast<RenderInline>(renderer))
        return updateInlineBoxDimensions(*renderInline, availableWidth);
}

const Layout::ElementBox& BoxGeometryUpdater::rootLayoutBox() const
{
    return *m_rootLayoutBox;
}

const RenderBlock& BoxGeometryUpdater::rootRenderer() const
{
    return downcast<RenderBlock>(*rootLayoutBox().rendererForIntegration());
}

}
}

