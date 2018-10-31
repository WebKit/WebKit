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
#include "BlockInvalidation.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingState.h"
#include "Invalidation.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockInvalidation);

static bool invalidationStopsAtFormattingContextBoundary(const Container& formattingContextRoot, const Box&, StyleDiff)
{
    UNUSED_PARAM(formattingContextRoot);

    ASSERT(formattingContextRoot.establishesFormattingContext());
    return true;
}

static LayoutState::UpdateType computeUpdateType(const Box&, StyleDiff, BlockFormattingState&)
{
    return LayoutState::UpdateType::All;
}

static LayoutState::UpdateType computeUpdateTypeForAncestor(const Container&, StyleDiff, BlockFormattingState&)
{
    return LayoutState::UpdateType::All;
}

InvalidationResult BlockInvalidation::invalidate(const Box& layoutBox, StyleDiff styleDiff, LayoutState& layoutState,
    BlockFormattingState& formattingState)
{
    // Invalidate this box and the containing block chain all the way up to the formatting context root (and beyond if needed).
    layoutState.markNeedsUpdate(layoutBox, computeUpdateType(layoutBox, styleDiff, formattingState));
    for (auto* containingBlock = layoutBox.containingBlock(); containingBlock; containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->establishesFormattingContext() && invalidationStopsAtFormattingContextBoundary(*containingBlock, layoutBox, styleDiff))
            return { containingBlock };
        layoutState.markNeedsUpdate(*containingBlock, computeUpdateTypeForAncestor(*containingBlock, styleDiff, formattingState));
    }
    // Invalidation always stops at the initial containing block.
    ASSERT_NOT_REACHED();
    return { nullptr };
}

}
}
#endif
