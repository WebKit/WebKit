/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationFlexLayout.h"

#include "FlexFormattingConstraints.h"
#include "FlexFormattingContext.h"
#include "FormattingContextBoxIterator.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderView.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace LayoutIntegration {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FlexLayout);

FlexLayout::FlexLayout(RenderFlexibleBox& flexBoxRenderer)
    : m_boxTree(flexBoxRenderer)
    , m_layoutState(flexBoxRenderer.view().layoutState())
{
}

FlexLayout::~FlexLayout()
{
}

static inline Layout::ConstraintsForFlexContent constraintsForFlexContent(const Layout::ElementBox& flexContainer)
{
    auto& flexContainerRenderer = downcast<RenderFlexibleBox>(*flexContainer.rendererForIntegration());
    auto& flexBoxStyle = flexContainer.style();
    auto boxSizingIsContentBox = flexBoxStyle.boxSizing() == BoxSizing::ContentBox;
    auto availableLogicalWidth = flexContainerRenderer.contentBoxRect().width();
    // FIXME: Use root's BoxGeometry which first needs to stop flipping for the formatting context.
    auto horizontallMarginBorderAndPadding = flexContainerRenderer.marginAndBorderAndPaddingStart() + flexContainerRenderer.marginAndBorderAndPaddingEnd();
    auto verticalMarginBorderAndPadding = flexContainerRenderer.marginAndBorderAndPaddingBefore() + flexContainerRenderer.marginAndBorderAndPaddingAfter();

    auto widthValue = [&](auto& computedValue) -> std::optional<LayoutUnit> {
        if (computedValue.isFixed())
            return LayoutUnit { boxSizingIsContentBox ? computedValue.value() : computedValue.value() - horizontallMarginBorderAndPadding };

        if (computedValue.isPercent()) {
            auto value = valueForLength(computedValue, flexContainerRenderer.containingBlock()->logicalWidth());
            return LayoutUnit { boxSizingIsContentBox ? value : value - horizontallMarginBorderAndPadding };
        }
        return { };
    };

    auto heightValue = [&](auto& computedValue, bool callRendererForPercentValue = false) -> std::optional<LayoutUnit> {
        if (computedValue.isFixed())
            return LayoutUnit { boxSizingIsContentBox ? computedValue.value() : computedValue.value() - verticalMarginBorderAndPadding };

        if (computedValue.isPercent()) {
            if (callRendererForPercentValue)
                return flexContainerRenderer.computePercentageLogicalHeight(computedValue, RenderBoxModelObject::UpdatePercentageHeightDescendants::No);

            if (flexContainerRenderer.containingBlock()->style().logicalHeight().isFixed()) {
                auto value = valueForLength(computedValue, flexContainerRenderer.containingBlock()->style().height().value());
                return LayoutUnit { boxSizingIsContentBox ? value : value - verticalMarginBorderAndPadding };
            }
        }
        return { };
    };

    auto widthGeometry = [&]() -> Layout::ConstraintsForFlexContent::AxisGeometry {
        return { widthValue(flexBoxStyle.minWidth()), widthValue(flexBoxStyle.maxWidth()), availableLogicalWidth ? availableLogicalWidth : widthValue(flexBoxStyle.width()), flexContainerRenderer.contentBoxLocation().x() };
    };

    auto heightGeometry = [&]() -> Layout::ConstraintsForFlexContent::AxisGeometry {
        auto availableSize = heightValue(flexBoxStyle.height(), true);
        auto logicalMinHeight = heightValue(flexBoxStyle.minHeight()).value_or(0_lu);
        auto logicalMaxHeight = heightValue(flexBoxStyle.maxHeight());
        if (!availableSize || (logicalMaxHeight && *logicalMaxHeight < *availableSize))
            availableSize = logicalMaxHeight;

        return Layout::ConstraintsForFlexContent::AxisGeometry { logicalMinHeight, logicalMaxHeight, availableSize, flexContainerRenderer.contentBoxLocation().y() };
    };

    return Layout::FlexFormattingUtils::isMainAxisParallelWithInlineAxis(flexContainer) ? Layout::ConstraintsForFlexContent(widthGeometry(), heightGeometry(), false) : Layout::ConstraintsForFlexContent(heightGeometry(), widthGeometry(), false);
}

