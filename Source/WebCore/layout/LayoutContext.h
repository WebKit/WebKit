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

#include "LayoutBox.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Display {
class Box;
}

namespace Layout {

class FormattingContext;
class FormattingState;
class StyleDiff;

// LayoutContext is the entry point for layout. It takes a (formatting root)container which acts as the root of the layout context.
// LayoutContext::layout() generates the display tree for the root container's subtree (it does not run layout on the root though).
// Note, while the root container is suppposed to be the entry point for the initial layout, it does not necessarily need to be the entry point of any
// subsequent layouts (subtree layout).
// LayoutContext also holds the formatting states. They cache formatting context specific data to enable performant incremental layouts.
class LayoutContext {
    WTF_MAKE_ISO_ALLOCATED(LayoutContext);
public:
    LayoutContext(const Box& root);

    void updateLayout();

    void addDisplayBox(const Box&, Display::Box&);
    Display::Box* displayBox(const Box&) const;

    void markNeedsLayout(const Box&, StyleDiff);
    bool needsLayout(const Box&) const;

private:
    FormattingState& formattingState(const FormattingContext&);
    std::unique_ptr<FormattingContext> formattingContext(const Box& formattingContextRoot);

    WeakPtr<Box> m_root;
    HashMap<const Box*, std::unique_ptr<FormattingState>> m_formattingStates;
};

}
}
#endif
