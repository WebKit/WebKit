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

#include "InlineRunProvider.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InlineLineBreaker {
    WTF_MAKE_ISO_ALLOCATED(InlineLineBreaker);
public:
    InlineLineBreaker(const LayoutState&, const InlineContent&, const Vector<InlineRunProvider::Run>&);

    struct Run {
        enum class Position { Undetermined, LineBegin, LineEnd };
        Position position;
        LayoutUnit width;
        InlineRunProvider::Run content;
    };
    std::optional<Run> nextRun(LayoutUnit contentLogicalLeft, LayoutUnit availableWidth, bool lineIsEmpty);

private:
    enum class LineBreakingBehavior { Keep, Break, WrapToNextLine };
    LineBreakingBehavior lineBreakingBehavior(const InlineRunProvider::Run&, bool lineIsEmpty);
    bool isAtContentEnd() const;
    Run splitRun(const InlineRunProvider::Run&, LayoutUnit contentLogicalLeft, LayoutUnit availableWidth, bool lineIsEmpty);
    LayoutUnit runWidth(const InlineRunProvider::Run&, LayoutUnit contentLogicalLeft) const;
    LayoutUnit textWidth(const InlineRunProvider::Run&, LayoutUnit contentLogicalLeft) const;
    std::optional<ItemPosition> adjustSplitPositionWithHyphenation(const InlineRunProvider::Run&, ItemPosition splitPosition, LayoutUnit contentLogicalLeft, LayoutUnit availableWidth, bool isLineEmpty) const;

    const LayoutState& m_layoutState;
    const InlineContent& m_inlineContent;
    const Vector<InlineRunProvider::Run>& m_inlineRuns;

    unsigned m_currentRunIndex { 0 };
    std::optional<ItemPosition> m_splitPosition;
    bool m_hyphenationIsDisabled { false };
};

}
}
#endif
