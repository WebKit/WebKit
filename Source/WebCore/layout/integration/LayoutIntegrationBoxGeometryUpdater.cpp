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
#include "RenderElementStyleInlines.h"
#include "RenderEmbeddedObject.h"
#include "RenderFileUploadControl.h"
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
#include "RenderModel.h"
#include "RenderSVGRoot.h"
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
        return LayoutUnit { fixed->resolveZoom(Style::ZoomNeeded { }) };

    if (marginEdge.isAuto() || !availableWidth)
        return { };

    return Style::evaluateMinimum<LayoutUnit>(marginEdge, *availableWidth, Style::ZoomNeeded { });
}

static LayoutUnit usedValueOrZero(const Style::PaddingEdge& paddingEdge, std::optional<LayoutUnit> availableWidth)
{
    if (auto fixed = paddingEdge.tryFixed())
        return LayoutUnit { fixed->resolveZoom(Style::ZoomNeeded { }) };

    if (!availableWidth)
        return { };

    return Style::evaluateMinimum<LayoutUnit>(paddingEdge, *availableWidth, Style::ZoomNeeded { });
}

static inline void adjustBorderForTableAndFieldset(const RenderBoxModelObject& renderer, RectEdges<LayoutUnit>& borderWidths)
{
    if (auto* table = dynamicDowncast<RenderTable>(renderer); table && table->collapseBorders()) {
        borderWidths = table->borderWidths();
        return;
    }

    if (auto* tableCell = dynamicDowncast<RenderTableCell>(renderer); tableCell && tableCell->table()->collapseBorders()) {
        borderWidths = tableCell->borderWidths();
        return;
    }

    if (renderer.isFieldset()) {
        auto adjustment = downcast<RenderBlock>(renderer).intrinsicBorderForFieldset();
        // Note that this adjustment is coming from _inside_ the fieldset so its own flow direction is what is relevant here.
        auto& style = renderer.style();
        switch (style.writingMode().blockDirection()) {
        case FlowDirection::TopToBottom:
            borderWidths.top() += adjustment;
            break;
        case FlowDirection::BottomToTop:
            borderWidths.bottom() += adjustment;
            break;
        case FlowDirection::LeftToRight:
            borderWidths.left() += adjustment;
            break;
        case FlowDirection::RightToLeft:
            borderWidths.right() += adjustment;
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
            offset -= (ancestor->marginStart() + ancestor->borderStart() + ancestor->paddingStart());
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

    auto borderWidths = RectEdges<LayoutUnit>::map(style.borderWidth(), [](auto width) {
        return Style::evaluate<LayoutUnit>(width, Style::ZoomNeeded { });
    });

    if (!isIntrinsicWidthMode)
        adjustBorderForTableAndFieldset(renderer, borderWidths);

    if (writingMode.isHorizontal()) {
        auto borderInlineStart = retainBorderStart ? writingMode.isInlineLeftToRight() ? borderWidths.left() : borderWidths.right() : 0_lu;
        auto borderInlineEnd = retainBorderEnd ? writingMode.isInlineLeftToRight() ? borderWidths.right() : borderWidths.left() : 0_lu;
        return { { borderInlineStart, borderInlineEnd }, { borderWidths.top(), borderWidths.bottom() } };
    }

    auto borderInlineStart = retainBorderStart ? writingMode.isInlineTopToBottom() ? borderWidths.top() : borderWidths.bottom() : 0_lu;
    auto borderInlineEnd = retainBorderEnd ? writingMode.isInlineTopToBottom() ? borderWidths.bottom() : borderWidths.top() : 0_lu;
    auto borderLineOver = writingMode.isLineOverRight() ? borderWidths.right() : borderWidths.left();
    auto borderLineUnder = writingMode.isLineOverRight() ? borderWidths.left() : borderWidths.right();
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

static LayoutUnit fontMetricsBasedBaseline(const RenderBox& renderBox)
{
    auto& fontMetrics = renderBox.firstLineStyle().metricsOfPrimaryFont();
    return fontMetrics.intAscent() + (renderBox.lineHeight() - fontMetrics.intHeight()) / 2;
}

static bool shouldUseMarginBoxAsBaseline(const RenderBox& renderBox)
{
    if (is<RenderTable>(renderBox))
        return true;

    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox)) {
        // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or if its 'overflow'
        // property has a computed value other than 'visible'. see https://www.w3.org/TR/CSS22/visudet.html
        if (blockFlow->style().display() == DisplayType::InlineBlock && !blockFlow->style().isOverflowVisible())
            return true;

        if (blockFlow->childrenInline() && !blockFlow->hasLines() && !blockFlow->hasLineIfEmpty())
            return true;

        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow. We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves). This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved).
        CheckedPtr scrollableArea = blockFlow->layer() ? blockFlow->layer()->scrollableArea() : nullptr;
        if (scrollableArea) {
            if (scrollableArea->marquee())
                return true;

            if (blockFlow->containingBlock()->writingMode().isHorizontal())
                return scrollableArea->verticalScrollbar() || scrollableArea->scrollOffset().y();
            return scrollableArea->horizontalScrollbar() || scrollableArea->scrollOffset().x();
        }
    }
    return false;
}

