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
#include "LayoutUnit.h"
#include <wtf/IndexedRange.h>

namespace WebCore {
namespace Layout {

// https://drafts.csswg.org/css-grid-1/#gutters
// Geometry of a single grid line. A grid with N tracks has N + 1 grid lines. Each
// line models the gutter region at that line: [start, end] runs from the trailing
// edge of the preceding track to the leading edge of the following track (which is
// also where a grid item placed against this line begins). The width of the gutter
// at this line is therefore end - start. The first and last lines have no gutter, so
// start == end for them.
struct GridLineGeometry {
    LayoutUnit start;
    LayoutUnit end;

    static GridLineGeometryList listFromTrackSizes(const TrackSizes& trackSizes, LayoutUnit gap)
    {
        GridLineGeometryList lines;
        lines.reserveInitialCapacity(trackSizes.size() + 1);

        auto position = 0_lu;
        lines.append({ position, position });
        for (auto [trackIndex, trackSize] : WTF::indexedRange(trackSizes)) {
            position += trackSize;
            // The gutter follows every track except the last.
            auto gutter = trackIndex == trackSizes.size() - 1 ? 0_lu : gap;
            lines.append({ position, position + gutter });
            position += gutter;
        }
        return lines;
    }

    // Size of the track that starts at line trackIndex, i.e. the space between the end
    // of that line's gutter and the start of the next line's gutter.
    static LayoutUnit trackSize(const GridLineGeometryList& lines, size_t trackIndex)
    {
        return lines[trackIndex + 1].start - lines[trackIndex].end;
    }

    // The bare size of every track (excluding gutters).
    static TrackSizes trackSizes(const GridLineGeometryList& lines)
    {
        TrackSizes sizes;
        sizes.reserveInitialCapacity(trackCount(lines));
        for (size_t trackIndex = 0; trackIndex < trackCount(lines); ++trackIndex)
            sizes.append(trackSize(lines, trackIndex));
        return sizes;
    }

    // Size of a grid area spanning from startLine to endLine, including the interior
    // gutters between those lines but excluding the gutters outside the area.
    static LayoutUnit areaDimensionSize(const GridLineGeometryList& lines, size_t startLine, size_t endLine)
    {
        ASSERT(endLine > startLine);
        return lines[endLine].start - lines[startLine].end;
    }

    // Number of tracks represented by a set of grid lines (one fewer than the number of lines).
    static size_t trackCount(const GridLineGeometryList& lines)
    {
        ASSERT(!lines.isEmpty());
        return lines.size() - 1;
    }
};

} // namespace Layout
} // namespace WebCore
