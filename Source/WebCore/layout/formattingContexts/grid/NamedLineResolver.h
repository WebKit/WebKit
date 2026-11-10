/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "NamedGridLines.h"

namespace WebCore {
namespace Layout {

// Which edge of a grid area a grid-*-start (Start) or grid-*-end (End) refers to.
enum class GridEdge : bool { Start, End };

// The rules that find a line from a name in one axis (css-grid-1 8.3, https://drafts.csswg.org/css-grid-1/#line-placement).
// Lines are 0-based, like NamedGridLines, and a result can be negative or past the explicit grid, where the line is in the implicit grid.
// It holds the axis's NamedGridLines by reference, so it is only used during GridLayoutContextBuilder::build, next to the
// NamedGridLines it was made from. Copying is deleted so that no copy escapes, and the constructor refuses a temporary.
class NamedLineResolver {
public:
    NamedLineResolver(const NamedGridLines&, size_t explicitTrackCount);
    NamedLineResolver(NamedGridLines&&, size_t) = delete;
    NamedLineResolver(const NamedLineResolver&) = delete;
    NamedLineResolver& operator=(const NamedLineResolver&) = delete;

    // Also what numeric lines count against: line -1 is the last line of the explicit grid.
    NODELETE size_t explicitTrackCount() const { return m_explicitTrackCount; }

    // The line of grid-*-start / grid-*-end given as an <integer> (not zero, negative to count from the end) and a <custom-ident>.
    // https://drafts.csswg.org/css-grid-1/#grid-placement-int
    NODELETE int nthNamedLine(const Style::CustomIdent&, int) const;

    // The line of grid-*-start / grid-*-end given as a lone <custom-ident>.
    // https://drafts.csswg.org/css-grid-1/#grid-placement-slot
    int namedAreaEdgeLine(const Style::CustomIdent&, GridEdge) const;

    // The line of grid-*-start / grid-*-end given as "span <integer>? <custom-ident>", which is `span` lines away from the opposite
    // edge, counting only the lines with the name. https://drafts.csswg.org/css-grid-1/#grid-placement-span-int
    // The GridEdge is the edge being resolved, and `oppositeLine` is the line of the other edge: End searches endward from the start
    // line, Start searches startward from the end line. `span` is always positive (it is the span integer, 1 by default), so the
    // direction comes from the edge and not from a sign, unlike the N of nthNamedLine().
    int namedSpanLine(const Style::CustomIdent&, unsigned span, int oppositeLine, GridEdge) const;

private:
    const NamedGridLines& m_namedGridLines;
    const size_t m_explicitTrackCount;
};

} // namespace Layout
} // namespace WebCore