static std::optional<LayoutUnit> baselineForBox(const RenderBox&);

static std::optional<LayoutUnit> lastInflowBoxBaseline(const RenderBlock& blockContainer)
{
    auto* lastInFlowChild = blockContainer.lastInFlowChildBox();
    for (auto* inflowBox = lastInFlowChild; inflowBox; inflowBox = inflowBox->previousInFlowSiblingBox()) {
        if (inflowBox->isWritingModeRoot())
            continue;

        if (inflowBox->shouldApplyLayoutContainment())
            continue;

        if (shouldUseMarginBoxAsBaseline(*inflowBox)) {
            // We need to find a better candidate for baseline.
            continue;
        }

        if (is<RenderFlexibleBox>(*inflowBox) || is<RenderGrid>(*inflowBox) || is<RenderBlockFlow>(*inflowBox) || is<RenderTextControlInnerContainer>(*inflowBox) || is<RenderMenuList>(*inflowBox)) {
            if (auto baseline = baselineForBox(*inflowBox))
                return LayoutUnit { (inflowBox->logicalTop() + *baseline).toInt() };
            continue;
        }
    }

    if (!lastInFlowChild && blockContainer.hasLineIfEmpty())
        return (fontMetricsBasedBaseline(blockContainer) + (blockContainer.containingBlock()->writingMode().isHorizontal() ? blockContainer.borderTop() + blockContainer.paddingTop() : blockContainer.borderRight() + blockContainer.paddingRight())).toInt();

    return { };
}

static std::optional<LayoutUnit> baselineForBox(const RenderBox& renderBox)
{
    auto writingMode = renderBox.containingBlock()->writingMode();
    if (renderBox.shouldApplyLayoutContainment())
        return { };

    if (writingMode.computedWritingMode() != renderBox.writingMode().computedWritingMode())
        return { };

    if (is<RenderIFrame>(renderBox)
        || is<RenderEmbeddedObject>(renderBox)
        || is<LegacyRenderSVGRoot>(renderBox)
        || is<RenderHTMLCanvas>(renderBox)
        || is<RenderViewTransitionCapture>(renderBox)
        || is<RenderTextControlMultiLine>(renderBox)
#if ENABLE(MODEL_ELEMENT)
        || is<RenderModel>(renderBox)
#endif
        || is<RenderSVGRoot>(renderBox))
        return { };

    auto borderBoxBottom = renderBox.height();
    auto marginBoxBottom = renderBox.marginBoxLogicalHeight(writingMode) - (writingMode.isHorizontal() ? renderBox.marginTop() : renderBox.marginRight());

    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderBox)) {
#if ENABLE(MULTI_REPRESENTATION_HEIC)
        if (renderImage->isMultiRepresentationHEIC())
            return roundToInt(marginBoxBottom) - LayoutUnit::fromFloatRound(renderImage->style().fontCascade().primaryFont()->metricsForMultiRepresentationHEIC().descent);
#endif
        return { };
    }

#if ENABLE(ATTACHMENT_ELEMENT)
    if (CheckedPtr rendererAttachment = dynamicDowncast<RenderAttachment>(renderBox)) {
        // Subtract margin top to preserve legacy behavior.
        auto marginBefore = renderBox.writingMode().isHorizontal() ? renderBox.marginTop() : renderBox.marginRight();
        if (CheckedPtr baselineElement = CheckedRef { rendererAttachment->attachmentElement() }->wideLayoutImageElement()) {
            if (auto* baselineElementRenderBox = baselineElement->renderBox()) {
                // This is the bottom of the image assuming it is vertically centered.
                return (borderBoxBottom + baselineElementRenderBox->height()) / 2 - marginBefore;
            }
            // Fallback to the bottom of the attachment if there is no image.
            return borderBoxBottom - marginBefore;
        }
        return rendererAttachment->theme().attachmentBaseline(*rendererAttachment) - marginBefore;
    }
