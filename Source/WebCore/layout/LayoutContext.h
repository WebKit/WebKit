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
#include "LayoutState.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class RenderView;

namespace Layout {

enum class StyleDiff;
class Box;
class FormattingContext;

// LayoutContext is the entry point for layout.
// LayoutContext::layout() generates the display tree for the root container's subtree (it does not run layout on the root though).
// Note, while the initial containing block is entry point for the initial layout, it does not necessarily need to be the entry point of any
// subsequent layouts (subtree layout). A non-initial, subtree layout could be initiated on multiple formatting contexts.
// Each formatting context has an entry point for layout, which potenitally means multiple entry points per layout frame.
// LayoutState holds the formatting states. They cache formatting context specific data to enable performant incremental layouts.
class LayoutContext {
    WTF_MAKE_ISO_ALLOCATED(LayoutContext);
public:
    // FIXME: This is a temporary entry point for LFC layout.
    static void run(const RenderView&);

    LayoutContext(LayoutState&);
    void layout();

    enum class UpdateType {
        Overflow = 1 << 0,
        Position = 1 << 1,
        Size     = 1 << 2
    };
    static constexpr OptionSet<UpdateType> updateAll() { return { UpdateType::Overflow, UpdateType::Position, UpdateType::Size }; }
    void markNeedsUpdate(const Box&, OptionSet<UpdateType> = updateAll());
    bool needsUpdate(const Box&) const;

    void styleChanged(const Box&, StyleDiff);

    static std::unique_ptr<FormattingContext> createFormattingContext(const Container& formattingContextRoot, LayoutState&);

    // For testing purposes only
    static void verifyAndOutputMismatchingLayoutTree(const LayoutState&, const RenderView&, const Container& initialContainingBlock);

private:
    void layoutFormattingContextSubtree(const Container&);
    LayoutState& layoutState() { return m_layoutState; }

    LayoutState& m_layoutState;
    WeakHashSet<const Container> m_formattingContextRootListForLayout;
};

}
}
#endif
