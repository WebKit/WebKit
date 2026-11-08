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

} // namespace Layout
} // namespace WebCore
