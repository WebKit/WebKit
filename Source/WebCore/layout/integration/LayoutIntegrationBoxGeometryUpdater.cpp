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
#include "LayoutIntegrationLineLayout.h"
#include "LegacyRenderSVGRoot.h"
#if ENABLE(MULTI_REPRESENTATION_HEIC)
#include "MultiRepresentationHEICMetrics.h"
#endif
#include "RenderAttachment.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderButton.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementInlines.h"
#include "RenderEmbeddedObject.h"
#include "RenderFlexibleBox.h"
#include "RenderFrameSet.h"
#include "RenderGrid.h"
#include "RenderHTMLCanvas.h"
#include "RenderIFrame.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMathMLBlock.h"
#include "RenderMenuList.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableInlines.h"
#include "RenderTextControlMultiLine.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "RenderViewTransitionCapture.h"

namespace WebCore {
namespace LayoutIntegration {

static LayoutUnit usedValueOrZero(const Style::MarginEdge& marginEdge, std::optional<LayoutUnit> availableWidth)
{
    if (auto fixed = marginEdge.tryFixed())
        return LayoutUnit { fixed->value };

    if (marginEdge.isAuto() || !availableWidth)
        return { };

    return Style::evaluateMinimum(marginEdge, *availableWidth);
}

static LayoutUnit usedValueOrZero(const Style::PaddingEdge& paddingEdge, std::optional<LayoutUnit> availableWidth)
{
    if (auto fixed = paddingEdge.tryFixed())
        return LayoutUnit { fixed->value };

    if (!availableWidth)
        return { };

    return Style::evaluateMinimum(paddingEdge, *availableWidth);
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
    return renderer.parent()->writingMode().isHorizontal() ? renderer.contentBoxWidth() : renderer.contentBoxHeight();
}

static inline LayoutUnit contentLogicalHeightForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->writingMode().isHorizontal() ? renderer.contentBoxHeight() : renderer.contentBoxWidth();
}

Layout::BoxGeometry::HorizontalEdges BoxGeometryUpdater::horizontalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode, bool retainMarginStart, bool retainMarginEnd)
{
    auto& style = renderer.style();

    if (writingMode.isHorizontal()) {
        auto marginInlineStart = retainMarginStart ? usedValueOrZero(writingMode.isInlineLeftToRight() ? style.marginLeft() : style.marginRight(), availableWidth) : 0_lu;
        auto marginInlineEnd = retainMarginEnd ? usedValueOrZero(writingMode.isInlineLeftToRight() ? style.marginRight() : style.marginLeft(), availableWidth) : 0_lu;

        return { marginInlineStart, marginInlineEnd };
    }

    auto marginInlineStart = retainMarginStart ? usedValueOrZero(writingMode.isInlineTopToBottom() ? style.marginTop() : style.marginBottom(), availableWidth) : 0_lu;
    auto marginInlineEnd = retainMarginEnd ? usedValueOrZero(writingMode.isInlineTopToBottom() ? style.marginBottom() : style.marginTop(), availableWidth) : 0_lu;

    return { marginInlineStart, marginInlineEnd };
}

Layout::BoxGeometry::VerticalEdges BoxGeometryUpdater::verticalLogicalMargin(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode)
{
    auto& style = renderer.style();
    if (writingMode.isHorizontal())
        return { usedValueOrZero(style.marginTop(), availableWidth), usedValueOrZero(style.marginBottom(), availableWidth) };
    if (writingMode.isLineOverLeft())
        return { usedValueOrZero(style.marginLeft(), availableWidth), usedValueOrZero(style.marginRight(), availableWidth) };
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
        auto borderInlineStart = retainBorderStart ? writingMode.isInlineLeftToRight() ? borderLeft : borderRight : 0_lu;
        auto borderInlineEnd = retainBorderEnd ? writingMode.isInlineLeftToRight() ? borderRight : borderLeft : 0_lu;
        return { { borderInlineStart, borderInlineEnd }, { borderTop, borderBottom } };
    }

    auto borderInlineStart = retainBorderStart ? writingMode.isInlineTopToBottom() ? borderTop : borderBottom : 0_lu;
    auto borderInlineEnd = retainBorderEnd ? writingMode.isInlineTopToBottom() ? borderBottom : borderTop : 0_lu;
    auto borderLineOver = writingMode.isLineOverRight() ? borderRight : borderLeft;
    auto borderLineUnder = writingMode.isLineOverRight() ? borderLeft : borderRight;
    return { { borderInlineStart, borderInlineEnd }, { borderLineOver, borderLineUnder } };
}

