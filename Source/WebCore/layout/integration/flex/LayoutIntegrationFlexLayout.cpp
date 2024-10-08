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

// FIXME: Merge these with the other integration layout functions.
static inline Layout::BoxGeometry::Edges flexBoxLogicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, FlowDirection blockFlowDirection)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(blockFlowDirection);

    auto borderLeft = renderer.borderLeft();
    auto borderRight = renderer.borderRight();
    auto borderTop = renderer.borderTop();
    auto borderBottom = renderer.borderBottom();

    return { { borderLeft, borderRight }, { borderTop, borderBottom } };
}

static inline Layout::BoxGeometry::Edges flexBoxLogicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, FlowDirection blockFlowDirection)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(blockFlowDirection);

    auto padding = renderer.padding();
    return { { padding.left(), padding.right() }, { padding.top(), padding.bottom() } };
}

void FlexLayout::updateFormattingContexGeometries(LayoutUnit availableLogicalWidth)
{
    auto boxGeometryUpdater = BoxGeometryUpdater { layoutState(), flexBox() };
    boxGeometryUpdater.setFormattingContextRootGeometry(availableLogicalWidth);
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
    auto& rootGeometry = layoutState().geometryForBox(flexBox());
    auto crossAxisSpaceForFlexItems = [&]() -> std::tuple<std::optional<LayoutUnit>, std::optional<LayoutUnit>> {
        auto& flexBoxStyle = flexBox().style();

        auto heightValue = [&](auto& computedValue) -> std::optional<LayoutUnit> {
            if (computedValue.isPercent())
                return flexBoxRenderer().computePercentageLogicalHeight(computedValue, RenderBoxModelObject::UpdatePercentageHeightDescendants::No);

            if (computedValue.isFixed()) {
                auto boxSizingIsContentBox = flexBoxStyle.boxSizing() == BoxSizing::ContentBox;
                return LayoutUnit { boxSizingIsContentBox ? computedValue.value() : computedValue.value() - rootGeometry.verticalMarginBorderAndPadding() };
            }

            return { };
        };

        auto crossAxisSpace = heightValue(flexBoxStyle.logicalHeight());
        if (auto maximumHeightValue = heightValue(flexBoxStyle.logicalMaxHeight()))
            crossAxisSpace = crossAxisSpace ? std::min(*crossAxisSpace, *maximumHeightValue) : *maximumHeightValue;
        return { crossAxisSpace, heightValue(flexBoxStyle.logicalMinHeight()) };
    };
    auto [availableCrossAxisSpace, minimumCrossAxisSpace] = crossAxisSpaceForFlexItems();
    auto constraints = Layout::ConstraintsForFlexContent { { { }, { }, rootGeometry.contentBoxWidth(), rootGeometry.contentBoxLeft() }, { minimumCrossAxisSpace, { }, availableCrossAxisSpace, rootGeometry.contentBoxTop() }, false };
    auto flexFormattingContext = Layout::FlexFormattingContext { flexBox(), layoutState() };
    flexFormattingContext.layout(constraints);

    updateRenderers();

    auto relayoutFlexItems = [&] {
        // Flex items need to be laid out now with their final size (and through setOverridingLogicalWidth/Height)
        // Note that they may re-size themselves.
        for (auto& layoutBox : formattingContextBoxes(flexBox())) {
            auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
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
    for (auto& layoutBox : formattingContextBoxes(flexBox())) {
        auto& renderer = downcast<RenderBox>(*layoutBox.rendererForIntegration());
        auto& flexItemGeometry = layoutState().geometryForBox(layoutBox);
        auto borderBox = Layout::BoxGeometry::borderBoxRect(flexItemGeometry);
        renderer.setLocation(borderBox.topLeft());
        renderer.setWidth(borderBox.width());
        renderer.setHeight(borderBox.height());

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

