/*
 * Copyright (C) 2014-2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GridPosition.h"

namespace WebCore {

class GridSpan;
class RenderBox;
class RenderStyle;

enum GridTrackSizingDirection {
    ForColumns,
    ForRows
};

class NamedLineCollection {
    WTF_MAKE_NONCOPYABLE(NamedLineCollection);
public:
    NamedLineCollection(const RenderStyle&, const String& namedLine, GridTrackSizingDirection, unsigned lastLine, unsigned autoRepeatTracksCount);

    bool hasNamedLines() const;
    unsigned firstPosition() const;

    bool contains(unsigned line) const;

private:
    size_t find(unsigned line) const;

    const Vector<unsigned>* m_namedLinesIndexes { nullptr };
    const Vector<unsigned>* m_autoRepeatNamedLinesIndexes { nullptr };

    unsigned m_insertionPoint;
    unsigned m_lastLine;
    unsigned m_autoRepeatTotalTracks;
    unsigned m_autoRepeatTrackListLength;
};

// Class with all the code related to grid items positions resolution.
class GridPositionsResolver {
public:
    static GridPositionSide initialPositionSide(GridTrackSizingDirection);
    static GridPositionSide finalPositionSide(GridTrackSizingDirection);
    static unsigned spanSizeForAutoPlacedItem(const RenderBox&, GridTrackSizingDirection);
    static GridSpan resolveGridPositionsFromStyle(const RenderStyle&, const RenderBox&, GridTrackSizingDirection, unsigned autoRepeatTracksCount);
    static unsigned explicitGridColumnCount(const RenderStyle&, unsigned autoRepeatColumnsCount);
    static unsigned explicitGridRowCount(const RenderStyle&, unsigned autoRepeatRowsCount);
};

} // namespace WebCore