Layout::BoxGeometry::Edges BoxGeometryUpdater::logicalPadding(const RenderBoxModelObject& renderer, std::optional<LayoutUnit> availableWidth, WritingMode writingMode, bool retainPaddingStart, bool retainPaddingEnd)
{
    auto& style = renderer.style();

    auto paddingLeft = usedValueOrZero(style.paddingLeft(), availableWidth);
    auto paddingRight = usedValueOrZero(style.paddingRight(), availableWidth);
    auto paddingTop = usedValueOrZero(style.paddingTop(), availableWidth);
    auto paddingBottom = usedValueOrZero(style.paddingBottom(), availableWidth);

    if (writingMode.isHorizontal()) {
        auto paddingInlineStart = retainPaddingStart ? writingMode.isInlineLeftToRight() ? paddingLeft : paddingRight : 0_lu;
        auto paddingInlineEnd = retainPaddingEnd ? writingMode.isInlineLeftToRight() ? paddingRight : paddingLeft : 0_lu;
        return { { paddingInlineStart, paddingInlineEnd }, { paddingTop, paddingBottom } };
    }

    auto paddingInlineStart = retainPaddingStart ? writingMode.isInlineTopToBottom() ? paddingTop : paddingBottom : 0_lu;
    auto paddingInlineEnd = retainPaddingEnd ? writingMode.isInlineTopToBottom() ? paddingBottom : paddingTop : 0_lu;
    auto paddingLineOver = writingMode.isLineOverRight() ? paddingRight : paddingLeft;
    auto paddingLineUnder = writingMode.isLineOverRight() ? paddingLeft : paddingRight;
    return { { paddingInlineStart, paddingInlineEnd }, { paddingLineOver, paddingLineUnder } };
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

static std::optional<LayoutUnit> inlineBlockBaseline(const RenderBox&);

std::optional<LayoutUnit> lastInflowBoxBaseline(auto& blockContainer)
{
    auto writingMode = blockContainer.containingBlock()->writingMode();
    auto haveInFlowChild = false;
    for (auto* box = blockContainer.lastChildBox(); box; box = box->previousSiblingBox()) {
        if (box->isFloatingOrOutOfFlowPositioned())
            continue;
        haveInFlowChild = true;
        if (auto result = inlineBlockBaseline(*box))
            return LayoutUnit { (box->logicalTop() + result.value()).toInt() }; // Translate to our coordinate space.
    }

    if (!haveInFlowChild && blockContainer.hasLineIfEmpty()) {
        auto& fontMetrics = blockContainer.firstLineStyle().metricsOfPrimaryFont();
        return LayoutUnit { LayoutUnit(fontMetrics.intAscent()
            + (blockContainer.lineHeight() - fontMetrics.intHeight()) / 2
            + (writingMode.isHorizontal() ? blockContainer.borderTop() + blockContainer.paddingTop() : blockContainer.borderRight() + blockContainer.paddingRight())).toInt() };
    }
    return { };
}

static std::optional<LayoutUnit> inlineBlockBaseline(const RenderBox& renderBox)
{
    auto writingMode = renderBox.containingBlock()->writingMode();
    auto lineDirection = writingMode.isHorizontal() ? HorizontalLine : VerticalLine;

    if (is<RenderTable>(renderBox))
        return { };

    if ((is<RenderFlexibleBox>(renderBox) || is<RenderGrid>(renderBox)) && !is<RenderMenuList>(renderBox) && !is<RenderTextControlInnerContainer>(renderBox))
        return renderBox.firstLineBaseline();

    if (renderBox.isWritingModeRoot())
        return { };

    if (renderBox.shouldApplyLayoutContainment()) {
        if (renderBox.isInline())
            return synthesizedBaseline(renderBox, *renderBox.parentStyle(), lineDirection, BorderBox) + (writingMode.isHorizontal() ? renderBox.marginBottom() : renderBox.marginLeft());
        return { };
    }

    if (CheckedPtr innerContainer = dynamicDowncast<RenderTextControlInnerContainer>(renderBox))
        return lastInflowBoxBaseline(*innerContainer);

    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox)) {
        RefPtr element = blockFlow->element();
        auto isFormControlElement = element && element->isFormControlElement();
        if (blockFlow->style().display() == DisplayType::InlineBlock) {
            // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or if its 'overflow'
            // property has a computed value other than 'visible'. see https://www.w3.org/TR/CSS22/visudet.html
            auto shouldSynthesizeBaseline = !blockFlow->style().isOverflowVisible() && !isFormControlElement && !is<RenderTextControlInnerBlock>(*blockFlow);
            if (shouldSynthesizeBaseline)
                return { };
        }

        // Note that here we only take the left and bottom into consideration. Our caller takes the right and top into consideration.
        LayoutUnit lastBaseline;
        if (!blockFlow->childrenInline()) {
            auto inlineBlockBaseline = lastInflowBoxBaseline(*blockFlow);
            if (!inlineBlockBaseline)
                return { };
            lastBaseline = *inlineBlockBaseline;
        } else if (!blockFlow->hasLines()) {
            if (!blockFlow->hasLineIfEmpty())
                return { };
            auto& fontMetrics = blockFlow->firstLineStyle().metricsOfPrimaryFont();
            return LayoutUnit { LayoutUnit(fontMetrics.intAscent()
                + (blockFlow->lineHeight() - fontMetrics.intHeight()) / 2
                + (lineDirection == HorizontalLine ? blockFlow->borderTop() + blockFlow->paddingTop() : blockFlow->borderRight() + blockFlow->paddingRight())).toInt() };
        } else if (blockFlow->svgTextLayout()) {
            auto& style = blockFlow->firstLineStyle();
            // LegacyInlineFlowBox::placeBoxesInBlockDirection will flip lines in case of verticalLR mode, so we can assume verticalRL for now.
            lastBaseline = style.metricsOfPrimaryFont().intAscent(blockFlow->legacyRootBox()->baselineType()) + (style.writingMode().isLineInverted() ? blockFlow->logicalHeight() - blockFlow->legacyRootBox()->logicalBottom() : blockFlow->legacyRootBox()->logicalTop());
        } else if (auto* inlineLayout = blockFlow->inlineLayout())
            lastBaseline = floorToInt(inlineLayout->lastLineLogicalBaseline());

        if (blockFlow->style().overflowY() == Overflow::Visible)
            return lastBaseline;

        auto boxHeight = synthesizedBaseline(renderBox, *blockFlow->parentStyle(), lineDirection, BorderBox) + (writingMode.isHorizontal() ? blockFlow->marginBottom() : blockFlow->marginLeft());
        // While css-align-3 defines the last baseline form block containers which are
        // scroll containers (with an initial baseline-source value) as the block-end margin
        // edge, we instead maintain our legacy behavior for form controls and content
        // inside form controls to maintain compatibility.
        auto isInFormControl = element && element->shadowHost() && element->shadowHost()->isFormControlElement();
        if (isFormControlElement || isInFormControl)
            return std::min(boxHeight, lastBaseline);
        return boxHeight;
    }

    if (CheckedPtr blockRenderer = dynamicDowncast<RenderBlock>(renderBox))
        return lastInflowBoxBaseline(*blockRenderer);

    return { };
}

