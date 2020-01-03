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

#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutState);

LayoutState::LayoutState(const LayoutTreeContent& layoutTreeContent)
    : m_layoutTreeContent(makeWeakPtr(layoutTreeContent))
{
    // It makes absolutely no sense to construct a dedicated layout state for a non-formatting context root (layout would be a no-op).
    ASSERT(m_layoutTreeContent->rootLayoutBox().establishesFormattingContext());

    auto quirksMode = [&] {
        auto& document = m_layoutTreeContent->rootRenderer().document();
        if (document.inLimitedQuirksMode())
            return LayoutState::QuirksMode::Limited;
        if (document.inQuirksMode())
            return LayoutState::QuirksMode::Yes;
        return LayoutState::QuirksMode::No;
    };
    setQuirksMode(quirksMode());
}

LayoutState::~LayoutState() = default;

Display::Box& LayoutState::displayBoxForRootLayoutBox()
{
    return displayBoxForLayoutBox(m_layoutTreeContent->rootLayoutBox());
}

Display::Box& LayoutState::displayBoxForLayoutBox(const Box& layoutBox)
{
    return *m_layoutToDisplayBox.ensure(&layoutBox, [] {
        return makeUnique<Display::Box>();
    }).iterator->value;
}

const Display::Box& LayoutState::displayBoxForLayoutBox(const Box& layoutBox) const
{
    ASSERT(hasDisplayBox(layoutBox));
    return *m_layoutToDisplayBox.get(&layoutBox);
}

FormattingState& LayoutState::formattingStateForBox(const Box& layoutBox) const
{
    auto& root = layoutBox.formattingContextRoot();
    RELEASE_ASSERT(m_formattingStates.contains(&root));
    return *m_formattingStates.get(&root);
}

FormattingState& LayoutState::establishedFormattingState(const Container& formattingRoot) const
{
    ASSERT(formattingRoot.establishesFormattingContext());
    RELEASE_ASSERT(m_formattingStates.contains(&formattingRoot));
    return *m_formattingStates.get(&formattingRoot);
}

FormattingState& LayoutState::createFormattingStateForFormattingRootIfNeeded(const Container& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesFormattingContext());

    if (formattingContextRoot.establishesInlineFormattingContext()) {
        return *m_formattingStates.ensure(&formattingContextRoot, [&] {

            // If the block container box that initiates this inline formatting context also establishes a block context, the floats outside of the formatting root
            // should not interfere with the content inside.
            // <div style="float: left"></div><div style="overflow: hidden"> <- is a non-intrusive float, because overflow: hidden triggers new block formatting context.</div>
            if (formattingContextRoot.establishesBlockFormattingContext())
                return makeUnique<InlineFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);

            // Otherwise, the formatting context inherits the floats from the parent formatting context.
            // Find the formatting state in which this formatting root lives, not the one it creates and use its floating state.
            auto& parentFormattingState = createFormattingStateForFormattingRootIfNeeded(formattingContextRoot.formattingContextRoot()); 
            auto& parentFloatingState = parentFormattingState.floatingState();
            return makeUnique<InlineFormattingState>(parentFloatingState, *this);
        }).iterator->value;
    }

    if (formattingContextRoot.establishesBlockFormattingContext()) {
        return *m_formattingStates.ensure(&formattingContextRoot, [&] {

            // Block formatting context always establishes a new floating state.
            return makeUnique<BlockFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);
        }).iterator->value;
    }

    if (formattingContextRoot.establishesTableFormattingContext()) {
        return *m_formattingStates.ensure(&formattingContextRoot, [&] {

            // Table formatting context always establishes a new floating state -and it stays empty.
            return makeUnique<TableFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);
        }).iterator->value;
    }

    CRASH();
}

void LayoutState::setViewportSize(const LayoutSize& viewportSize)
{
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        m_viewportSize = viewportSize;
        return;
    }
    ASSERT_NOT_REACHED();
}

LayoutSize LayoutState::viewportSize() const
{
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return m_viewportSize;
    ASSERT_NOT_REACHED();
    return { };
}

bool LayoutState::isIntegratedRootBoxFirstChild() const
{
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        auto& rootRenderer = m_layoutTreeContent->rootRenderer();
        ASSERT(rootRenderer.parent());
        return rootRenderer.parent()->firstChild() == &rootRenderer;
    }
    ASSERT_NOT_REACHED();
    return false;
}

}
}

#endif
