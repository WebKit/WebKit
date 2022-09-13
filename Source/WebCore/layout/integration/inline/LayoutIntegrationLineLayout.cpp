/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationLineLayout.h"

#include "DeprecatedGlobalSettings.h"
#include "EventRegion.h"
#include "FloatingState.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineDamage.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "InlineInvalidation.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationInlineContentBuilder.h"
#include "LayoutIntegrationInlineContentPainter.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutListMarkerBox.h"
#include "LayoutReplacedBox.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderAttachment.h"
#include "RenderBlockFlow.h"
#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTextControlMultiLine.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {
namespace LayoutIntegration {

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_boxTree(flow)
    , m_layoutState(flow.document(), rootLayoutBox(), Layout::LayoutState::FormattingContextIntegrationType::Inline)
    , m_inlineFormattingState(m_layoutState.ensureInlineFormattingState(rootLayoutBox()))
{
    m_layoutState.setIsIntegratedRootBoxFirstChild(flow.parent()->firstChild() == &flow);
}

LineLayout::~LineLayout()
{
    clearInlineContent();
}

RenderBlockFlow* LineLayout::blockContainer(RenderObject& renderer)
{
    // FIXME: These fake renderers have their parent set but are not actually in the tree.
    if (renderer.isReplica() || renderer.isRenderScrollbarPart())
        return nullptr;
    
    for (auto* parent = renderer.parent(); parent; parent = parent->parent()) {
        if (!parent->childrenInline())
            return nullptr;
        if (is<RenderBlockFlow>(*parent))
            return downcast<RenderBlockFlow>(parent);
    }

    return nullptr;
}

LineLayout* LineLayout::containing(RenderObject& renderer)
{
    if (!renderer.isInline())
        return nullptr;

    if (auto* container = blockContainer(renderer))
        return container->modernLineLayout();

    return nullptr;
}

const LineLayout* LineLayout::containing(const RenderObject& renderer)
{
    return containing(const_cast<RenderObject&>(renderer));
}

bool LineLayout::isEnabled()
{
    return DeprecatedGlobalSettings::inlineFormattingContextIntegrationEnabled();
}

bool LineLayout::canUseFor(const RenderBlockFlow& flow)
{
    if (!isEnabled())
        return false;

    return canUseForLineLayout(flow);
}

bool LineLayout::canUseForAfterStyleChange(const RenderBlockFlow& flow, StyleDifference diff)
{
    ASSERT(isEnabled());
    return canUseForLineLayoutAfterStyleChange(flow, diff);
}

bool LineLayout::canUseForAfterInlineBoxStyleChange(const RenderInline& inlineBox, StyleDifference diff)
{
    ASSERT(isEnabled());
    return canUseForLineLayoutAfterInlineBoxStyleChange(inlineBox, diff);
}

bool LineLayout::shouldSwitchToLegacyOnInvalidation() const
{
    // FIXME: Support partial invalidation in LFC.
    // This avoids O(n^2) when lots of boxes are being added dynamically while forcing layouts between.
    constexpr size_t maximimumBoxTreeSizeForInvalidation = 128;
    return m_boxTree.boxCount() > maximimumBoxTreeSizeForInvalidation;
}

void LineLayout::updateReplacedDimensions(const RenderBox& replaced)
{
    updateLayoutBoxDimensions(replaced);
}

void LineLayout::updateInlineBlockDimensions(const RenderBlock& inlineBlock)
{
    updateLayoutBoxDimensions(inlineBlock);
}

void LineLayout::updateInlineTableDimensions(const RenderTable& inlineTable)
{
    updateLayoutBoxDimensions(inlineTable);
}

void LineLayout::updateListItemDimensions(const RenderListItem& listItem)
{
    updateLayoutBoxDimensions(listItem);
}

void LineLayout::updateListMarkerDimensions(const RenderListMarker& listMarker)
{
    updateLayoutBoxDimensions(listMarker);

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(listMarker);
    if (downcast<Layout::ListMarkerBox>(layoutBox).isOutside()) {
        auto& rootGeometry = m_layoutState.geometryForRootBox();
        auto& listMarkerGeometry = m_inlineFormattingState.boxGeometry(layoutBox);
        auto horizontalMargin = listMarkerGeometry.horizontalMargin();
        auto outsideOffset = rootGeometry.paddingStart().value_or(0_lu) + rootGeometry.borderStart();
        listMarkerGeometry.setHorizontalMargin({ horizontalMargin.start - outsideOffset, horizontalMargin.end + outsideOffset });  
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

static inline Layout::BoxGeometry::VerticalMargin verticalLogicalMargin(const RenderBoxModelObject& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return { renderer.marginTop(), renderer.marginBottom() };
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        return { renderer.marginRight(), renderer.marginLeft() };
    default:
        ASSERT_NOT_REACHED();
        return { renderer.marginTop(), renderer.marginBottom() };
    }
}

static inline Layout::Edges logicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, WritingMode writingMode, bool retainBorderStart = true, bool retainBorderEnd = true)
{
    auto borderLeft = renderer.borderLeft();
    auto borderRight = renderer.borderRight();
    auto borderTop = renderer.borderTop();
    auto borderBottom = renderer.borderBottom();

    if (writingMode == WritingMode::TopToBottom) {
        if (isLeftToRightInlineDirection)
            return { { retainBorderStart ? borderLeft : 0_lu, retainBorderEnd ? borderRight : 0_lu }, { borderTop, borderBottom } };
        return { { retainBorderStart ? borderRight : 0_lu, retainBorderEnd ? borderLeft : 0_lu }, { borderTop, borderBottom } };
    }

    auto borderLogicalLeft = retainBorderStart ? isLeftToRightInlineDirection ? borderTop : borderBottom : 0_lu;
    auto borderLogicalRight = retainBorderEnd ? isLeftToRightInlineDirection ? borderBottom : borderTop : 0_lu;
    auto borderLogicalTop = writingMode == WritingMode::LeftToRight ? borderLeft : borderRight;
    auto borderLogicalBottom = writingMode == WritingMode::LeftToRight ? borderRight : borderLeft;
    return { { borderLogicalLeft, borderLogicalRight }, { borderLogicalTop, borderLogicalBottom } };
}

static inline Layout::Edges logicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, WritingMode writingMode, bool retainPaddingStart = true, bool retainPaddingEnd = true)
{
    auto paddingLeft = renderer.paddingLeft();
    auto paddingRight = renderer.paddingRight();
    auto paddingTop = renderer.paddingTop();
    auto paddingBottom = renderer.paddingBottom();

    if (writingMode == WritingMode::TopToBottom) {
        if (isLeftToRightInlineDirection)
            return { { retainPaddingStart ? paddingLeft : 0_lu, retainPaddingEnd ? paddingRight : 0_lu }, { paddingTop, paddingBottom } };
        return { { retainPaddingStart ? paddingRight : 0_lu, retainPaddingEnd ? paddingLeft : 0_lu }, { paddingTop, paddingBottom } };
    }

    auto paddingLogicalLeft = retainPaddingStart ? isLeftToRightInlineDirection ? paddingTop : paddingBottom : 0_lu;
    auto paddingLogicalRight = retainPaddingEnd ? isLeftToRightInlineDirection ? paddingBottom : paddingTop : 0_lu;
    auto paddingLogicalTop = writingMode == WritingMode::LeftToRight ? paddingLeft : paddingRight;
    auto paddingLogicalBottom = writingMode == WritingMode::LeftToRight ? paddingRight : paddingLeft;
    return { { paddingLogicalLeft, paddingLogicalRight }, { paddingLogicalTop, paddingLogicalBottom } };
}