LayoutUnit static baselinePosition(const RenderBox& renderBox)
{
    ASSERT(renderBox.isInFlow());

    auto writingMode = renderBox.containingBlock()->writingMode();
    auto marginBefore = writingMode.isHorizontal() ? renderBox.marginTop() : renderBox.marginRight();

    if (is<RenderIFrame>(renderBox)
        || is<RenderEmbeddedObject>(renderBox)
        || is<LegacyRenderSVGRoot>(renderBox)
        || is<RenderHTMLCanvas>(renderBox)
        || is<RenderViewTransitionCapture>(renderBox))
        return roundToInt(renderBox.marginBoxLogicalHeight(writingMode));

#if ENABLE(ATTACHMENT_ELEMENT)
    if (CheckedPtr renderer = dynamicDowncast<RenderAttachment>(renderBox)) {
        if (auto* baselineElement = renderer->attachmentElement().wideLayoutImageElement()) {
            if (auto* baselineElementRenderBox = baselineElement->renderBox()) {
                // This is the bottom of the image assuming it is vertically centered.
                return (renderer->height() + baselineElementRenderBox->height()) / 2;
            }
            // Fallback to the bottom of the attachment if there is no image.
            return renderer->height();
        }
        return renderer->theme().attachmentBaseline(*renderer);
    }
#endif

    if (CheckedPtr renderer = dynamicDowncast<RenderButton>(renderBox)) {
        // We cannot rely on RenderFlexibleBox::baselinePosition() because of flexboxes have some special behavior
        // regarding baselines that shouldn't apply to buttons.
        if (auto baseline = renderBox.firstLineBaseline())
            return marginBefore + *baseline;
        auto contentBoxBottom = writingMode.isHorizontal() ? renderer->borderTop() + renderer->paddingTop() + renderer->contentBoxHeight() : renderer->borderRight() + renderer->paddingRight() + renderer->contentBoxWidth();
        return marginBefore + contentBoxBottom;
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderImage>(renderBox)) {
        auto offset = LayoutUnit { };
#if ENABLE(MULTI_REPRESENTATION_HEIC)
        if (renderer->isMultiRepresentationHEIC()) {
            auto metrics = renderer->style().fontCascade().primaryFont()->metricsForMultiRepresentationHEIC();
            offset = LayoutUnit::fromFloatRound(metrics.descent);
        }
#endif
        return roundToInt(renderer->marginBoxLogicalHeight(writingMode)) - offset;
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderListBox>(renderBox)) {
        // FIXME: This hardcoded baselineAdjustment is what we used to do for the old
        // widget, but I'm not sure this is right for the new control.
        const int baselineAdjustment = 7;
        auto baseline = roundToInt(renderer->marginBoxLogicalHeight(writingMode));
        if (renderer->shouldApplyLayoutContainment())
            return baseline;
        return baseline - baselineAdjustment;
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderTextControlMultiLine>(renderBox))
        return roundToInt(renderer->marginBoxLogicalHeight(writingMode));

    if (CheckedPtr renderer = dynamicDowncast<RenderSlider>(renderBox)) {
        // FIXME: Patch this function for writing-mode.
        return renderer->height() + renderer->marginTop();
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderTable>(renderBox)) {
        if (auto baselinePos = renderBox.firstLineBaseline())
            return marginBefore + *baselinePos;
        return roundToInt(renderer->marginBoxLogicalHeight(writingMode));
    }

    if ((is<RenderFlexibleBox>(renderBox) || is<RenderGrid>(renderBox)) && !is<RenderMenuList>(renderBox)) {
        if (auto baseline = renderBox.firstLineBaseline())
            return marginBefore.toInt() + *baseline;
        return synthesizedBaseline(renderBox, *renderBox.parentStyle(), writingMode.isHorizontal() ? HorizontalLine : VerticalLine, BorderBox) + renderBox.marginLogicalHeight();
    }

    if (renderBox.style().hasUsedAppearance() && !renderBox.theme().isControlContainer(renderBox.style().usedAppearance())) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        return renderBox.theme().baselinePosition(renderBox);
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderListMarker>(renderBox)) {
        if (CheckedPtr listItem = renderer->listItem(); listItem && !renderer->isImage()) {
            auto& fontMetrics = renderer->style().metricsOfPrimaryFont();
            return LayoutUnit { fontMetrics.intAscent() + (renderer->lineHeight() - fontMetrics.intHeight()) / 2 }.toInt();
        }
        return roundToInt(renderBox.marginBoxLogicalHeight(writingMode));
    }