void FlexLayout::updateFormattingContexGeometries()
{
    auto boxGeometryUpdater = BoxGeometryUpdater { layoutState(), flexBox() };
    boxGeometryUpdater.setFormattingContextRootGeometry(flexBoxRenderer().containingBlock()->availableLogicalWidth());
    boxGeometryUpdater.setFormattingContextContentGeometry(layoutState().geometryForBox(flexBox()).contentBoxWidth(), { });
}

void FlexLayout::updateStyle(const RenderBlock&, const RenderStyle&)
{
}

std::pair<LayoutUnit, LayoutUnit> FlexLayout::computeIntrinsicWidthConstraints()
{
    auto flexFormattingContext = Layout::FlexFormattingContext { flexBox(), layoutState() };
    auto constraints = flexFormattingContext.computedIntrinsicWidthConstraints();

    return { constraints.minimum, constraints.maximum };
}

void FlexLayout::layout()
{
    Layout::FlexFormattingContext { flexBox(), layoutState() }.layout(constraintsForFlexContent(flexBox()));

    updateRenderers();

    auto relayoutFlexItems = [&] {
        // Flex items need to be laid out now with their final size (and through setOverridingLogicalWidth/Height)
        // Note that they may re-size themselves.
        auto flexContainerIsHorizontal = flexBox().writingMode().isHorizontal();
        for (auto& layoutBox : formattingContextBoxes(flexBox())) {
            auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
            auto isOrthogonal = flexContainerIsHorizontal != renderer.writingMode().isHorizontal();
            auto borderBox = Layout::BoxGeometry::borderBoxRect(layoutState().geometryForBox(layoutBox));

            renderer.setWidth(LayoutUnit { });
            renderer.setHeight(LayoutUnit { });
            // logical here means width and height constraints for the _content_ of the flex items not the flex items' own dimension inside the flex container.
            renderer.setOverridingLogicalWidth(isOrthogonal ? borderBox.height() : borderBox.width());
            renderer.setOverridingLogicalHeight(isOrthogonal ? borderBox.width() : borderBox.height());

            renderer.setChildNeedsLayout(MarkOnlyThis);
            renderer.layoutIfNeeded();
            renderer.clearOverridingContentSize();

            renderer.setWidth(flexContainerIsHorizontal ? borderBox.width() : borderBox.height());
            renderer.setHeight(flexContainerIsHorizontal ? borderBox.height() : borderBox.width());
        }
    };
    relayoutFlexItems();
}

void FlexLayout::updateRenderers()
{
    auto flexContainerIsHorizontal = flexBox().writingMode().isHorizontal();
    for (auto& layoutBox : formattingContextBoxes(flexBox())) {
        auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
        auto& flexItemGeometry = layoutState().geometryForBox(layoutBox);
        auto borderBox = Layout::BoxGeometry::borderBoxRect(flexItemGeometry);
        renderer.setLocation(flexContainerIsHorizontal ? borderBox.topLeft() : borderBox.topLeft().transposedPoint());
        renderer.setWidth(flexContainerIsHorizontal ? borderBox.width() : borderBox.height());
        renderer.setHeight(flexContainerIsHorizontal ? borderBox.height() : borderBox.width());

        renderer.setMarginStart(flexItemGeometry.marginStart());
        renderer.setMarginEnd(flexItemGeometry.marginEnd());
        renderer.setMarginBefore(flexItemGeometry.marginBefore());
        renderer.setMarginAfter(flexItemGeometry.marginAfter());

        if (!renderer.everHadLayout() || renderer.checkForRepaintDuringLayout())
            renderer.repaint();
    }
}

void FlexLayout::paint(PaintInfo&, const LayoutPoint&)
{
}

bool FlexLayout::hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction)
{
    return false;
}

void FlexLayout::collectOverflow()
{
}

LayoutUnit FlexLayout::contentBoxLogicalHeight() const
{
    auto contentLogicalBottom = LayoutUnit { };
    for (auto& layoutBox : formattingContextBoxes(flexBox()))
        contentLogicalBottom = std::max(contentLogicalBottom, Layout::BoxGeometry::marginBoxRect(layoutState().geometryForBox(layoutBox)).bottom());
    return contentLogicalBottom - layoutState().geometryForBox(flexBox()).contentBoxTop();
}

}
}

