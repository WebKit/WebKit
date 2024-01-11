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

#include "LayoutUnits.h"

namespace WebCore {
namespace Layout {

class InlineItem;

enum class LineEndingEllipsisPolicy : uint8_t {
    NoEllipsis,
    WhenContentOverflowsInInlineDirection,
    WhenContentOverflowsInBlockDirection,
    // FIXME: This should be used when we realize the last line of this IFC is where the content is truncated (sibling IFC has more lines).
    Always
};

struct ExpansionInfo {
    size_t opportunityCount { 0 };
    Vector<size_t> opportunityList;
    Vector<ExpansionBehavior> behaviorList;
};

struct InlineItemPosition {
    size_t index { 0 };
    size_t offset { 0 }; // Note that this is offset relative to the start position of the InlineItem.

    friend bool operator==(const InlineItemPosition&, const InlineItemPosition&) = default;
    operator bool() const { return index || offset; }
};

struct InlineItemRange {
    InlineItemRange(InlineItemPosition start, InlineItemPosition end);
    InlineItemRange(size_t startIndex, size_t endIndex);
    InlineItemRange() = default;

    size_t startIndex() const { return start.index; }
    size_t endIndex() const { return end.index; }
    bool isEmpty() const { return startIndex() == endIndex() && start.offset == end.offset; }

    InlineItemPosition start;
    InlineItemPosition end;
};

struct PreviousLine {
    size_t lineIndex { 0 };
    // Content width measured during line breaking (avoid double-measuring).
    std::optional<InlineLayoutUnit> trailingOverflowingContentWidth { };
    bool endsWithLineBreak { false };
    bool hasInlineContent { false };
    TextDirection inlineBaseDirection { TextDirection::LTR };
    Vector<const Box*> suspendedFloats;
};

inline InlineItemRange::InlineItemRange(InlineItemPosition start, InlineItemPosition end)
    : start(start)
    , end(end)
{
}

inline InlineItemRange::InlineItemRange(size_t startIndex, size_t endIndex)
    : start { startIndex, { } }
    , end { endIndex, { } }
{
}

}
}