#if ENABLE(MATHML)
    if (is<RenderMathMLBlock>(renderBox)) {
        if (auto baseline = renderBox.firstLineBaseline())
            return *baseline;
        // Fallback to regular block level baseline.
    }
#endif

    if (CheckedPtr renderer = dynamicDowncast<RenderBlock>(renderBox)) {
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow. We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves). This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        auto ignoreBaseline = [&] {
            if (renderer->isWritingModeRoot())
                return true;

            CheckedPtr scrollableArea = renderer->layer() ? renderer->layer()->scrollableArea() : nullptr;
            if (!scrollableArea)
                return false;

            if (scrollableArea->marquee())
                return true;

            if (writingMode.isHorizontal())
                return scrollableArea->verticalScrollbar() || scrollableArea->scrollOffset().y();
            return scrollableArea->horizontalScrollbar() || scrollableArea->scrollOffset().x();
        };

        if (!ignoreBaseline()) {
            auto inlineBlockBaselinePosition = inlineBlockBaseline(renderBox);
            if (is<RenderDeprecatedFlexibleBox>(renderBox) && inlineBlockBaselinePosition) {
                // Historically, we did this check for all baselines. But we can't
                // remove this code from deprecated flexbox, because it effectively
                // breaks -webkit-line-clamp, which is used in the wild -- we would
                // calculate the baseline as if -webkit-line-clamp wasn't used.
                // For simplicity, we use this for all uses of deprecated flexbox.
                auto bottomOfContent = renderBox.borderBefore() + renderBox.paddingBefore() + renderBox.contentBoxLogicalHeight();
                if (*inlineBlockBaselinePosition > bottomOfContent)
                    return roundToInt(renderBox.marginBoxLogicalHeight(writingMode));
            }
            if (inlineBlockBaselinePosition)
                return marginBefore + *inlineBlockBaselinePosition;
        }
    }
    return roundToInt(renderBox.marginBoxLogicalHeight(writingMode));
}

