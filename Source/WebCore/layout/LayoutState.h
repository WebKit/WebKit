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
#include "LayoutTreeBuilder.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Display {
class Box;
}

namespace Layout {

class Box;
class FormattingContext;
class FormattingState;

class LayoutState {
    WTF_MAKE_ISO_ALLOCATED(LayoutState);
public:
    LayoutState(const LayoutTreeContent&);
    ~LayoutState();

    FormattingState& createFormattingStateForFormattingRootIfNeeded(const Container& formattingContextRoot);
    FormattingState& establishedFormattingState(const Container& formattingRoot) const;
    FormattingState& formattingStateForBox(const Box&) const;
    bool hasFormattingState(const Container& formattingRoot) const { return m_formattingStates.contains(&formattingRoot); }

#ifndef NDEBUG
    void registerFormattingContext(const FormattingContext&);
    void deregisterFormattingContext(const FormattingContext& formattingContext) { m_formattingContextList.remove(&formattingContext); }
#endif

    Display::Box& displayBoxForRootLayoutBox();
    Display::Box& displayBoxForLayoutBox(const Box& layoutBox);
    const Display::Box& displayBoxForLayoutBox(const Box& layoutBox) const;
    bool hasDisplayBox(const Box& layoutBox) const { return m_layoutToDisplayBox.contains(&layoutBox); }

    enum class QuirksMode { No, Limited, Yes };
    bool inQuirksMode() const { return m_quirksMode == QuirksMode::Yes; }
    bool inLimitedQuirksMode() const { return m_quirksMode == QuirksMode::Limited; }
    bool inNoQuirksMode() const { return m_quirksMode == QuirksMode::No; }

    const Container& root() const { return m_layoutTreeContent->rootLayoutBox(); }
#ifndef NDEBUG
    const RenderBox& rootRenderer() const { return m_layoutTreeContent->rootRenderer(); }
#endif

    // LFC integration only. Full LFC has proper ICB access.
    void setViewportSize(const LayoutSize&);
    LayoutSize viewportSize() const;
    bool isIntegratedRootBoxFirstChild() const;

private:
    void setQuirksMode(QuirksMode quirksMode) { m_quirksMode = quirksMode; }

    HashMap<const Container*, std::unique_ptr<FormattingState>> m_formattingStates;
#ifndef NDEBUG
    HashSet<const FormattingContext*> m_formattingContextList;
#endif
    HashMap<const Box*, std::unique_ptr<Display::Box>> m_layoutToDisplayBox;
    QuirksMode m_quirksMode { QuirksMode::No };

    WeakPtr<const LayoutTreeContent> m_layoutTreeContent;
    // LFC integration only.
    LayoutSize m_viewportSize;
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
