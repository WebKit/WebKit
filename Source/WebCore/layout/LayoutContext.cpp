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
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutContext);

LayoutContext::LayoutContext(Box& root)
    : m_root(makeWeakPtr(root))
{
}

void LayoutContext::updateLayout()
{
    auto context = formattingContext(*m_root);
    auto state = formattingState(*context);
    context->layout(state);
}

FormattingState& LayoutContext::formattingState(const FormattingContext& context)
{
    return *m_formattingStates.ensure(&context.root(), [&context] {
        return context.formattingState();
    }).iterator->value;
}

std::unique_ptr<FormattingContext> LayoutContext::formattingContext(Box& formattingContextRoot)
{
    if (formattingContextRoot.establishesBlockFormattingContext())
        return std::make_unique<BlockFormattingContext>(formattingContextRoot);

    if (formattingContextRoot.establishesInlineFormattingContext())
        return std::make_unique<InlineFormattingContext>(formattingContextRoot);

    ASSERT_NOT_REACHED();
    return nullptr;
}

}
}

#endif
