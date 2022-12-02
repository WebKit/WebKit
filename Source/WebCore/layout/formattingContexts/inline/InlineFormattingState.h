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

#include "FormattingState.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayLine.h"
#include "InlineItem.h"
#include "InlineLineBox.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

using InlineItems = Vector<InlineItem>;
using InlineLineBoxes = Vector<LineBox>;
using DisplayLines = Vector<InlineDisplay::Line>;
using DisplayBoxes = Vector<InlineDisplay::Box>;

// InlineFormattingState holds the state for a particular inline formatting context tree.
class InlineFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingState);
public:
    InlineFormattingState(LayoutState&);
    ~InlineFormattingState();

    InlineItems& inlineItems() { return m_inlineItems; }
    const InlineItems& inlineItems() const { return m_inlineItems; }
    void addInlineItems(InlineItems&& inlineItems) { m_inlineItems.appendVector(WTFMove(inlineItems)); }

    const InlineLineBoxes& lineBoxes() const { return m_lineBoxes; }
    void addLineBox(LineBox&& lineBox) { m_lineBoxes.append(WTFMove(lineBox)); }

    const DisplayLines& lines() const { return m_displayLines; }
    DisplayLines& lines() { return m_displayLines; }
    void addLine(const InlineDisplay::Line& line) { m_displayLines.append(line); }

    const DisplayBoxes& boxes() const { return m_displayBoxes; }
    DisplayBoxes& boxes() { return m_displayBoxes; }
    void addBoxes(DisplayBoxes&& boxes) { m_displayBoxes.appendVector(WTFMove(boxes)); }

    void setClearGapAfterLastLine(InlineLayoutUnit verticalGap);
    InlineLayoutUnit clearGapAfterLastLine() const { return m_clearGapAfterLastLine; }

    void clearInlineItems() { m_inlineItems.clear(); }
    void clearLineAndBoxes();
    void shrinkToFit();

private:
    // Cacheable input to line layout.
    InlineItems m_inlineItems;
    InlineLineBoxes m_lineBoxes;
    DisplayLines m_displayLines;
    DisplayBoxes m_displayBoxes;
    InlineLayoutUnit m_clearGapAfterLastLine { 0 };
};

inline void InlineFormattingState::setClearGapAfterLastLine(InlineLayoutUnit verticalGap)
{
    ASSERT(verticalGap >= 0);
    m_clearGapAfterLastLine = verticalGap;
}

inline void InlineFormattingState::clearLineAndBoxes()
{
    m_lineBoxes.clear();
    m_displayBoxes.clear();
    m_displayLines.clear();
    m_clearGapAfterLastLine = { };
}

inline void InlineFormattingState::shrinkToFit()
{
    m_inlineItems.shrinkToFit();
    m_displayLines.shrinkToFit();
    m_lineBoxes.shrinkToFit();
    m_displayBoxes.shrinkToFit();
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

