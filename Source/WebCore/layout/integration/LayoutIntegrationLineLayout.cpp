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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "DisplayPainter.h"
#include "InlineFormattingState.h"
#include "InvalidationState.h"
#include "LayoutContext.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"
#include "RuntimeEnabledFeatures.h"
#include "SimpleLineLayout.h"

namespace WebCore {
namespace LayoutIntegration {

LineLayout::LineLayout(const RenderBlockFlow& flow)
    : m_flow(flow)
{
    m_treeContent = Layout::TreeBuilder::buildLayoutTree(flow);
}

LineLayout::~LineLayout() = default;

bool LineLayout::canUseFor(const RenderBlockFlow& flow)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return false;

    // Initially only a subset of SLL features is supported.
    if (!SimpleLineLayout::canUseFor(flow))
        return false;

    if (flow.style().textTransform() == TextTransform::Capitalize)
        return false;

    if (flow.fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow)
        return false;

    return true;
}

void LineLayout::layout()
{
    if (!m_layoutState)
        m_layoutState = makeUnique<Layout::LayoutState>(*m_treeContent);

    prepareRootGeometryForLayout();

    auto layoutContext = Layout::LayoutContext { *m_layoutState };
    auto invalidationState = Layout::InvalidationState { };

    layoutContext.layoutWithPreparedRootGeometry(invalidationState);

    auto& lineBoxes = downcast<Layout::InlineFormattingState>(m_layoutState->establishedFormattingState(rootLayoutBox())).displayInlineContent()->lineBoxes;
    m_contentLogicalHeight = lineBoxes.last().logicalBottom() - lineBoxes.first().logicalTop();
}

void LineLayout::prepareRootGeometryForLayout()
{
    auto& displayBox = m_layoutState->displayBoxForRootLayoutBox();

    // Don't set marging properties or height. These should not be be accessed by inline layout.
    displayBox.setBorder(Layout::Edges { { m_flow.borderStart(), m_flow.borderEnd() }, { m_flow.borderBefore(), m_flow.borderAfter() } });
    displayBox.setPadding(Layout::Edges { { m_flow.paddingStart(), m_flow.paddingEnd() }, { m_flow.paddingBefore(), m_flow.paddingAfter() } });
    displayBox.setContentBoxWidth(m_flow.contentSize().width());
}

const Display::InlineContent* LineLayout::displayInlineContent() const
{
    return downcast<Layout::InlineFormattingState>(m_layoutState->establishedFormattingState(rootLayoutBox())).displayInlineContent();
}

void LineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& graphicsContext = paintInfo.context();

    graphicsContext.save();
    graphicsContext.translate(paintOffset);

    Display::Painter::paintInlineFlow(*m_layoutState, paintInfo.context());

    graphicsContext.restore();
}

LineLayoutTraversal::TextBoxIterator LineLayout::textBoxesFor(const RenderText& renderText) const
{
    auto* inlineContent = displayInlineContent();
    if (!inlineContent)
        return { };
    auto* layoutBox = m_treeContent->layoutBoxForRenderer(renderText);
    ASSERT(layoutBox);

    Optional<size_t> firstIndex = 0;
    size_t lastIndex = 0;
    for (size_t i = 0; i < inlineContent->runs.size(); ++i) {
        auto& run =  inlineContent->runs[i];
        if (&run.layoutBox() == layoutBox) {
            if (!firstIndex)
                firstIndex = i;
            lastIndex = i;
        }
    }
    if (!firstIndex)
        return { };

    return { LineLayoutTraversal::DisplayRunPath(*inlineContent, *firstIndex, lastIndex + 1) };
}

LineLayoutTraversal::ElementBoxIterator LineLayout::elementBoxFor(const RenderLineBreak& renderLineBreak) const
{
    auto* inlineContent = displayInlineContent();
    if (!inlineContent)
        return { };
    auto* layoutBox = m_treeContent->layoutBoxForRenderer(renderLineBreak);
    ASSERT(layoutBox);

    for (size_t i = 0; i < inlineContent->runs.size(); ++i) {
        auto& run =  inlineContent->runs[i];
        if (&run.layoutBox() == layoutBox)
            return { LineLayoutTraversal::DisplayRunPath(*inlineContent, i, i + 1) };
    }

    return { };
}

const Layout::Container& LineLayout::rootLayoutBox() const
{
    return m_treeContent->rootLayoutBox();
}

}
}

#endif