static inline LayoutSize scrollbarLogicalSize(const RenderBox& renderer)
{
    // Scrollbars eat into the padding box area. They never stretch the border box but they may shrink the padding box.
    // In legacy render tree, RenderBox::contentWidth/contentHeight values are adjusted to accomodate the scrollbar width/height.
    // e.g. <div style="width: 10px; overflow: scroll;">content</div>, RenderBox::contentWidth() won't be returning the value of 10px but instead 0px (10px - 15px).
    auto horizontalSpaceReservedForScrollbar = std::max(0_lu, renderer.paddingBoxRectIncludingScrollbar().width() - renderer.paddingBoxWidth());
    auto verticalSpaceReservedForScrollbar = std::max(0_lu, renderer.paddingBoxRectIncludingScrollbar().height() - renderer.paddingBoxHeight());
    return { horizontalSpaceReservedForScrollbar, verticalSpaceReservedForScrollbar };
}

void LineLayout::updateLayoutBoxDimensions(const RenderBox& replacedOrInlineBlock)
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(replacedOrInlineBlock);
    // Internally both replaced and inline-box content use replaced boxes.
    auto& replacedBox = downcast<Layout::ReplacedBox>(layoutBox);

    auto& replacedBoxGeometry = m_layoutState.ensureGeometryForBox(replacedBox);
    auto scrollbarSize = scrollbarLogicalSize(replacedOrInlineBlock);
    replacedBoxGeometry.setHorizontalSpaceForScrollbar(scrollbarSize.width());
    replacedBoxGeometry.setVerticalSpaceForScrollbar(scrollbarSize.height());

    replacedBoxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(replacedOrInlineBlock));
    replacedBoxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(replacedOrInlineBlock));

    auto isLeftToRightInlineDirection = replacedOrInlineBlock.parent()->style().isLeftToRightDirection();
    auto writingMode = replacedOrInlineBlock.parent()->style().writingMode();

    replacedBoxGeometry.setVerticalMargin(verticalLogicalMargin(replacedOrInlineBlock, writingMode));
    replacedBoxGeometry.setHorizontalMargin(horizontalLogicalMargin(replacedOrInlineBlock, isLeftToRightInlineDirection, writingMode == WritingMode::TopToBottom));
    replacedBoxGeometry.setBorder(logicalBorder(replacedOrInlineBlock, isLeftToRightInlineDirection, writingMode));
    replacedBoxGeometry.setPadding(logicalPadding(replacedOrInlineBlock, isLeftToRightInlineDirection, writingMode));

    auto hasNonSyntheticBaseline = [&] {
        if (is<RenderListMarker>(replacedOrInlineBlock))
            return !downcast<RenderListMarker>(replacedOrInlineBlock).isImage();
        if (is<RenderReplaced>(replacedOrInlineBlock)
            || is<RenderListBox>(replacedOrInlineBlock)
            || is<RenderSlider>(replacedOrInlineBlock)
            || is<RenderTextControlMultiLine>(replacedOrInlineBlock)
            || is<RenderTable>(replacedOrInlineBlock)
#if ENABLE(ATTACHMENT_ELEMENT)
            || is<RenderAttachment>(replacedOrInlineBlock)
#endif
            || is<RenderButton>(replacedOrInlineBlock)) {
            // These are special RenderBlock renderers that override the default baseline position behavior of the inline block box.
            return true;
        }
        auto& blockFlow = downcast<RenderBlockFlow>(replacedOrInlineBlock);
        auto hasAppareance = blockFlow.style().hasEffectiveAppearance() && !blockFlow.theme().isControlContainer(blockFlow.style().effectiveAppearance());
        return hasAppareance || !blockFlow.childrenInline() || blockFlow.hasLines() || blockFlow.hasLineIfEmpty();
    }();
    if (hasNonSyntheticBaseline) {
        auto baseline = replacedOrInlineBlock.baselinePosition(AlphabeticBaseline, false /* firstLine */, writingMode == WritingMode::TopToBottom ? HorizontalLine : VerticalLine, PositionOnContainingLine);
        if (is<RenderListMarker>(replacedOrInlineBlock)) {
            ASSERT(!downcast<RenderListMarker>(replacedOrInlineBlock).isImage());
            baseline = replacedOrInlineBlock.style().metricsOfPrimaryFont().ascent();
        }
        replacedBox.setBaseline(roundToInt(baseline));
    }
}

