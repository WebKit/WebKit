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
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderView.h"

namespace WebCore {
namespace LayoutIntegration {

FlexLayout::FlexLayout(RenderFlexibleBox& flexBoxRenderer)
    : m_boxTree(flexBoxRenderer)
    , m_layoutState(flexBoxRenderer.view().layoutState())
{
}

FlexLayout::~FlexLayout()
{
}

// FIXME: Merge these with the other integration layout functions.
static inline Layout::BoxGeometry::Edges flexBoxLogicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(blockFlowDirection);

    auto borderLeft = renderer.borderLeft();
    auto borderRight = renderer.borderRight();
    auto borderTop = renderer.borderTop();
    auto borderBottom = renderer.borderBottom();

    return { { borderLeft, borderRight }, { borderTop, borderBottom } };
}

static inline Layout::BoxGeometry::Edges flexBoxLogicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, BlockFlowDirection blockFlowDirection)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(blockFlowDirection);

    auto paddingLeft = renderer.paddingLeft();
    auto paddingRight = renderer.paddingRight();
    auto paddingTop = renderer.paddingTop();
    auto paddingBottom = renderer.paddingBottom();

    return { { paddingLeft, paddingRight }, { paddingTop, paddingBottom } };
}

void FlexLayout::updateFormattingRootGeometryAndInvalidate()
{
    auto updateGeometry = [&](auto& root) {
        auto& flexBoxRenderer = this->flexBoxRenderer();

        auto isLeftToRightInlineDirection = flexBoxRenderer.style().isLeftToRightDirection();
        auto blockFlowDirection = writingModeToBlockFlowDirection(flexBoxRenderer.style().writingMode());

        root.setContentBoxWidth(blockFlowDirection == BlockFlowDirection::TopToBottom ? flexBoxRenderer.contentWidth() : flexBoxRenderer.contentHeight());
        root.setPadding(flexBoxLogicalPadding(flexBoxRenderer, isLeftToRightInlineDirection, blockFlowDirection));
        root.setBorder(flexBoxLogicalBorder(flexBoxRenderer, isLeftToRightInlineDirection, blockFlowDirection));
        root.setHorizontalMargin({ });
        root.setVerticalMargin({ });
    };
    updateGeometry(layoutState().ensureGeometryForBox(flexBox()));
}

void FlexLayout::updateFlexItemDimensions(const RenderBlock& flexItem, LayoutUnit, LayoutUnit)
{
    auto& rootGeometry = layoutState().geometryForBox(flexBox());
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(flexItem);
    auto& boxGeometry = layoutState().ensureGeometryForBox(layoutBox);
    auto& style = flexItem.style();

    boxGeometry.setVerticalMargin({ flexItem.marginTop(), flexItem.marginBottom() });
    boxGeometry.setHorizontalMargin({ flexItem.marginLeft(), flexItem.marginRight() });
    boxGeometry.setBorder({ { flexItem.borderLeft(), flexItem.borderRight() }, { flexItem.borderTop(), flexItem.borderBottom() } });
    boxGeometry.setPadding(Layout::BoxGeometry::Edges { { flexItem.paddingLeft(), flexItem.paddingRight() }, { flexItem.paddingTop(), flexItem.paddingBottom() } });

    if (style.width().isFixed() || style.width().isPercentOrCalculated()) {
        auto widthValue = minimumValueForLength(style.width(), rootGeometry.contentBoxWidth());
        if (style.boxSizing() == BoxSizing::BorderBox)
            widthValue = std::max(0_lu, widthValue - boxGeometry.horizontalBorderAndPadding());
        boxGeometry.setContentBoxWidth(widthValue);
    }

    if (style.height().isFixed()) {
        auto heightValue = LayoutUnit { style.height().value() };
        if (style.boxSizing() == BoxSizing::BorderBox)
            heightValue = std::max(0_lu, heightValue - boxGeometry.verticalBorderAndPadding());
        boxGeometry.setContentBoxHeight(heightValue);
    }
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
    auto& rootGeometry = layoutState().geometryForBox(flexBox());
    auto horizontalConstraints = Layout::HorizontalConstraints { rootGeometry.contentBoxLeft(), rootGeometry.contentBoxWidth() };
    auto verticalSpaceForFlexItems = [&]() -> std::tuple<std::optional<LayoutUnit>, std::optional<LayoutUnit>> {
        auto& flexBoxStyle = flexBox().style();

        auto adjustedHeightValue = [&](auto& property) -> std::optional<LayoutUnit> {
            if (!property.isFixed())
                return { };
            auto boxSizingIsContentBox = flexBoxStyle.boxSizing() == BoxSizing::ContentBox;
            return LayoutUnit { boxSizingIsContentBox ? property.value() : property.value() - rootGeometry.verticalMarginBorderAndPadding() };
        };

        auto verticalSpace = adjustedHeightValue(flexBoxStyle.logicalHeight());
        if (auto maximumHeightValue = adjustedHeightValue(flexBoxStyle.logicalMaxHeight()))
            verticalSpace = verticalSpace ? std::min(*verticalSpace, *maximumHeightValue) : *maximumHeightValue;
        return { verticalSpace, adjustedHeightValue(flexBoxStyle.logicalMinHeight()) };
    };
    auto [availableVerticalSpace, minimumVerticalSpace] = verticalSpaceForFlexItems();
    auto constraints = Layout::ConstraintsForFlexContent { { horizontalConstraints, rootGeometry.contentBoxTop() }, availableVerticalSpace, minimumVerticalSpace };
    auto flexFormattingContext = Layout::FlexFormattingContext { flexBox(), layoutState() };
    flexFormattingContext.layout(constraints);

    updateRenderers();

    auto relayoutFlexItems = [&] {
        // Flex items need to be laid out now with their final size (and through setOverridingLogicalWidth/Height)
        // Note that they may re-size themselves.
        for (auto& renderObject : m_boxTree.renderers()) {
            auto& renderer = downcast<RenderBox>(*renderObject);
            auto& layoutBox = *renderer.layoutBox();
            auto borderBox = Layout::BoxGeometry::borderBoxRect(layoutState().geometryForBox(layoutBox));

            renderer.setWidth(LayoutUnit { });
            renderer.setHeight(LayoutUnit { });
            // FIXME: This may need a visual vs. logical flip.
            renderer.setOverridingLogicalWidth(borderBox.width());
            renderer.setOverridingLogicalHeight(borderBox.height());

            renderer.setChildNeedsLayout(MarkOnlyThis);
            renderer.layoutIfNeeded();
            renderer.clearOverridingContentSize();

            renderer.setWidth(borderBox.width());
            renderer.setHeight(borderBox.height());
        }
    };
    relayoutFlexItems();
}

void FlexLayout::updateRenderers()
{
    for (auto& renderObject : m_boxTree.renderers()) {
        auto& renderer = downcast<RenderBox>(*renderObject);
        auto& layoutBox = *renderer.layoutBox();
        auto& flexItemGeometry = layoutState().geometryForBox(layoutBox);
        auto borderBox = Layout::BoxGeometry::borderBoxRect(flexItemGeometry);
        renderer.setLocation(borderBox.topLeft());
        renderer.setWidth(borderBox.width());
        renderer.setHeight(borderBox.height());

        renderer.setMarginStart(flexItemGeometry.marginStart());
        renderer.setMarginEnd(flexItemGeometry.marginEnd());
        renderer.setMarginBefore(flexItemGeometry.marginBefore());
        renderer.setMarginAfter(flexItemGeometry.marginAfter());
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

LayoutUnit FlexLayout::contentLogicalHeight() const
{
    return { };
}

}
}