#endif

    if (is<RenderButton>(renderBox)) {
        // We cannot rely on RenderFlexibleBox::baselinePosition() because of flexboxes have some special behavior
        // regarding baselines that shouldn't apply to buttons.
        return renderBox.lastLineBaseline();
    }

    if (is<RenderListBox>(renderBox)) {
        // FIXME: This hardcoded baselineAdjustment is what we used to do for the old
        // widget, but I'm not sure this is right for the new control.
        const int baselineAdjustment = 7;
        return roundToInt(marginBoxBottom) - baselineAdjustment;
    }

    if (CheckedPtr textControl = dynamicDowncast<RenderTextControlSingleLine>(renderBox)) {
        if (auto* innerTextRenderer = textControl->innerTextRenderer()) {
            auto baseline = LayoutUnit { };
            if (innerTextRenderer->inlineLayout())
                baseline = std::min<LayoutUnit>(innerTextRenderer->marginBoxLogicalHeight(writingMode), floorToInt(innerTextRenderer->inlineLayout()->lastLineBaseline()));
            else
                baseline = fontMetricsBasedBaseline(*innerTextRenderer);
            baseline = floorToInt(innerTextRenderer->logicalTop() + baseline);
            for (auto* ancestor = innerTextRenderer->containingBlock(); ancestor && ancestor != textControl; ancestor = ancestor->containingBlock())
                baseline = floorToInt(ancestor->logicalTop() + baseline);
            return baseline;
        }
        // input::-webkit-textfield-decoration-container { display: none }
        return { };
    }

    if (CheckedPtr fileUpload = dynamicDowncast<RenderFileUploadControl>(renderBox)) {
        if (auto* inlineLayout = fileUpload->inlineLayout())
            return std::min<LayoutUnit>(marginBoxBottom, floorToInt(inlineLayout->lastLineBaseline()));
        return { };
    }

    if (is<RenderSlider>(renderBox))
        return borderBoxBottom;

#if ENABLE(MATHML)
    if (is<RenderMathMLBlock>(renderBox))
        return renderBox.firstLineBaseline();
