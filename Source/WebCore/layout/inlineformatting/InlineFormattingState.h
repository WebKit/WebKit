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

#include "DisplayRun.h"
#include "FormattingState.h"
#include "InlineItem.h"
#include "InlineLineBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

using InlineItems = Vector<std::unique_ptr<InlineItem>, 30>;

// InlineFormattingState holds the state for a particular inline formatting context tree.
class InlineFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingState);
public:
    InlineFormattingState(Ref<FloatingState>&&, LayoutState&);
    virtual ~InlineFormattingState();

    InlineItems& inlineItems() { return m_inlineItems; }
    const InlineItems& inlineItems() const { return m_inlineItems; }
    void addInlineItem(std::unique_ptr<InlineItem>&& inlineItem) { m_inlineItems.append(WTFMove(inlineItem)); }

    using DisplayRuns = Vector<Display::Run, 10>;
    using LineBoxes = Vector<LineBox, 5>;

    const DisplayRuns& displayRuns() const { return m_displayRuns; }
    DisplayRuns& displayRuns() { return m_displayRuns; }
    void addDisplayRun(Display::Run&&);
    void resetDisplayRuns();

    const LineBoxes& lineBoxes() const { return m_lineBoxes; }
    LineBoxes& lineBoxes() { return m_lineBoxes; }
    void addLineBox(LineBox&& lineBox) { m_lineBoxes.append(lineBox); }

    const LineBox& lineBoxForRun(const Display::Run& displayRun) const { return m_lineBoxes[displayRun.lineIndex()]; }

private:
    // Cacheable input to line layout.
    InlineItems m_inlineItems;

    // Line layout results
    DisplayRuns m_displayRuns;
    LineBoxes m_lineBoxes;
};

inline void InlineFormattingState::addDisplayRun(Display::Run&& displayRun)
{
    m_displayRuns.append(WTFMove(displayRun));
}

inline void InlineFormattingState::resetDisplayRuns()
{
    m_displayRuns.clear();
    // Resetting the runs means no more line boxes either.
    m_lineBoxes.clear();

}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

#endif
