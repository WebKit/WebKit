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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutContainer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderView;

namespace Display {
class Box;
}

namespace Layout {

enum class StyleDiff;
class Box;
class FormattingContext;
class FormattingState;

// LayoutState is the entry point for layout. It takes the initial containing block which acts as the root of the layout context.
// LayoutState::layout() generates the display tree for the root container's subtree (it does not run layout on the root though).
// Note, while the initial containing block is entry point for the initial layout, it does not necessarily need to be the entry point of any
// subsequent layouts (subtree layout). A non-initial, subtree layout could be initiated on multiple formatting contexts.
// Each formatting context has an entry point for layout, which potenitally means multiple entry points per layout frame.
// LayoutState also holds the formatting states. They cache formatting context specific data to enable performant incremental layouts.
class LayoutState {
    WTF_MAKE_ISO_ALLOCATED(LayoutState);
public:
    LayoutState(const Container& initialContainingBlock);

    // FIXME: This is a temporary entry point for LFC layout.
    static void run(const RenderView&);

    void updateLayout();
    void styleChanged(const Box&, StyleDiff);
    enum class QuirksMode { No, Limited, Yes };
    void setQuirksMode(QuirksMode quirksMode) { m_quirksMode = quirksMode; }

    enum class UpdateType {
        Overflow = 1 << 0,
        Position = 1 << 1,
        Size     = 1 << 2,
        All      = Overflow | Position | Size
    };
    void markNeedsUpdate(const Box&, OptionSet<UpdateType>);
    bool needsUpdate(const Box&) const;

    FormattingState& formattingStateForBox(const Box&) const;
    FormattingState& establishedFormattingState(const Box& formattingRoot) const;
    bool hasFormattingState(const Box& formattingRoot) const { return m_formattingStates.contains(&formattingRoot); }
    FormattingState& createFormattingStateForFormattingRootIfNeeded(const Box& formattingRoot);

    std::unique_ptr<FormattingContext> createFormattingContext(const Box& formattingContextRoot);
#ifndef NDEBUG
    void registerFormattingContext(const FormattingContext&);
    void deregisterFormattingContext(const FormattingContext& formattingContext) { m_formattingContextList.remove(&formattingContext); }
#endif

    Display::Box& displayBoxForLayoutBox(const Box& layoutBox) const;
    bool hasDisplayBox(const Box& layoutBox) const { return m_layoutToDisplayBox.contains(&layoutBox); }

    bool inQuirksMode() const { return m_quirksMode == QuirksMode::Yes; }
    bool inLimitedQuirksMode() const { return m_quirksMode == QuirksMode::Limited; }
    bool inNoQuirksMode() const { return m_quirksMode == QuirksMode::No; }
    // For testing purposes only
    void verifyAndOutputMismatchingLayoutTree(const RenderView&) const;

private:
    const Container& initialContainingBlock() const { return *m_initialContainingBlock; }
    void layoutFormattingContextSubtree(const Box&);

    WeakPtr<const Container> m_initialContainingBlock;
    HashSet<const Container*> m_formattingContextRootListForLayout;
    HashMap<const Box*, std::unique_ptr<FormattingState>> m_formattingStates;
#ifndef NDEBUG
    HashSet<const FormattingContext*> m_formattingContextList;
#endif
    mutable HashMap<const Box*, std::unique_ptr<Display::Box>> m_layoutToDisplayBox;
    QuirksMode m_quirksMode { QuirksMode::No };
};

#ifndef NDEBUG
inline void LayoutState::registerFormattingContext(const FormattingContext& formattingContext)
{
    // Multiple formatting contexts of the same root within a layout frame indicates defective layout logic.
    ASSERT(!m_formattingContextList.contains(&formattingContext));
    m_formattingContextList.add(&formattingContext);
}
#endif

}
}
#endif
