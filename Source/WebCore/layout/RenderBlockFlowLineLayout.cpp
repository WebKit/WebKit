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
#include "RenderBlockFlowLineLayout.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "DisplayPainter.h"
#include "InlineFormattingState.h"
#include "InvalidationState.h"
#include "LayoutContext.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RuntimeEnabledFeatures.h"
#include "SimpleLineLayout.h"

namespace WebCore {
namespace Layout {

RenderBlockFlowLineLayout::RenderBlockFlowLineLayout(const RenderBlockFlow& flow)
    : m_flow(flow)
{
    m_treeContent = TreeBuilder::buildLayoutTree(flow);
}

RenderBlockFlowLineLayout::~RenderBlockFlowLineLayout() = default;

bool RenderBlockFlowLineLayout::canUseFor(const RenderBlockFlow& flow)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextRenderTreeIntegrationEnabled())
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

void RenderBlockFlowLineLayout::layout()
{
    if (!m_layoutState)
        m_layoutState = makeUnique<LayoutState>(*m_treeContent);

    auto& rootContainer = m_layoutState->root();
    auto layoutContext = LayoutContext { *m_layoutState };
    auto invalidationState = InvalidationState { };
    layoutContext.layout(m_flow.contentSize(), invalidationState);

    auto& lineBoxes = downcast<InlineFormattingState>(m_layoutState->establishedFormattingState(rootContainer)).lineBoxes();
    auto height = lineBoxes.last()->logicalBottom();

    auto& displayBox = m_layoutState->displayBoxForLayoutBox(rootContainer);
    displayBox.setContentBoxHeight(height);
}

LayoutUnit RenderBlockFlowLineLayout::contentBoxHeight() const
{
    return m_layoutState ? m_layoutState->displayBoxForLayoutBox(m_layoutState->root()).contentBoxHeight() : 0_lu;
}

void RenderBlockFlowLineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto& graphicsContext = paintInfo.context();

    graphicsContext.save();
    graphicsContext.translate(paintOffset);

    Display::Painter::paintInlineFlow(*m_layoutState, paintInfo.context());

    graphicsContext.restore();
}

}
}

#endif
