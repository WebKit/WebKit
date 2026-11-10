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

#include "config.h"
#include "NamedLineResolver.h"

#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Layout {

NamedLineResolver::NamedLineResolver(const NamedGridLines& namedGridLines, size_t explicitTrackCount)
    : m_namedGridLines(namedGridLines)
    , m_explicitTrackCount(explicitTrackCount)
{
}

int NamedLineResolver::nthNamedLine(const Style::CustomIdent& lineName, int n) const
{
    ASSERT(n);
    // https://drafts.csswg.org/css-grid-1/#grid-placement-int
    // "If a name is given as a <custom-ident>, only lines with that name are counted. If not enough lines with that name exist,
    // all implicit grid lines are assumed to have that name for the purpose of finding this position."
    // The Nth line with the name is the Nth entry of its lines (from the end for a negative N). Past the last one, the lines
    // after the end of the explicit grid (or before its start) count, one per line. Style clamps line numbers to +-maxTracks,
    // and the lines and the explicit track count are limited by maxTracks as well, so none of this overflows an int.
    auto linesWithName = m_namedGridLines.lines(lineName);
    if (n > 0) {
        auto count = static_cast<size_t>(n);
        if (count <= linesWithName.size())
            return static_cast<int>(linesWithName[count - 1]);
        return static_cast<int>(m_explicitTrackCount + (count - linesWithName.size()));
    }
    auto count = static_cast<size_t>(-static_cast<int64_t>(n));
    if (count <= linesWithName.size())
        return static_cast<int>(linesWithName[linesWithName.size() - count]);
    return -static_cast<int>(count - linesWithName.size());
}

int NamedLineResolver::namedAreaEdgeLine(const Style::CustomIdent& areaName, GridEdge edge) const
{
    // https://drafts.csswg.org/css-grid-1/#grid-placement-slot
    // "First attempt to match the grid area's edge to a named grid area: if there is a grid line whose line name is
    // <custom-ident>-start (for grid-*-start) / <custom-ident>-end (for grid-*-end), contributes the first such line to the grid
    // item's placement."
    auto linesWithEdgeName = m_namedGridLines.lines({ makeAtomString(areaName.value, edge == GridEdge::Start ? "-start"_s : "-end"_s) });
    if (!linesWithEdgeName.empty())
        return static_cast<int>(linesWithEdgeName.front());

    // "Otherwise, treat this as if the integer 1 had been specified along with the <custom-ident>."
    return nthNamedLine(areaName, 1);
}

int NamedLineResolver::namedSpanLine(const Style::CustomIdent& lineName, unsigned span, int oppositeLine, GridEdge edge) const
{
    ASSERT(span);
    // https://drafts.csswg.org/css-grid-1/#grid-placement-span-int
    // "Contributes a grid span to the grid item's placement such that the corresponding edge of the grid item's grid area is N
    // lines from its opposite edge in the corresponding direction. [...] If a name is given as a <custom-ident>, only lines
    // with that name are counted. If not enough lines with that name exist, all implicit grid lines on the side of the explicit
    // grid corresponding to the search direction are assumed to have that name for the purpose of counting this span."
    // The lines with the name are sorted, so the lines past the opposite line are one end of the vector. When the span needs
    // more lines than that, the rest are implicit lines on the side of the explicit grid being searched, one per line, counted
    // from the explicit grid's edge or from the opposite line, whichever is further along. Style clamps the span to the track
    // limit, which the lines and the explicit track count are limited by as well, so none of this overflows an int.
    // For example, with `grid-template-columns: [foo] 1fr 1fr [foo] 1fr` (named lines 0 and 2, three tracks):
    //     span foo / 4 (end line 3): two named lines before it (2 and 0), so span 1 is line 2, span 2 is line 0 and span 3 is the
    //     leading implicit line -1.
    //     3 / span foo (start line 2): no named line after it, so span 1 is the implicit line 4 (after the explicit end at 3).
    auto linesWithName = m_namedGridLines.lines(lineName);
    if (edge == GridEdge::End) {
        // A negative opposite line is before every named line.
        auto firstAfter = linesWithName.begin();
        if (oppositeLine >= 0)
            firstAfter = std::upper_bound(linesWithName.begin(), linesWithName.end(), static_cast<size_t>(oppositeLine));
        auto count = static_cast<size_t>(linesWithName.end() - firstAfter);
        if (span <= count)
            return static_cast<int>(firstAfter[span - 1]);
        return std::max(static_cast<int>(m_explicitTrackCount), oppositeLine) + static_cast<int>(span - count);
    }

    auto firstNotBefore = linesWithName.begin();
    if (oppositeLine > 0)
        firstNotBefore = std::lower_bound(linesWithName.begin(), linesWithName.end(), static_cast<size_t>(oppositeLine));
    auto count = static_cast<size_t>(firstNotBefore - linesWithName.begin());
    if (span <= count)
        return static_cast<int>(firstNotBefore[-static_cast<ptrdiff_t>(span)]);
    return std::min(0, oppositeLine) - static_cast<int>(span - count);
}

} // namespace Layout
} // namespace WebCore