#endif

    if (is<RenderTable>(renderBox))
        return renderBox.firstLineBaseline();

    if (is<RenderMenuList>(renderBox) || is<RenderTextControlInnerContainer>(renderBox)) {
        // Both menu list and inner container are types of flex box but they behave slightly differently so always check them before checking for flex.
        return lastInflowBoxBaseline(downcast<RenderBlock>(renderBox));
    }

    if (is<RenderFlexibleBox>(renderBox) || is<RenderGrid>(renderBox))
        return renderBox.firstLineBaseline();

    if (renderBox.isFieldset()) {
        // Note that <fieldset> may simply be a flex/grid box (a non-RenderBlockFlow RenderBlock) and already handled above.
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox)) {
            // <fieldset> with no legend.
            if (CheckedPtr inlineLayout = blockFlow->inlineLayout())
                return floorToInt(inlineLayout->lastLineBaseline());
            return lastInflowBoxBaseline(*blockFlow);
        }
        return { };
    }

    if (renderBox.element() && renderBox.element()->isFormControlElement()) {
        // For "leaf" theme objects like checkbox, let the theme decide what the baseline position is.
        if (renderBox.style().hasUsedAppearance() && !renderBox.theme().isControlContainer(renderBox.style().usedAppearance()))
            return renderBox.theme().baselinePosition(renderBox);

        // Non-RenderTextControlSingleLine input type like input type color.
        if (CheckedPtr container = dynamicDowncast<RenderBox>(renderBox.firstInFlowChild())) {
            if (auto baseline = container->firstLineBaseline())
                return container->logicalTop() + *baseline;
        }
        // e.g. leaf theme objects with no appearance (none) and empty content (e.g. before pseudo and content: "").
        return { };
    }

    if (RefPtr element = renderBox.element(); element && element->shadowHost() && element->shadowHost()->isFormControlElement()) {
        // Inside RenderTextControl's shadow DOM (e.g. strong-password text)
        auto lastBaseline = std::optional<LayoutUnit> { };
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox)) {
            if (auto* inlineLayout = blockFlow->inlineLayout())
                lastBaseline = floorToInt(inlineLayout->lastLineBaseline());
        }
        if (!lastBaseline)
            lastBaseline = (fontMetricsBasedBaseline(renderBox) + (writingMode.isHorizontal() ? renderBox.borderTop() + renderBox.paddingTop() : renderBox.borderRight() + renderBox.paddingRight())).toInt();
        return std::min(marginBoxBottom, *lastBaseline);
    }

    if (CheckedPtr deprecatedFlexBox = dynamicDowncast<RenderDeprecatedFlexibleBox>(renderBox)) {
        // Historically, we did this check for all baselines. But we can't
        // remove this code from deprecated flexbox, because it effectively
        // breaks -webkit-line-clamp, which is used in the wild -- we would
        // calculate the baseline as if -webkit-line-clamp wasn't used.
        // For simplicity, we use this for all uses of deprecated flexbox.
        auto bottomOfContent = deprecatedFlexBox->borderBefore() + deprecatedFlexBox->paddingBefore() + deprecatedFlexBox->contentBoxLogicalHeight();
        auto baseline = lastInflowBoxBaseline(*deprecatedFlexBox);
        if (baseline && *baseline <= bottomOfContent)
            return *baseline;
        return { };
    }

    if (CheckedPtr listMarker = dynamicDowncast<RenderListMarker>(renderBox)) {
        if (CheckedPtr listItem = listMarker->listItem(); listItem && !listMarker->isImage())
            return fontMetricsBasedBaseline(*listMarker).toInt();
        return { };
    }

    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderBox)) {
        if (shouldUseMarginBoxAsBaseline(*blockFlow) || blockFlow->style().overflowY() != Overflow::Visible)
            return marginBoxBottom;

        // Note that here we only take the left and bottom into consideration. Our caller takes the right and top into consideration.
        if (!blockFlow->childrenInline())
            return lastInflowBoxBaseline(*blockFlow);

        if (!blockFlow->hasLines()) {
            ASSERT(blockFlow->hasLineIfEmpty());
            return (fontMetricsBasedBaseline(*blockFlow) + (writingMode.isHorizontal() ? blockFlow->borderTop() + blockFlow->paddingTop() : blockFlow->borderRight() + blockFlow->paddingRight())).toInt();
        }

        if (auto* inlineLayout = blockFlow->inlineLayout())
            return floorToInt(inlineLayout->lastLineBaseline());

        if (blockFlow->svgTextLayout()) {
            auto& style = blockFlow->firstLineStyle();
            // LegacyInlineFlowBox::placeBoxesInBlockDirection will flip lines in case of verticalLR mode, so we can assume verticalRL for now.
            return LayoutUnit(blockFlow->legacyRootBox()->logicalTop() + style.metricsOfPrimaryFont().intAscent(blockFlow->legacyRootBox()->baselineType()));
        }

        ASSERT_NOT_REACHED();
        return marginBoxBottom;
    }

    ASSERT_NOT_REACHED();
    return { };
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

    if (hasNonSyntheticBaseline()) {
        auto baselinePosition = [&]() -> LayoutUnit {
            auto rootWritingMode = renderBox.containingBlock()->writingMode();
            auto marginBefore = renderBox.writingMode().isHorizontal() ? renderBox.marginTop() : renderBox.marginRight();

            if (auto baseline = baselineForBox(renderBox)) {
                *baseline = rootWritingMode.isLineInverted() ? renderBox.logicalHeight() - *baseline : *baseline;
                return marginBefore + *baseline;
            }

            auto marginBoxLogicalHeight = renderBox.marginBoxLogicalHeight(rootWritingMode);
            auto isWritingModeRoot = rootWritingMode.computedWritingMode() != renderBox.writingMode().computedWritingMode();

            if (renderBox.isFieldset()) {
                if (isWritingModeRoot || renderBox.shouldApplyLayoutContainment())
                    return marginBoxLogicalHeight;
                return roundToInt(marginBoxLogicalHeight);
            }

            if (is<RenderButton>(renderBox)) {
                auto contentBoxBottom = rootWritingMode.isHorizontal() ? renderBox.borderTop() + renderBox.paddingTop() + renderBox.contentBoxHeight() : renderBox.borderRight() + renderBox.paddingRight() + renderBox.contentBoxWidth();
                return marginBefore + roundToInt(contentBoxBottom);
            }

            return roundToInt(marginBoxLogicalHeight);
        };
        const_cast<Layout::ElementBox&>(*renderBox.layoutBox()).setBaselineForIntegration(baselinePosition());
    }
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
    if (!renderBox.isOutOfFlowPositioned()) {
        boxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(renderBox));
        boxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(renderBox));
    }
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

    if (rootLayoutBox().establishesGridFormattingContext()) {
        for (auto* gridItemOrOutOfFlowPositionedChild = rootLayoutBox().firstChild(); gridItemOrOutOfFlowPositionedChild; gridItemOrOutOfFlowPositionedChild = gridItemOrOutOfFlowPositionedChild->nextSibling())
            updateBoxGeometry(downcast<RenderElement>(*gridItemOrOutOfFlowPositionedChild->rendererForIntegration()), availableLogicalWidth, intrinsicWidthMode);
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