static inline void setIntegrationBaseline(const RenderBox& renderBox)
{
    if (renderBox.isFloatingOrOutOfFlowPositioned())
        return;

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

    if (hasNonSyntheticBaseline())
        const_cast<Layout::ElementBox&>(*renderBox.layoutBox()).setBaselineForIntegration(baselinePosition(renderBox));
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
    rootGeometry.setContentBoxWidth(writingMode.isHorizontal() ? rootRenderer.contentBoxWidth() : rootRenderer.contentBoxHeight());
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

    if (rootRenderer.isRenderSVGText()) {
        auto horizontalConstraints = Layout::HorizontalConstraints { 0_lu, LayoutUnit::max() };
        return { { horizontalConstraints, 0_lu }, 0_lu, rootRenderer.size() };
    }

    auto padding = logicalPadding(rootRenderer, availableWidth, writingMode);
    auto border = logicalBorder(rootRenderer, writingMode);
    if (writingMode.isVertical() && writingMode.isLineInverted()) {
        padding.vertical = { padding.vertical.after, padding.vertical.before };
        border.vertical = { border.vertical.after, border.vertical.before };
    }
    padding.vertical += intrinsicPaddingForTableCell(rootRenderer);

    auto scrollbarSize = scrollbarLogicalSize(rootRenderer);
    auto shouldPlaceVerticalScrollbarOnLeft = rootRenderer.shouldPlaceVerticalScrollbarOnLeft();

    auto contentBoxWidth = writingMode.isHorizontal() ? rootRenderer.contentBoxWidth() : rootRenderer.contentBoxHeight();
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

    return { { horizontalConstraints, contentBoxTop }, visualLeft, rootRenderer.size() };
}

void BoxGeometryUpdater::updateBoxGeometryAfterIntegrationLayout(const Layout::ElementBox& layoutBox, LayoutUnit availableWidth)
{
    auto* renderBox = dynamicDowncast<RenderBox>(layoutBox.rendererForIntegration());
    if (!renderBox) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    boxGeometry.setContentBoxSize(renderBox->contentBoxLogicalSize());
    boxGeometry.setSpaceForScrollbar(scrollbarLogicalSize(*renderBox));

    auto integrationAdjustments = [&] {
        // FIXME: These should eventually be all absorbed by LFC layout.
        setIntegrationBaseline(*renderBox);

        if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(*renderBox)) {
            auto& style = layoutBox.parent().style();
            boxGeometry.setHorizontalMargin(horizontalLogicalMargin(*renderListMarker, { }, style.writingMode()));
            if (!renderListMarker->isInside())
                setListMarkerOffsetForMarkerOutside(*renderListMarker);
            const_cast<Layout::ElementBox&>(layoutBox).setListMarkerLayoutBounds(renderListMarker->layoutBounds());
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

