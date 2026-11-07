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

#include "GridTypeAliases.h"
#include <WebCore/LayoutUnit.h>
#include <wtf/BitVector.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

// The positions of one axis' grid lines, relative to the start edge of the grid container's content box.
// https://drafts.csswg.org/css-grid-1/#gutters
// Every line is two positions: the end edge of the track before it and the start edge of the track after it, which are one gutter apart.
// This is the only place that decides where gutters are.
//
// FIXME: Content distribution (align-content / justify-content, https://drafts.csswg.org/css-align-3/#distribution-grid) plugs in here as
// constructor input: a position offset applied before the first track, and a distribution offset added to every gutter that exists, once
// per collapsed run (collapsed gutters are one distribution opportunity).
class GridLinePositions {
public:
    GridLinePositions(const TrackSizes&, const BitVector& collapsedTracks, LayoutUnit gap);

    size_t trackCount() const { return m_trackStartPositions.size(); }
    // Where an item whose start line is `lineIndex` begins. `lineIndex == trackCount()` is the end edge of the grid.
    LayoutUnit positionAsStartLine(size_t lineIndex) const;
    // Where an item whose end line is `lineIndex` ends. `lineIndex == 0` is the start edge of the grid.
    LayoutUnit positionAsEndLine(size_t lineIndex) const;
    // The size of the grid area between two lines, interior gutters included and the gutter after the last track not.
    LayoutUnit areaSize(size_t startLine, size_t endLine) const;

private:
    Vector<LayoutUnit> m_trackStartPositions;
    Vector<LayoutUnit> m_trackEndPositions;
};

} // namespace Layout
} // namespace WebCore
