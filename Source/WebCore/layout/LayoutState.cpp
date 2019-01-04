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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutState);

LayoutState::LayoutState(const Container& initialContainingBlock, const LayoutSize& containerSize)
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
    displayBox.setContentBoxHeight(containerSize.height());
    displayBox.setContentBoxWidth(containerSize.width());

    m_formattingContextRootListForLayout.add(&initialContainingBlock);
}

void LayoutState::updateLayout()
{
    ASSERT(!m_formattingContextRootListForLayout.isEmpty());
    for (auto* layoutRoot : m_formattingContextRootListForLayout)
        layoutFormattingContextSubtree(*layoutRoot);
    m_formattingContextRootListForLayout.clear();
}

void LayoutState::layoutFormattingContextSubtree(const Box& layoutRoot)
{
    RELEASE_ASSERT(layoutRoot.establishesFormattingContext());
    auto& formattingState = createFormattingStateForFormattingRootIfNeeded(layoutRoot);
    auto formattingContext = formattingState.createFormattingContext(layoutRoot);
    formattingContext->layout();
    formattingContext->layoutOutOfFlowDescendants(layoutRoot);
}

Display::Box& LayoutState::displayBoxForLayoutBox(const Box& layoutBox) const
{
    return *m_layoutToDisplayBox.ensure(&layoutBox, [&layoutBox] {
        return std::make_unique<Display::Box>(layoutBox.style());
    }).iterator->value;
}

void LayoutState::styleChanged(const Box& layoutBox, StyleDiff styleDiff)
{
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
                return std::make_unique<InlineFormattingState>(FloatingState::create(*this, formattingRoot), *this);

            // Otherwise, the formatting context inherits the floats from the parent formatting context.
            // Find the formatting state in which this formatting root lives, not the one it creates and use its floating state.
            return std::make_unique<InlineFormattingState>(formattingStateForBox(formattingRoot).floatingState(), *this);
        }).iterator->value;
    }

    if (formattingRoot.establishesBlockFormattingContext()) {
        return *m_formattingStates.ensure(&formattingRoot, [&] {

            // Block formatting context always establishes a new floating state.
            return std::make_unique<BlockFormattingState>(FloatingState::create(*this, formattingRoot), *this);
        }).iterator->value;
    }

    CRASH();
}

}
}

#endif
