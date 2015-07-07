/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef GridResolvedPosition_h
#define GridResolvedPosition_h

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridPosition.h"

namespace WebCore {

class GridSpan;
class RenderBox;
class RenderStyle;

enum GridPositionSide {
    ColumnStartSide,
    ColumnEndSide,
    RowStartSide,
    RowEndSide
};

enum GridTrackSizingDirection {
    ForColumns,
    ForRows
};

// This class represents an index into one of the dimensions of the grid array.
// Wraps a size_t integer just for the purpose of knowing what we manipulate in the grid code.
class GridResolvedPosition {
public:
    static GridResolvedPosition adjustGridPositionForRowEndColumnEndSide(size_t resolvedPosition)
    {
        return resolvedPosition ? GridResolvedPosition(resolvedPosition - 1) : GridResolvedPosition(0);
    }

    static GridResolvedPosition adjustGridPositionForSide(size_t resolvedPosition, GridPositionSide side)
    {
        // An item finishing on the N-th line belongs to the N-1-th cell.
        if (side == ColumnEndSide || side == RowEndSide)
            return adjustGridPositionForRowEndColumnEndSide(resolvedPosition);

        return GridResolvedPosition(resolvedPosition);
    }

    static GridSpan resolveGridPositionsFromAutoPlacementPosition(const RenderStyle&, const RenderBox&, GridTrackSizingDirection, const GridResolvedPosition&);
    static void adjustNamedGridItemPosition(const RenderStyle&, GridPosition&, GridPositionSide);
    static void adjustGridPositionsFromStyle(const RenderStyle&, GridPosition& initialPosition, GridPosition& finalPosition, GridPositionSide initialPositionSide, GridPositionSide finalPositionSide);
    static std::unique_ptr<GridSpan> resolveGridPositionsFromStyle(const RenderStyle&, const RenderBox&, GridTrackSizingDirection);
    static GridResolvedPosition resolveNamedGridLinePositionFromStyle(const RenderStyle&, const GridPosition&, GridPositionSide);
    static GridResolvedPosition resolveGridPositionFromStyle(const RenderStyle&, const GridPosition&, GridPositionSide);
    static std::unique_ptr<GridSpan> resolveGridPositionAgainstOppositePosition(const RenderStyle&, const GridResolvedPosition&, const GridPosition&, GridPositionSide);
    static std::unique_ptr<GridSpan> resolveNamedGridLinePositionAgainstOppositePosition(const RenderStyle&, const GridResolvedPosition&, const GridPosition&, GridPositionSide);
    static std::unique_ptr<GridSpan> resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition&, const GridPosition&, const Vector<size_t>&);
    static std::unique_ptr<GridSpan> resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition&, const GridPosition&, const Vector<size_t>&);

    GridResolvedPosition(size_t position)
        : m_integerPosition(position)
    {
    }

    GridResolvedPosition(const GridPosition& position, GridPositionSide side)
    {
        ASSERT(position.integerPosition());
        size_t integerPosition = position.integerPosition() - 1;

        m_integerPosition = adjustGridPositionForSide(integerPosition, side).m_integerPosition;
    }

    GridResolvedPosition& operator*()
    {
        return *this;
    }

    GridResolvedPosition& operator++()
    {
        m_integerPosition++;
        return *this;
    }

    bool operator==(const GridResolvedPosition& other) const
    {
        return m_integerPosition == other.m_integerPosition;
    }

    bool operator!=(const GridResolvedPosition& other) const
    {
        return m_integerPosition != other.m_integerPosition;
    }

    bool operator<(const GridResolvedPosition& other) const
    {
        return m_integerPosition < other.m_integerPosition;
    }

    bool operator>(const GridResolvedPosition& other) const
    {
        return m_integerPosition > other.m_integerPosition;
    }

    bool operator<=(const GridResolvedPosition& other) const
    {
        return m_integerPosition <= other.m_integerPosition;
    }

    bool operator>=(const GridResolvedPosition& other) const
    {
        return m_integerPosition >= other.m_integerPosition;
    }

    size_t toInt() const
    {
        return m_integerPosition;
    }

    GridResolvedPosition next() const
    {
        return GridResolvedPosition(m_integerPosition + 1);
    }

private:

    size_t m_integerPosition;
};

} // namespace WebCore

#endif // ENABLE(CSS_GRID_LAYOUT)

#endif // GridResolvedPosition_h