void LineLayout::updateLineBreakBoxDimensions(const RenderLineBreak& lineBreakBox)
{
    // This is just a box geometry reset (see InlineFormattingContext::layoutInFlowContent).
    auto& boxGeometry = m_layoutState.ensureGeometryForBox(m_boxTree.layoutBoxForRenderer(lineBreakBox));

    boxGeometry.setHorizontalMargin({ });
    boxGeometry.setBorder({ });
    boxGeometry.setPadding({ });
    boxGeometry.setContentBoxWidth({ });
    boxGeometry.setVerticalMargin({ });
}

void LineLayout::updateInlineBoxDimensions(const RenderInline& renderInline)
{
    auto& boxGeometry = m_layoutState.ensureGeometryForBox(m_boxTree.layoutBoxForRenderer(renderInline));

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();

    boxGeometry.setVerticalMargin({ });
    auto isLeftToRightInlineDirection = renderInline.style().isLeftToRightDirection();
    auto writingMode = renderInline.style().writingMode();

    boxGeometry.setHorizontalMargin(horizontalLogicalMargin(renderInline, isLeftToRightInlineDirection, writingMode == WritingMode::TopToBottom, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setBorder(logicalBorder(renderInline, isLeftToRightInlineDirection, writingMode, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setPadding(logicalPadding(renderInline, isLeftToRightInlineDirection, writingMode, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
}

void LineLayout::updateStyle(const RenderBoxModelObject& renderer, const RenderStyle& oldStyle)
{
    auto invalidation = Layout::InlineInvalidation { ensureLineDamage() };
    invalidation.styleChanged(m_boxTree.layoutBoxForRenderer(renderer), oldStyle);

    m_boxTree.updateStyle(renderer);
}

void LineLayout::updateOverflow()
{
    auto inlineContentBuilder = InlineContentBuilder { flow(), m_boxTree };
    inlineContentBuilder.updateLineOverflow(m_inlineFormattingState, *m_inlineContent);
}

std::pair<LayoutUnit, LayoutUnit> LineLayout::computeIntrinsicWidthConstraints()
{
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), m_inlineFormattingState, nullptr };
    auto constraints = inlineFormattingContext.computedIntrinsicWidthConstraintsForIntegration();

    return { constraints.minimum, constraints.maximum };
}

void LineLayout::layout()
{
    auto& rootLayoutBox = this->rootLayoutBox();
    if (!rootLayoutBox.hasInFlowOrFloatingChild())
        return;

    prepareLayoutState();
    prepareFloatingState();

    // FIXME: Do not clear the lines and boxes here unconditionally, but consult with the damage object instead.
    clearInlineContent();

    auto& rootGeometry = m_layoutState.geometryForBox(rootLayoutBox);
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox, m_inlineFormattingState, m_lineDamage.get() };

    auto horizontalConstraints = Layout::HorizontalConstraints { rootGeometry.contentBoxLeft(), rootGeometry.contentBoxWidth() };

    inlineFormattingContext.layoutInFlowContentForIntegration({ horizontalConstraints, rootGeometry.contentBoxTop() });

    constructContent();

    m_lineDamage = { };
}

void LineLayout::constructContent()
{
    auto inlineContentBuilder = InlineContentBuilder { flow(), m_boxTree };
    inlineContentBuilder.build(m_inlineFormattingState, ensureInlineContent());
    ASSERT(m_inlineContent);

    auto& rootGeometry = m_layoutState.geometryForRootBox();
    auto isLeftToRightInlineDirection = rootLayoutBox().style().isLeftToRightDirection();
    auto& boxAndRendererList = m_boxTree.boxAndRendererList();
    for (auto& boxAndRenderer : boxAndRendererList) {
        auto& layoutBox = boxAndRenderer.box.get();
        if (!layoutBox.isReplacedBox() && !layoutBox.isFloatingPositioned())
            continue;

        auto& renderer = downcast<RenderBox>(*boxAndRenderer.renderer);
        auto& boxGeometry = m_inlineFormattingState.boxGeometry(layoutBox);
        auto visualBorderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(boxGeometry) };

        if (layoutBox.isOutOfFlowPositioned()) {
            auto& layer = *renderer.layer();
            layer.setStaticBlockPosition(visualBorderBoxRect.y());
            layer.setStaticInlinePosition(visualBorderBoxRect.x());
            continue;
        }

        if (layoutBox.isFloatingPositioned()) {
            auto& floatingObject = flow().insertFloatingObjectForIFC(renderer);
            auto marginBoxRect = LayoutRect { Layout::BoxGeometry::marginBoxRect(boxGeometry) };
            auto marginLeft = isLeftToRightInlineDirection ? boxGeometry.marginStart() : boxGeometry.marginEnd();
            auto marginTop = boxGeometry.marginBefore();
            if (!isLeftToRightInlineDirection) {
                // FIXME: This is temporary until after the floating state can mix and match floats coming from different inline directions.
                // Computed float geometry in visual coords will make this code redundant (and that's why this flip should not go to the display builders).
                marginBoxRect.setX(rootGeometry.borderBoxWidth() - marginBoxRect.maxX());
                visualBorderBoxRect.setX(marginBoxRect.x() + marginLeft);
            }
            floatingObject.setFrameRect(marginBoxRect);
            floatingObject.setMarginOffset({ marginLeft, marginTop });
            floatingObject.setIsPlaced(true);
        }

        renderer.setLocation(visualBorderBoxRect.location());
    }

    m_inlineContent->clearGapAfterLastLine = m_inlineFormattingState.clearGapAfterLastLine();
    m_inlineContent->shrinkToFit();
    m_inlineFormattingState.shrinkToFit();
}

void LineLayout::updateFormattingRootGeometryAndInvalidate()
{
    auto& flow = this->flow();

    auto updateGeometry = [&](auto& root) {
        auto isLeftToRightInlineDirection = flow.style().isLeftToRightDirection();
        auto writingMode = flow.style().writingMode();
        auto scrollbarSize = scrollbarLogicalSize(flow);

        root.setContentBoxWidth(WebCore::isHorizontalWritingMode(writingMode) ? flow.contentWidth() : flow.contentHeight());
        root.setPadding(logicalPadding(flow, isLeftToRightInlineDirection, writingMode));
        root.setBorder(logicalBorder(flow, isLeftToRightInlineDirection, writingMode));
        root.setHorizontalSpaceForScrollbar(scrollbarSize.width());
        root.setVerticalSpaceForScrollbar(scrollbarSize.height());
        root.setHorizontalMargin({ });
        root.setVerticalMargin({ });
    };
    auto& rootLayoutBox = this->rootLayoutBox();
    if (!m_layoutState.hasBoxGeometry(rootLayoutBox))
        return updateGeometry(m_layoutState.ensureGeometryForBox(rootLayoutBox));

    auto& rootGeometry = m_layoutState.geometryForRootBox();
    auto newLogicalWidth = flow.contentLogicalWidth();
    if (newLogicalWidth != rootGeometry.contentBoxWidth())
        Layout::InlineInvalidation(ensureLineDamage()).horizontalConstraintChanged();
    updateGeometry(rootGeometry);
}

void LineLayout::prepareLayoutState()
{
    m_layoutState.setViewportSize(flow().frame().view()->size());
}

void LineLayout::prepareFloatingState()
{
    auto& floatingState = m_inlineFormattingState.floatingState();
    floatingState.clear();

    if (!flow().containsFloats())
        return;

    if (flow().containingBlock())
        floatingState.setIsLeftToRightDirection(flow().containingBlock()->style().isLeftToRightDirection());
    for (auto& floatingObject : *flow().floatingObjectSet()) {
        auto& visualRect = floatingObject->frameRect();
        auto position = floatingObject->type() == FloatingObject::FloatRight
            ? Layout::FloatingState::FloatItem::Position::Right
            : Layout::FloatingState::FloatItem::Position::Left;
        auto boxGeometry = Layout::BoxGeometry { };
        // FIXME: We are flooring here for legacy compatibility.
        //        See FloatingObjects::intervalForFloatingObject and RenderBlockFlow::clearFloats.
        auto logicalTop = visualRect.y().floor();
        auto logicalHeight = visualRect.maxY().floor() - logicalTop;
        auto logicalRect = flow().style().isHorizontalWritingMode() ? LayoutRect(visualRect.x(), logicalTop, visualRect.width(), logicalHeight)
            : LayoutRect(logicalTop, visualRect.x(), logicalHeight, visualRect.width());

        boxGeometry.setLogicalTopLeft(logicalRect.location());
        boxGeometry.setContentBoxWidth(logicalRect.width());
        boxGeometry.setContentBoxHeight(logicalRect.height());
        boxGeometry.setBorder({ });
        boxGeometry.setPadding({ });
        boxGeometry.setHorizontalMargin({ });
        boxGeometry.setVerticalMargin({ });
        floatingState.append({ position, boxGeometry });
    }
}

LayoutUnit LineLayout::contentLogicalHeight() const
{
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->lines;
    auto flippedContentHeightForWritingMode = rootLayoutBox().style().isHorizontalWritingMode()
        ? lines.last().lineBoxBottom() - lines.first().lineBoxTop()
        : lines.last().lineBoxRight() - lines.first().lineBoxLeft();
    return LayoutUnit { flippedContentHeightForWritingMode + m_inlineContent->clearGapAfterLastLine };
}

size_t LineLayout::lineCount() const
{
    if (!m_inlineContent)
        return 0;
    if (!m_inlineContent->hasContent())
        return 0;

    return m_inlineContent->lines.size();
}

bool LineLayout::hasVisualOverflow() const
{
    return m_inlineContent && m_inlineContent->hasVisualOverflow();
}

LayoutUnit LineLayout::firstLinePhysicalBaseline() const
{
    if (!m_inlineContent || m_inlineContent->lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& firstLine = m_inlineContent->lines.first();
    switch (rootLayoutBox().style().writingMode()) {
    case WritingMode::TopToBottom:
        return LayoutUnit { firstLine.lineBoxTop() + firstLine.baseline() };
    case WritingMode::LeftToRight:
        return LayoutUnit { firstLine.lineBoxLeft() + (firstLine.lineBoxWidth() - firstLine.baseline()) };
    case WritingMode::RightToLeft:
        return LayoutUnit { firstLine.lineBoxLeft() + firstLine.baseline() };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit LineLayout::lastLineLogicalBaseline() const
{
    if (!m_inlineContent || m_inlineContent->lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& lastLine = m_inlineContent->lines.last();
    switch (rootLayoutBox().style().writingMode()) {
    case WritingMode::TopToBottom:
        return LayoutUnit { lastLine.lineBoxTop() + lastLine.baseline() };
    case WritingMode::LeftToRight: {
        // FIXME: We should set the computed height on the root's box geometry (in RenderBlockFlow) so that
        // we could call m_layoutState.geometryForRootBox().borderBoxHeight() instead.

        // Line is always visual coordinates while logicalHeight is not (i.e. this translate to "box visual width" - "line visual right")
        auto lineLogicalTop = flow().logicalHeight() - lastLine.lineBoxRight();
        return LayoutUnit { lineLogicalTop + lastLine.baseline() };
    }
    case WritingMode::RightToLeft:
        return LayoutUnit { lastLine.lineBoxLeft() + lastLine.baseline() };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

void LineLayout::adjustForPagination()
{
    if (!m_inlineContent)
        return;

    auto paginedInlineContent = adjustLinePositionsForPagination(*m_inlineContent, flow());
    if (!paginedInlineContent) {
        m_isPaginatedContent = false;
        return;
    }
    m_isPaginatedContent = true;
    m_inlineContent = WTFMove(paginedInlineContent);
}

void LineLayout::collectOverflow()
{
    if (!m_inlineContent)
        return;

    for (auto& line : m_inlineContent->lines) {
        flow().addLayoutOverflow(Layout::toLayoutRect(line.scrollableOverflow()));
        if (!flow().hasNonVisibleOverflow())
            flow().addVisualOverflow(Layout::toLayoutRect(line.inkOverflow()));
    }
}

InlineContent& LineLayout::ensureInlineContent()
{
    if (!m_inlineContent)
        m_inlineContent = makeUnique<InlineContent>(*this);
    return *m_inlineContent;
}

InlineIterator::TextBoxIterator LineLayout::textBoxesFor(const RenderText& renderText) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderText);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::textBoxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::LeafBoxIterator LineLayout::boxFor(const RenderElement& renderElement) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderElement);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::boxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::InlineBoxIterator LineLayout::firstInlineBoxFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);
    auto* box = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!box)
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, *box);
}

InlineIterator::InlineBoxIterator LineLayout::firstRootInlineBox() const
{
    if (!m_inlineContent || !m_inlineContent->hasContent())
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, m_inlineContent->boxes[0]);
}

InlineIterator::LineBoxIterator LineLayout::firstLineBox() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineBoxIteratorModernPath(*m_inlineContent, 0) };
}

