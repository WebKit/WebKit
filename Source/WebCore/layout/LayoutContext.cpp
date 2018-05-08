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

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutContext);

LayoutContext::LayoutContext(const Box& root)
    : m_root(makeWeakPtr(const_cast<Box&>(root)))
{
}

void LayoutContext::updateLayout()
{
    ASSERT(!m_formattingContextRootListForLayout.isEmpty());
    for (auto* layoutRoot : m_formattingContextRootListForLayout) {
        RELEASE_ASSERT(layoutRoot->establishesFormattingContext());
        auto context = formattingContext(*layoutRoot);
        auto& state = establishedFormattingState(*layoutRoot, *context);
        context->layout(*this, state);
    }
    m_formattingContextRootListForLayout.clear();
}

Display::Box& LayoutContext::createDisplayBox(const Box& layoutBox)
{
    std::unique_ptr<Display::Box> displayBox(new Display::Box());
    auto* displayBoxPtr = displayBox.get();
    m_layoutToDisplayBox.add(&layoutBox, WTFMove(displayBox));
    return *displayBoxPtr;
}

void LayoutContext::styleChanged(const Box& layoutBox, StyleDiff styleDiff)
{
    auto& formattingState = formattingStateForBox(layoutBox);
    const Container* invalidationRoot = nullptr;
    if (is<BlockFormattingState>(formattingState))
        invalidationRoot = BlockInvalidation::invalidate(layoutBox, styleDiff, *this, downcast<BlockFormattingState>(formattingState)).root;
    else if (is<InlineFormattingState>(formattingState))
        invalidationRoot = InlineInvalidation::invalidate(layoutBox, styleDiff, *this, downcast<InlineFormattingState>(formattingState)).root;
    else
        ASSERT_NOT_REACHED();
    ASSERT(invalidationRoot);
    m_formattingContextRootListForLayout.addVoid(invalidationRoot);
}

void LayoutContext::markNeedsUpdate(const Box&, OptionSet<UpdateType>)
{
}

FormattingState& LayoutContext::formattingStateForBox(const Box& layoutBox) const
{
    auto& root = layoutBox.formattingContextRoot();
    RELEASE_ASSERT(m_formattingStates.contains(&root));
    return *m_formattingStates.get(&root);
}

FormattingState& LayoutContext::establishedFormattingState(const Box& formattingContextRoot, const FormattingContext& context)
{
    return *m_formattingStates.ensure(&formattingContextRoot, [this, &context] {
        return context.createFormattingState(context.createOrFindFloatingState(*this));
    }).iterator->value;
}

std::unique_ptr<FormattingContext> LayoutContext::formattingContext(const Box& formattingContextRoot)
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
