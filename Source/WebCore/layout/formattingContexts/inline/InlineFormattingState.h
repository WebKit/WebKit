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
    void setInlineItems(InlineItems&& inlineItems) { m_inlineItems = WTFMove(inlineItems); }
    void appendInlineItems(InlineItems&& inlineItems) { m_inlineItems.appendVector(WTFMove(inlineItems)); }

    const DisplayLines& lines() const { return m_displayLines; }
    DisplayLines& lines() { return m_displayLines; }
    void addLine(const InlineDisplay::Line& line) { m_displayLines.append(line); }

    const DisplayBoxes& boxes() const { return m_displayBoxes; }
    DisplayBoxes& boxes() { return m_displayBoxes; }
    void addBoxes(DisplayBoxes&& boxes) { m_displayBoxes.appendVector(WTFMove(boxes)); }

    void setClearGapAfterLastLine(InlineLayoutUnit verticalGap);
    InlineLayoutUnit clearGapAfterLastLine() const { return m_clearGapAfterLastLine; }

    void setClearGapBeforeFirstLine(InlineLayoutUnit verticalGap) { m_clearGapBeforeFirstLine = verticalGap; }
    InlineLayoutUnit clearGapBeforeFirstLine() const { return m_clearGapBeforeFirstLine; }

    void clearInlineItems() { m_inlineItems.clear(); }
    void shrinkToFit();

    void addNestedListMarkerOffset(const ElementBox& listMarkerBox, LayoutUnit offset) { m_nestedListMarkerOffset.add(&listMarkerBox, offset); }
    LayoutUnit nestedListMarkerOffset(const ElementBox& listMarkerBox) const { return m_nestedListMarkerOffset.get(&listMarkerBox); }
    void resetNestedListMarkerOffsets() { return m_nestedListMarkerOffset.clear(); }

private:
    // Cacheable input to line layout.
    InlineItems m_inlineItems;
    DisplayLines m_displayLines;
    DisplayBoxes m_displayBoxes;
    // FIXME: This should be part of a non-persistent formatting state.
    HashMap<const ElementBox*, LayoutUnit> m_nestedListMarkerOffset;
    InlineLayoutUnit m_clearGapBeforeFirstLine { 0 };
    InlineLayoutUnit m_clearGapAfterLastLine { 0 };
};

inline void InlineFormattingState::setClearGapAfterLastLine(InlineLayoutUnit verticalGap)
{
    ASSERT(verticalGap >= 0);
    m_clearGapAfterLastLine = verticalGap;
}

inline void InlineFormattingState::shrinkToFit()
{
    m_inlineItems.shrinkToFit();
    m_displayLines.shrinkToFit();
    m_displayBoxes.shrinkToFit();
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

