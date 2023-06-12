/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "BlockLayoutState.h"
#include "FloatingState.h"

namespace WebCore {
namespace Layout {

class InlineLayoutState {
public:
    InlineLayoutState(BlockLayoutState&, HashMap<const ElementBox*, LayoutUnit>&& nestedListMarkerOffsets);

    void setClearGapAfterLastLine(InlineLayoutUnit verticalGap);
    InlineLayoutUnit clearGapAfterLastLine() const { return m_clearGapAfterLastLine; }

    void setClearGapBeforeFirstLine(InlineLayoutUnit verticalGap) { m_clearGapBeforeFirstLine = verticalGap; }
    InlineLayoutUnit clearGapBeforeFirstLine() const { return m_clearGapBeforeFirstLine; }

    const BlockLayoutState& parentBlockLayoutState() const { return m_parentBlockLayoutState; }
    BlockLayoutState& parentBlockLayoutState() { return m_parentBlockLayoutState; }

    LayoutUnit nestedListMarkerOffset(const ElementBox& listMarkerBox) const { return m_nestedListMarkerOffsets.get(&listMarkerBox); }

private:
    BlockLayoutState& m_parentBlockLayoutState;
    InlineLayoutUnit m_clearGapBeforeFirstLine { 0.f };
    InlineLayoutUnit m_clearGapAfterLastLine { 0.f };
    // FIXME: This is required by the integaration codepath.
    HashMap<const ElementBox*, LayoutUnit> m_nestedListMarkerOffsets;
};

inline InlineLayoutState::InlineLayoutState(BlockLayoutState& parentBlockLayoutState, HashMap<const ElementBox*, LayoutUnit>&& nestedListMarkerOffsets)
    : m_parentBlockLayoutState(parentBlockLayoutState)
    , m_nestedListMarkerOffsets(WTFMove(nestedListMarkerOffsets))
{
}

inline void InlineLayoutState::setClearGapAfterLastLine(InlineLayoutUnit verticalGap)
{
    ASSERT(verticalGap >= 0);
    m_clearGapAfterLastLine = verticalGap;
}

}
}
