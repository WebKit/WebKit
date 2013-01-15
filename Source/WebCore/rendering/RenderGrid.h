/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderGrid_h
#define RenderGrid_h

#include "RenderBlock.h"

namespace WebCore {

class GridTrack;

class RenderGrid : public RenderBlock {
public:
    RenderGrid(Node*);
    virtual ~RenderGrid();

    virtual const char* renderName() const OVERRIDE;

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) OVERRIDE;

    virtual bool avoidsFloats() const OVERRIDE { return true; }
    virtual bool canCollapseAnonymousBlockChild() const OVERRIDE { return false; }

private:
    virtual bool isRenderGrid() const OVERRIDE { return true; }
    virtual void computePreferredLogicalWidths() OVERRIDE;

    enum TrackSizingDirection { ForColumns, ForRows };
    void computedUsedBreadthOfGridTracks(TrackSizingDirection, Vector<GridTrack>&);
    LayoutUnit computeUsedBreadthOfLength(TrackSizingDirection, const Length&);
    void distributeSpaceToTracks(TrackSizingDirection, Vector<GridTrack>&, LayoutUnit availableLogicalSpace);
    void layoutGridItems();

    LayoutPoint findChildLogicalPosition(RenderBox*, const Vector<GridTrack>& columnTracks, const Vector<GridTrack>& rowTracks);
    size_t resolveGridPosition(const GridPosition&) const;
};

} // namespace WebCore

#endif // RenderGrid_h
