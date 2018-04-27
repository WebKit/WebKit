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
#include "BlockFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingState.h"
#include "FloatingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const Box& formattingContextRoot, LayoutContext& layoutContext)
    : FormattingContext(formattingContextRoot, layoutContext)
{
}

void BlockFormattingContext::layout(FormattingState&)
{
}

std::unique_ptr<FormattingState> BlockFormattingContext::createFormattingState(Ref<FloatingState>&& floatingState) const
{
    return std::make_unique<BlockFormattingState>(WTFMove(floatingState));
}

Ref<FloatingState> BlockFormattingContext::createOrFindFloatingState() const
{
    // Block formatting context always establishes a new floating state.
    return FloatingState::create();
}

void BlockFormattingContext::computeStaticPosition(const Box&) const
{
}

void BlockFormattingContext::computeWidth(const Box&) const
{
}

void BlockFormattingContext::computeHeight(const Box&) const
{
}

LayoutUnit BlockFormattingContext::marginTop(const Box&) const
{
    return 0;
}

LayoutUnit BlockFormattingContext::marginBottom(const Box&) const
{
    return 0;
}

}
}

#endif
