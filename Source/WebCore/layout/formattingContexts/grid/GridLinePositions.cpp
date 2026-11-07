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
#include "GridLinePositions.h"

#include <wtf/IndexedRange.h>

namespace WebCore {
namespace Layout {

GridLinePositions::GridLinePositions(const TrackSizes& trackSizes, const BitVector& collapsedTracks, LayoutUnit gap)
{
    m_trackStartPositions.reserveInitialCapacity(trackSizes.size());
    m_trackEndPositions.reserveInitialCapacity(trackSizes.size());

    auto nonCollapsedTracksRemaining = trackSizes.size() - collapsedTracks.bitCount();
    LayoutUnit position;
    for (auto [trackIndex, trackSize] : WTF::indexedRange(trackSizes)) {
        m_trackStartPositions.append(position);
        position += trackSize;
        m_trackEndPositions.append(position);

        // https://drafts.csswg.org/css-grid-1/#gutters
        // https://drafts.csswg.org/css-grid-1/#collapsed-gutter
        // A gutter follows a track unless the track is collapsed. The gutters on both sides of a collapsed track coincide, so a run of
        // collapsed tracks adds one gutter, and no gutter is left after the last non-collapsed track.
        if (!collapsedTracks.get(trackIndex) && --nonCollapsedTracksRemaining)
            position += gap;
    }
}

LayoutUnit GridLinePositions::positionAsStartLine(size_t lineIndex) const
{
    ASSERT(lineIndex <= trackCount());
    if (lineIndex < trackCount())
        return m_trackStartPositions[lineIndex];
    return trackCount() ? m_trackEndPositions.last() : LayoutUnit { };
}

LayoutUnit GridLinePositions::positionAsEndLine(size_t lineIndex) const
{
    ASSERT(lineIndex <= trackCount());
    if (lineIndex)
        return m_trackEndPositions[lineIndex - 1];
    return trackCount() ? m_trackStartPositions.first() : LayoutUnit { };
}

LayoutUnit GridLinePositions::areaSize(size_t startLine, size_t endLine) const
{
    ASSERT(endLine > startLine);
    return positionAsEndLine(endLine) - positionAsStartLine(startLine);
}

} // namespace Layout
} // namespace WebCore
