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
#include "LayoutState.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingContext.h"
#include "BlockFormattingState.h"
#include "BlockInvalidation.h"
#include "DisplayBox.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "InlineInvalidation.h"
#include "Invalidation.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutPhase.h"
#include "LayoutTreeBuilder.h"
#include "RenderView.h"
#include "TableFormattingContext.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutState);

LayoutState::LayoutState(const Container& initialContainingBlock)
    : m_initialContainingBlock(makeWeakPtr(initialContainingBlock))
{
    // LayoutState is always initiated with the ICB.
    ASSERT(!initialContainingBlock.parent());
    ASSERT(initialContainingBlock.establishesBlockFormattingContext());

    auto& displayBox = displayBoxForLayoutBox(initialContainingBlock);
    displayBox.setHorizontalMargin({ });
    displayBox.setHorizontalComputedMargin({ });
    displayBox.setVerticalMargin({ });
    displayBox.setBorder({ });
    displayBox.setPadding({ });
    displayBox.setTopLeft({ });
    displayBox.setContentBoxHeight(LayoutUnit(initialContainingBlock.style().logicalHeight().value()));
    displayBox.setContentBoxWidth(LayoutUnit(initialContainingBlock.style().logicalWidth().value()));

    m_formattingContextRootListForLayout.add(&initialContainingBlock);
}

void LayoutState::updateLayout()
{
    PhaseScope scope(Phase::Type::Layout);

    ASSERT(!m_formattingContextRootListForLayout.isEmpty());
    for (auto* layoutRoot : m_formattingContextRootListForLayout)
        layoutFormattingContextSubtree(*layoutRoot);
    m_formattingContextRootListForLayout.clear();
}

void LayoutState::layoutFormattingContextSubtree(const Box& layoutRoot)
{
    RELEASE_ASSERT(layoutRoot.establishesFormattingContext());
    auto formattingContext = createFormattingContext(layoutRoot);
    formattingContext->layout();
    formattingContext->layoutOutOfFlowContent();
}

Display::Box& LayoutState::displayBoxForLayoutBox(const Box& layoutBox) const
{
    return *m_layoutToDisplayBox.ensure(&layoutBox, [&layoutBox] {
        return makeUnique<Display::Box>(layoutBox.style());
    }).iterator->value;
}

void LayoutState::styleChanged(const Box& layoutBox, StyleDiff styleDiff)
{
    PhaseScope scope(Phase::Type::Invalidation);

    auto& formattingState = formattingStateForBox(layoutBox);
    const Container* invalidationRoot = nullptr;
    if (is<BlockFormattingState>(formattingState))
        invalidationRoot = BlockInvalidation::invalidate(layoutBox, styleDiff, *this, downcast<BlockFormattingState>(formattingState)).root;
    else if (is<InlineFormattingState>(formattingState))
        invalidationRoot = InlineInvalidation::invalidate(layoutBox, styleDiff, *this, downcast<InlineFormattingState>(formattingState)).root;
    else
        ASSERT_NOT_IMPLEMENTED_YET();
    ASSERT(invalidationRoot);
    m_formattingContextRootListForLayout.addVoid(invalidationRoot);
}

void LayoutState::markNeedsUpdate(const Box&, OptionSet<UpdateType>)
{
}

FormattingState& LayoutState::formattingStateForBox(const Box& layoutBox) const
{
    auto& root = layoutBox.formattingContextRoot();
    RELEASE_ASSERT(m_formattingStates.contains(&root));
    return *m_formattingStates.get(&root);
}

FormattingState& LayoutState::establishedFormattingState(const Box& formattingRoot) const
{
    ASSERT(formattingRoot.establishesFormattingContext());
    RELEASE_ASSERT(m_formattingStates.contains(&formattingRoot));
    return *m_formattingStates.get(&formattingRoot);
}

FormattingState& LayoutState::createFormattingStateForFormattingRootIfNeeded(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());

    if (formattingRoot.establishesInlineFormattingContext()) {
        return *m_formattingStates.ensure(&formattingRoot, [&] {

            // If the block container box that initiates this inline formatting context also establishes a block context, the floats outside of the formatting root
            // should not interfere with the content inside.
            // <div style="float: left"></div><div style="overflow: hidden"> <- is a non-intrusive float, because overflow: hidden triggers new block formatting context.</div>
            if (formattingRoot.establishesBlockFormattingContext())
                return makeUnique<InlineFormattingState>(FloatingState::create(*this, formattingRoot), *this);

            // Otherwise, the formatting context inherits the floats from the parent formatting context.
            // Find the formatting state in which this formatting root lives, not the one it creates and use its floating state.
            auto& parentFormattingState = createFormattingStateForFormattingRootIfNeeded(formattingRoot.formattingContextRoot()); 
            auto& parentFloatingState = parentFormattingState.floatingState();
            return makeUnique<InlineFormattingState>(parentFloatingState, *this);
        }).iterator->value;
    }

    if (formattingRoot.establishesBlockFormattingContext()) {
        return *m_formattingStates.ensure(&formattingRoot, [&] {

            // Block formatting context always establishes a new floating state.
            return makeUnique<BlockFormattingState>(FloatingState::create(*this, formattingRoot), *this);
        }).iterator->value;
    }

    if (formattingRoot.establishesTableFormattingContext()) {
        return *m_formattingStates.ensure(&formattingRoot, [&] {

            // Table formatting context always establishes a new floating state -and it stays empty.
            return makeUnique<TableFormattingState>(FloatingState::create(*this, formattingRoot), *this);
        }).iterator->value;
    }

    CRASH();
}

std::unique_ptr<FormattingContext> LayoutState::createFormattingContext(const Box& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    if (formattingContextRoot.establishesInlineFormattingContext()) {
        auto& inlineFormattingState = downcast<InlineFormattingState>(createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<InlineFormattingContext>(formattingContextRoot, inlineFormattingState);
    }

    if (formattingContextRoot.establishesBlockFormattingContext()) {
        ASSERT(formattingContextRoot.establishesBlockFormattingContextOnly());
        auto& blockFormattingState = downcast<BlockFormattingState>(createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<BlockFormattingContext>(formattingContextRoot, blockFormattingState);
    }

    if (formattingContextRoot.establishesTableFormattingContext()) {
        auto& tableFormattingState = downcast<TableFormattingState>(createFormattingStateForFormattingRootIfNeeded(formattingContextRoot));
        return makeUnique<TableFormattingContext>(formattingContextRoot, tableFormattingState);
    }

    CRASH();
}

void LayoutState::run(const RenderView& renderView)
{
    auto initialContainingBlock = TreeBuilder::createLayoutTree(renderView);
    auto layoutState = LayoutState(*initialContainingBlock);
    // Not efficient, but this is temporary anyway.
    // Collect the out-of-flow descendants at the formatting root level (as opposed to at the containing block level, though they might be the same).
    for (auto& descendant : descendantsOfType<Box>(*initialContainingBlock)) {
        if (!descendant.isOutOfFlowPositioned())
            continue;
        auto& formattingState = layoutState.createFormattingStateForFormattingRootIfNeeded(descendant.formattingContextRoot());
        formattingState.addOutOfFlowBox(descendant);
    }
    auto quirksMode = [&] {
        auto& document = renderView.document();
        if (document.inLimitedQuirksMode())
            return QuirksMode::Limited;
        if (document.inQuirksMode())
            return QuirksMode::Yes;
        return QuirksMode::No;
    };
    layoutState.setQuirksMode(quirksMode());
    layoutState.updateLayout();
    layoutState.verifyAndOutputMismatchingLayoutTree(renderView);
}

}
}

#endif