InlineIterator::LineBoxIterator LineLayout::lastLineBox() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineBoxIteratorModernPath(*m_inlineContent, m_inlineContent->lines.isEmpty() ? 0 : m_inlineContent->lines.size() - 1) };
}

LayoutRect LineLayout::firstInlineBoxRect(const RenderInline& renderInline) const
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);
    auto* firstBox = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!firstBox)
        return { };

    // FIXME: We should be able to flip the display boxes soon after the root block
    // is finished sizing in one go.
    auto firstBoxRect = Layout::toLayoutRect(firstBox->visualRectIgnoringBlockDirection());
    switch (rootLayoutBox().style().writingMode()) {
    case WritingMode::TopToBottom:
    case WritingMode::LeftToRight:
        return firstBoxRect;
    case WritingMode::RightToLeft:
        firstBoxRect.setX(flow().width() - firstBoxRect.maxX());
        return firstBoxRect;
    default:
        ASSERT_NOT_REACHED();
        return firstBoxRect;
    }
}

LayoutRect LineLayout::enclosingBorderBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    // FIXME: This keeps the existing output.
    if (!m_inlineContent->hasContent())
        return { };

    return Layout::BoxGeometry::borderBoxRect(m_inlineFormattingState.boxGeometry(m_boxTree.layoutBoxForRenderer(renderInline)));
}

