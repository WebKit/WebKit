/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "LayoutContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingContext.h"
#include "BlockFormattingState.h"
#include "DisplayBox.h"
#include "DisplayPainter.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "InvalidationContext.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutPhase.h"
#include "LayoutTreeBuilder.h"
#include "RenderStyleConstants.h"
#include "RenderView.h"
#include "TableFormattingContext.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutContext);

LayoutContext::LayoutContext(LayoutState& layoutState)
    : m_layoutState(layoutState)
{
}

void LayoutContext::layout(InvalidationState& invalidationState)
{
    PhaseScope scope(Phase::Type::Layout);

    auto& formattingContextRootsForLayout = invalidationState.formattingContextRoots();
    ASSERT(!formattingContextRootsForLayout.computesEmpty());
    for (auto& formattingContextRoot : formattingContextRootsForLayout)
        layoutFormattingContextSubtree(formattingContextRoot, invalidationState);
}

void LayoutContext::layoutFormattingContextSubtree(const Container& formattingContextRoot, InvalidationState& invalidationState)
{
    RELEASE_ASSERT(formattingContextRoot.establishesFormattingContext());
    auto formattingContext = createFormattingContext(formattingContextRoot, layoutState());
    formattingContext->layoutInFlowContent(invalidationState);
    formattingContext->layoutOutOfFlowContent(invalidationState);
}

std::unique_ptr<FormattingContext> LayoutContext::createFormattingContext(const Container& formattingContextRoot, LayoutState& layoutState)
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    if (formattingContextRoot.establishesInlineFormattingContext()) {
        auto& inlineFormattingState = downcast<InlineFormattingState>(layoutState.createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<InlineFormattingContext>(formattingContextRoot, inlineFormattingState);
    }

    if (formattingContextRoot.establishesBlockFormattingContext()) {
        ASSERT(formattingContextRoot.establishesBlockFormattingContextOnly());
        auto& blockFormattingState = downcast<BlockFormattingState>(layoutState.createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<BlockFormattingContext>(formattingContextRoot, blockFormattingState);
    }

    if (formattingContextRoot.establishesTableFormattingContext()) {
        auto& tableFormattingState = downcast<TableFormattingState>(layoutState.createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<TableFormattingContext>(formattingContextRoot, tableFormattingState);
    }

    CRASH();
}

static void initializeLayoutState(LayoutState& layoutState, const RenderView& renderView)
{
    auto quirksMode = [&] {
        auto& document = renderView.document();
        if (document.inLimitedQuirksMode())
            return LayoutState::QuirksMode::Limited;
        if (document.inQuirksMode())
            return LayoutState::QuirksMode::Yes;
        return LayoutState::QuirksMode::No;
    };

    layoutState.setQuirksMode(quirksMode());

    auto& layoutRoot = layoutState.root();
    layoutState.createFormattingStateForFormattingRootIfNeeded(layoutRoot);
    // Not efficient, but this is temporary anyway.
    // Collect the out-of-flow descendants at the formatting root level (as opposed to at the containing block level, though they might be the same).
    for (auto& descendant : descendantsOfType<Box>(layoutRoot)) {
        if (!descendant.isOutOfFlowPositioned())
            continue;
        auto& formattingState = layoutState.createFormattingStateForFormattingRootIfNeeded(descendant.formattingContextRoot());
        formattingState.addOutOfFlowBox(descendant);
    }
}

void LayoutContext::runLayout(LayoutState& layoutState)
{
    auto& layoutRoot = layoutState.root();
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutRoot);
    displayBox.setHorizontalMargin({ });
    displayBox.setHorizontalComputedMargin({ });
    displayBox.setVerticalMargin({ });
    displayBox.setBorder({ });
    displayBox.setPadding({ });
    displayBox.setTopLeft({ });
    displayBox.setContentBoxHeight(LayoutUnit(layoutRoot.style().logicalHeight().value()));
    displayBox.setContentBoxWidth(LayoutUnit(layoutRoot.style().logicalWidth().value()));

    auto invalidationState = InvalidationState { };
    auto invalidationContext = InvalidationContext { invalidationState };
    invalidationContext.styleChanged(*layoutRoot.firstChild(), StyleDifference::Layout);

    LayoutContext(layoutState).layout(invalidationState);
}

std::unique_ptr<LayoutState> LayoutContext::runLayoutAndVerify(const RenderView& renderView)
{
    auto layoutState = makeUnique<LayoutState>(TreeBuilder::createLayoutTree(renderView));
    initializeLayoutState(*layoutState, renderView);
    runLayout(*layoutState);
    LayoutContext::verifyAndOutputMismatchingLayoutTree(*layoutState, renderView);
    return layoutState;
}

void LayoutContext::paint(const LayoutState& layoutState, GraphicsContext& context, const IntRect& dirtyRect)
{
    Display::Painter::paint(layoutState, context, dirtyRect);
}

}
}

#endif
