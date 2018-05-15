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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Box& formattingContextRoot)
    : FormattingContext(formattingContextRoot)
{
}

void InlineFormattingContext::layout(LayoutContext&, FormattingState&) const
{
}

std::unique_ptr<FormattingState> InlineFormattingContext::createFormattingState(Ref<FloatingState>&& floatingState) const
{
    return std::make_unique<InlineFormattingState>(WTFMove(floatingState));
}

Ref<FloatingState> InlineFormattingContext::createOrFindFloatingState(LayoutContext& layoutContext) const
{
    // If the block container box that initiates this inline formatting context also establishes a block context, the floats outside of the formatting root
    // should not interfere with the content inside.
    // <div style="float: left"></div><div style="overflow: hidden"> <- is a non-intrusive float, because overflow: hidden triggers new block formatting context.</div>
    if (root().establishesBlockFormattingContext())
        return FloatingState::create();
    // Otherwise, the formatting context inherits the floats from the parent formatting context.
    // Find the formatting state in which this formatting root lives, not the one it creates (this) and use its floating state.
    auto& formattingState = layoutContext.formattingStateForBox(root());
    return formattingState.floatingState();
}

void InlineFormattingContext::computeInFlowWidth(LayoutContext&, const Box&, Display::Box&) const
{
}

void InlineFormattingContext::computeInFlowHeight(LayoutContext&, const Box&, Display::Box&) const
{
}

}
}

#endif