LayoutRect LineLayout::visualOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    LayoutRect result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.unite(Layout::toLayoutRect(inlineBox.inkOverflow()));
    });

    return result;
}

Vector<FloatRect> LineLayout::collectInlineBoxRects(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    Vector<FloatRect> result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.append(inlineBox.visualRectIgnoringBlockDirection());
    });

    return result;
}

const RenderObject& LineLayout::rendererForLayoutBox(const Layout::Box& layoutBox) const
{
    return m_boxTree.rendererForLayoutBox(layoutBox);
}

const Layout::ContainerBox& LineLayout::rootLayoutBox() const
{
    return m_boxTree.rootLayoutBox();
}

Layout::ContainerBox& LineLayout::rootLayoutBox()
{
    return m_boxTree.rootLayoutBox();
}

static LayoutPoint flippedContentOffsetIfNeeded(const RenderBlockFlow& root, const RenderBox& childRenderer, LayoutPoint contentOffset)
{
    if (root.style().isFlippedBlocksWritingMode())
        return root.flipForWritingModeForChild(childRenderer, contentOffset);
    return contentOffset;
}

static LayoutRect flippedRectForWritingMode(const RenderBlockFlow& root, const FloatRect& rect)
{
    auto flippedRect = LayoutRect { rect };
    root.flipForWritingMode(flippedRect);
    return flippedRect;
}

void LineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const RenderInline* layerRenderer)
{
    if (!m_inlineContent)
        return;

    auto shouldPaintForPhase = [&] {
        switch (paintInfo.phase) {
        case PaintPhase::Foreground:
        case PaintPhase::EventRegion:
        case PaintPhase::TextClip:
        case PaintPhase::Mask:
        case PaintPhase::Selection:
        case PaintPhase::Outline:
        case PaintPhase::ChildOutlines:
        case PaintPhase::SelfOutline:
            return true;
        default:
            return false;
        }
    };
    if (!shouldPaintForPhase())
        return;

    InlineContentPainter { paintInfo, paintOffset, layerRenderer, *m_inlineContent, m_boxTree }.paint();
}

bool LineLayout::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction, const RenderInline* layerRenderer)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!m_inlineContent)
        return false;

    auto hitTestBoundingBox = locationInContainer.boundingBox();
    hitTestBoundingBox.moveBy(-accumulatedOffset);
    auto boxRange = m_inlineContent->boxesForRect(hitTestBoundingBox);

    LayerPaintScope layerPaintScope(m_boxTree, layerRenderer);

    for (auto& box : makeReversedRange(boxRange)) {
        if (!box.isVisible())
            continue;

        auto& renderer = m_boxTree.rendererForLayoutBox(box.layoutBox());

        if (!layerPaintScope.includes(box))
            continue;

        if (box.isAtomicInlineLevelBox()) {
            if (renderer.hitTest(request, result, locationInContainer, flippedContentOffsetIfNeeded(flow(), downcast<RenderBox>(renderer), accumulatedOffset)))
                return true;
            continue;
        }

        auto& currentLine = m_inlineContent->lines[box.lineIndex()];
        auto boxRect = flippedRectForWritingMode(flow(), InlineDisplay::Box::visibleRectIgnoringBlockDirection(box, currentLine.visibleRectIgnoringBlockDirection()));
        boxRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(boxRect))
            continue;

        auto& elementRenderer = is<RenderElement>(renderer) ? downcast<RenderElement>(renderer) : *renderer.parent();
        if (!elementRenderer.visibleToHitTesting(request))
            continue;
        
        renderer.updateHitTestResult(result, flow().flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, boxRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

void LineLayout::releaseCaches(RenderView& view)
{
    if (!isEnabled())
        return;

    for (auto& renderer : descendantsOfType<RenderBlockFlow>(view)) {
        if (auto* lineLayout = renderer.modernLineLayout())
            lineLayout->releaseCaches();
    }
}

void LineLayout::releaseCaches()
{
    m_inlineFormattingState.inlineItems().clear();
    if (m_inlineContent)
        m_inlineContent->releaseCaches();
}

void LineLayout::clearInlineContent()
{
    if (!m_inlineContent)
        return;
    m_inlineContent = nullptr;
}

Layout::InlineDamage& LineLayout::ensureLineDamage()
{
    if (!m_lineDamage)
        m_lineDamage = makeUnique<Layout::InlineDamage>();
    return *m_lineDamage;
}

#if ENABLE(TREE_DEBUGGING)
void LineLayout::outputLineTree(WTF::TextStream& stream, size_t depth) const
{
    showInlineContent(stream, *m_inlineContent, depth);
}
#endif

}
}

