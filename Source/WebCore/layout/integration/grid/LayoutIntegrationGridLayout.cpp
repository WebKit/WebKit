/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationGridLayout.h"

#include "FormattingContextBoxIterator.h"
#include "GridFormattingContext.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationBoxTreeUpdater.h"
#include "RenderGrid.h"
#include "RenderView.h"
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

namespace LayoutIntegration {

GridLayout::GridLayout(RenderGrid& renderGrid)
    : m_gridBox(BoxTreeUpdater { renderGrid }.build())
    , m_layoutState(renderGrid.view().layoutState())
{
}

void GridLayout::updateFormattingContextGeometries()
{
    auto boxGeometryUpdater = BoxGeometryUpdater { layoutState(), gridBox() };
    CheckedPtr gridBoxContainingBlock = CheckedRef { gridBoxRenderer() }->containingBlock();

    boxGeometryUpdater.setFormattingContextRootGeometry(gridBoxContainingBlock->contentBoxLogicalWidth());
    boxGeometryUpdater.setFormattingContextContentGeometry(CheckedRef { layoutState() }->geometryForBox(gridBox()).contentBoxWidth(), { });
}

static inline Layout::GridFormattingContext::GridLayoutConstraints constraintsForGridContent(const Layout::ElementBox& gridContainer)
{
    CheckedRef gridContainerRenderer = downcast<RenderGrid>(*gridContainer.rendererForIntegration());

    auto availableInlineSpace = [&]() -> LayoutUnit {
        if (auto overridingWidth = gridContainerRenderer->overridingBorderBoxLogicalWidth())
            return gridContainerRenderer->contentBoxLogicalWidth(*overridingWidth);
        return gridContainerRenderer->contentBoxLogicalWidth();
    }();
    auto availableBlockSpace = gridContainerRenderer->availableLogicalHeightForContentBox();

    return {
        .inlineAxisAvailableSpace = availableInlineSpace,
        .blockAxisAvailableSpace = availableBlockSpace
    };
}

void GridLayout::updateGridItemRenderers()
{
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        CheckedRef renderer = downcast<RenderBox>(*layoutBox->rendererForIntegration());
        auto& gridItemGeometry = CheckedRef { layoutState() }->geometryForBox(layoutBox);
        auto borderBoxRect = Layout::BoxGeometry::borderBoxRect(gridItemGeometry);

        renderer->setLocation(borderBoxRect.topLeft());
        renderer->setWidth(borderBoxRect.width());
        renderer->setHeight(borderBoxRect.height());

        renderer->setMarginBefore(gridItemGeometry.marginBefore());
        renderer->setMarginAfter(gridItemGeometry.marginAfter());
        renderer->setMarginStart(gridItemGeometry.marginStart());
        renderer->setMarginEnd(gridItemGeometry.marginEnd());
    }
}

void GridLayout::updateFormattingContextRootRenderer()
{
    CheckedRef renderGrid = gridBoxRenderer();
    auto& currentGrid = renderGrid->currentGrid();
    currentGrid.setNeedsItemsPlacement(false);
    OrderIteratorPopulator orderIteratorPopulator(currentGrid.orderIterator());

    for (CheckedRef layoutBox : formattingContextBoxes(gridBox()))
        orderIteratorPopulator.collectChild(CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) });
}

void GridLayout::layout()
{
    Layout::GridFormattingContext { gridBox(), layoutState() }.layout(constraintsForGridContent(gridBox()));
    updateGridItemRenderers();
    updateFormattingContextRootRenderer();
}

TextStream& operator<<(TextStream& stream, const GridLayout& layout)
{
    stream << "GridLayout@" << &layout;
    stream << " gridBox=" << &layout.gridBox();
    size_t index = 0;
    for (CheckedRef box : Layout::formattingContextBoxes(layout.gridBox())) {
        stream << "\n  [" << index++ << "] box=" << box.ptr();
        stream << " anonymous=" << (box->isAnonymous() ? "yes" : "no");
        stream << " establishesContext=" << (box->establishesFormattingContext() ? "yes" : "no");
        stream << " display=" << box->style().display();
        if (CheckedPtr renderer = box->rendererForIntegration())
            stream << " renderer=" << renderer->renderName() << '@' << renderer.get();
        else
            stream << " renderer=<null>";
    }
    return stream;
}

} // namespace LayoutIntegration

} // namespace WebCore
