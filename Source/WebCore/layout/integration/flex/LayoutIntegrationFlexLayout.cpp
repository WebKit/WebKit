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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FlexFormattingConstraints.h"
#include "FlexFormattingContext.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "RenderFlexibleBox.h"

namespace WebCore {
namespace LayoutIntegration {

FlexLayout::FlexLayout(RenderFlexibleBox& flexBoxRenderer)
    : m_boxTree(flexBoxRenderer)
    , m_layoutState(flexBoxRenderer.document(), rootLayoutBox(), Layout::LayoutState::FormattingContextIntegrationType::Flex)
    , m_flexFormattingState(m_layoutState.ensureFlexFormattingState(rootLayoutBox()))
{
}

// FIXME: Merge these with the other integration layout functions.
static inline Layout::Edges flexBoxLogicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, WritingMode writingMode)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(writingMode);

    auto borderLeft = renderer.borderLeft();
    auto borderRight = renderer.borderRight();
    auto borderTop = renderer.borderTop();
    auto borderBottom = renderer.borderBottom();

    return { { borderLeft, borderRight }, { borderTop, borderBottom } };
}

static inline Layout::Edges flexBoxLogicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightInlineDirection, WritingMode writingMode)
{
    UNUSED_PARAM(isLeftToRightInlineDirection);
    UNUSED_PARAM(writingMode);

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
        auto writingMode = flexBoxRenderer.style().writingMode();

        root.setContentBoxWidth(writingMode == WritingMode::TopToBottom ? flexBoxRenderer.contentWidth() : flexBoxRenderer.contentHeight());
        root.setPadding(flexBoxLogicalPadding(flexBoxRenderer, isLeftToRightInlineDirection, writingMode));
        root.setBorder(flexBoxLogicalBorder(flexBoxRenderer, isLeftToRightInlineDirection, writingMode));
        root.setHorizontalMargin({ });
        root.setVerticalMargin({ });
    };
    updateGeometry(m_layoutState.ensureGeometryForBox(rootLayoutBox()));

    for (auto& flexItem : Layout::childrenOfType<Layout::Box>(rootLayoutBox()))
        m_flexFormattingState.clearIntrinsicWidthConstraints(flexItem);
}

void FlexLayout::updateFlexItemDimensions(const RenderBlock& flexItem, LayoutUnit minimumContentSize, LayoutUnit maximumContentSize)
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(flexItem);
    auto& boxGeometry = m_layoutState.ensureGeometryForBox(layoutBox);

    boxGeometry.setContentBoxWidth(flexItem.contentWidth());
    boxGeometry.setContentBoxHeight(flexItem.contentHeight());
    boxGeometry.setVerticalMargin({ flexItem.marginTop(), flexItem.marginBottom() });
    boxGeometry.setHorizontalMargin({ flexItem.marginLeft(), flexItem.marginRight() });
    boxGeometry.setBorder({ { flexItem.borderLeft(), flexItem.borderRight() }, { flexItem.borderTop(), flexItem.borderBottom() } });
    boxGeometry.setPadding(Layout::Edges { { flexItem.paddingLeft(), flexItem.paddingRight() }, { flexItem.paddingTop(), flexItem.paddingBottom() } });

    // FIXME: We may need to differentiate preferred and min/max content size.
    m_flexFormattingState.setIntrinsicWidthConstraintsForBox(layoutBox, { minimumContentSize, maximumContentSize });
}

void FlexLayout::updateStyle(const RenderBlock&, const RenderStyle&)
{
}

std::pair<LayoutUnit, LayoutUnit> FlexLayout::computeIntrinsicWidthConstraints()
{
    auto flexFormattingContext = Layout::FlexFormattingContext { rootLayoutBox(), m_flexFormattingState };
    auto constraints = flexFormattingContext.computedIntrinsicWidthConstraintsForIntegration();

    return { constraints.minimum, constraints.maximum };
}

void FlexLayout::layout()
{
    auto& rootGeometry = m_layoutState.geometryForBox(rootLayoutBox());
    auto horizontalConstraints = Layout::HorizontalConstraints { rootGeometry.contentBoxLeft(), rootGeometry.contentBoxWidth() };
    auto rootBoxLogicalHeight = rootLayoutBox().style().logicalHeight();
    auto availableVerticalSpace = std::optional<LayoutUnit> { rootBoxLogicalHeight.isFixed() ? std::make_optional(rootBoxLogicalHeight.value()) : std::nullopt };
    auto constraints = Layout::ConstraintsForFlexContent { { horizontalConstraints, rootGeometry.contentBoxTop() }, availableVerticalSpace };

    auto flexFormattingContext = Layout::FlexFormattingContext { rootLayoutBox(), m_flexFormattingState };
    flexFormattingContext.layoutInFlowContentForIntegration(constraints);

    updateRenderers();
}

void FlexLayout::updateRenderers() const
{
    auto& boxAndRendererList = m_boxTree.boxAndRendererList();
    for (auto& boxAndRenderer : boxAndRendererList) {
        auto& layoutBox = boxAndRenderer.box.get();

        auto& renderer = downcast<RenderBox>(*boxAndRenderer.renderer);
        auto& flexItemGeometry = m_flexFormattingState.boxGeometry(layoutBox);
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
    return Layout::FlexFormattingContext { rootLayoutBox(), m_flexFormattingState }.usedContentHeight();
}

}
}

#endif
