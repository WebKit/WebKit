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
#include "NamedGridLines.h"

#include "StyleGridTemplateList.h"
#include <algorithm>

namespace WebCore {
namespace Layout {

NamedGridLines::NamedGridLines(NamedLines&& namedLines)
    : m_namedLines(WTF::move(namedLines))
{
    for (auto& [lineName, lineIndexes] : m_namedLines) {
        std::ranges::sort(lineIndexes);
        lineIndexes.shrink(std::ranges::unique(lineIndexes).begin() - lineIndexes.begin());
    }
}

std::span<const size_t> NamedGridLines::lines(const Style::CustomIdent& lineName) const
{
    auto iterator = m_namedLines.find(lineName);
    if (iterator == m_namedLines.end())
        return { };
    return iterator->value.span();
}

// https://drafts.csswg.org/css-grid-1/#overlarge-grids
// "UAs may clamp the possible size of the implicit grid to be within a UA-defined limit, dropping all lines outside that
// limit." The lines of the tracks that were cut off do not exist, nor do their names.
static void addLineName(NamedGridLines::NamedLines& namedLines, const Style::CustomIdent& lineName, size_t lineIndex, size_t explicitTracksCount)
{
    if (lineIndex <= explicitTracksCount)
        namedLines.add(lineName, Vector<size_t> { }).iterator->value.append(lineIndex);
}

// https://drafts.csswg.org/css-grid-1/#repeat-notation
// Each repetition names its own lines, so the line where two repetitions meet carries the names of both. A repetition is
// added when its first line exists, which includes a partial last one, or one starting on the last line, when the limit cut it.
static void addAutoRepeatLineNames(NamedGridLines::NamedLines& namedLines, const Style::GridTemplateList& gridTemplateList, size_t autoRepeatTracksCount, bool autoRepeatTruncatedByLimit, size_t explicitTracksCount)
{
    auto& autoRepeatSizes = gridTemplateList.autoRepeatSizes;
    if (autoRepeatSizes.isEmpty() || gridTemplateList.autoRepeatNamedLines.map.isEmpty())
        return;
    ASSERT(autoRepeatTruncatedByLimit || !(autoRepeatTracksCount % autoRepeatSizes.size()));
    auto repetitionsCount = autoRepeatTracksCount / autoRepeatSizes.size() + (autoRepeatTruncatedByLimit ? 1 : 0);
    for (auto repetition : std::views::iota(0uz, repetitionsCount)) {
        auto repetitionFirstLine = gridTemplateList.autoRepeatInsertionPoint + repetition * autoRepeatSizes.size();
        for (auto& [lineName, lineIndexes] : gridTemplateList.autoRepeatNamedLines.map) {
            for (auto lineIndex : lineIndexes)
                addLineName(namedLines, lineName, repetitionFirstLine + lineIndex, explicitTracksCount);
        }
    }
}

// https://drafts.csswg.org/css-grid-1/#named-lines
// "Lines are named by adding a <custom-ident> in square brackets around the line's name" and a line can have several names.
//
// Style numbers the lines of a non-subgrid template like this (see the GridTemplateList constructor):
// - namedLines: a name's line number is the count of tracks before it, where the whole auto-repeat entry counts as one track
//   (repeat(N, ...) is already expanded). The names before the auto-repeat entry are numbered autoRepeatInsertionPoint at
//   most, and the names after it autoRepeatInsertionPoint + 1 at least. The auto-repeat's own tracks are not in "sizes".
// - autoRepeatNamedLines: a name's line number is the count of tracks before it inside the auto-repeat entry, from 0 to
//   autoRepeatSizes.size().
// A line after the auto-repeat entry is therefore at (style's number - 1 + the auto-repeated tracks count) in the explicit grid.
// That count is the one ExplicitGridResolver capped to the limit, so nothing here multiplies the repetition count (which can be in the
// millions) by the tracks per repetition, and nothing can wrap on a 32-bit size_t.
// The line numbers are 0-based, so they are not shifted into the 1-based ones of grid-row-start and friends.
NamedGridLines NamedGridLines::create(const Style::GridTemplateList& gridTemplateList, size_t autoRepeatTracksCount, bool autoRepeatTruncatedByLimit, size_t explicitTracksCount)
{
    bool hasAutoRepeatEntry = gridTemplateList.autoRepeatType != AutoRepeatType::None;
    size_t insertionPoint = gridTemplateList.autoRepeatInsertionPoint;

    NamedLines namedLines;
    for (auto& [lineName, lineIndexes] : gridTemplateList.namedLines.map) {
        for (auto lineIndex : lineIndexes) {
            if (!hasAutoRepeatEntry || lineIndex <= insertionPoint)
                addLineName(namedLines, lineName, lineIndex, explicitTracksCount);
            else if (!autoRepeatTruncatedByLimit) {
                // The line after the auto-repeat entry is past the limit when the limit cut the auto-repeat, and the capped count
                // would put it on the last line instead.
                addLineName(namedLines, lineName, lineIndex - 1 + autoRepeatTracksCount, explicitTracksCount);
            }
        }
    }

    addAutoRepeatLineNames(namedLines, gridTemplateList, autoRepeatTracksCount, autoRepeatTruncatedByLimit, explicitTracksCount);
    return NamedGridLines { WTF::move(namedLines) };
}

} // namespace Layout
} // namespace WebCore
