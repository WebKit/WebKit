/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "CaretRectComputation.h"
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "TextAffinity.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class LayoutUnit;
class Position;
class RenderObject;
class VisiblePosition;

class RenderedPosition {
public:
    RenderedPosition();
    explicit RenderedPosition(const VisiblePosition&);
    explicit RenderedPosition(const Position&, Affinity);
    bool isEquivalent(const RenderedPosition&) const;

    bool isNull() const { return !m_renderer; }
    InlineIterator::LineBoxIterator lineBox() const { return m_box ? m_box->lineBox() : InlineIterator::LineBoxIterator(); }

    unsigned char bidiLevelOnLeft() const;
    unsigned char bidiLevelOnRight() const;
    RenderedPosition leftBoundaryOfBidiRun(unsigned char bidiLevelOfRun);
    RenderedPosition rightBoundaryOfBidiRun(unsigned char bidiLevelOfRun);

    enum ShouldMatchBidiLevel { MatchBidiLevel, IgnoreBidiLevel };
    bool atLeftBoundaryOfBidiRun() const { return atLeftBoundaryOfBidiRun(IgnoreBidiLevel, 0); }
    bool atRightBoundaryOfBidiRun() const { return atRightBoundaryOfBidiRun(IgnoreBidiLevel, 0); }
    // The following two functions return true only if the current position is at the end of the bidi run
    // of the specified bidi embedding level.
    bool atLeftBoundaryOfBidiRun(unsigned char bidiLevelOfRun) const { return atLeftBoundaryOfBidiRun(MatchBidiLevel, bidiLevelOfRun); }
    bool atRightBoundaryOfBidiRun(unsigned char bidiLevelOfRun) const { return atRightBoundaryOfBidiRun(MatchBidiLevel, bidiLevelOfRun); }

    Position positionAtLeftBoundaryOfBiDiRun() const;
    Position positionAtRightBoundaryOfBiDiRun() const;

    IntRect absoluteRect(CaretRectMode = CaretRectMode::Normal) const;

private:
    bool operator==(const RenderedPosition&) const { return false; }
    explicit RenderedPosition(const RenderObject*, InlineIterator::LeafBoxIterator, unsigned offset);

    InlineIterator::LeafBoxIterator previousLeafOnLine() const;
    InlineIterator::LeafBoxIterator nextLeafOnLine() const;
    bool atLeftmostOffsetInBox() const { return m_box && m_offset == m_box->leftmostCaretOffset(); }
    bool atRightmostOffsetInBox() const { return m_box && m_offset == m_box->rightmostCaretOffset(); }
    bool atLeftBoundaryOfBidiRun(ShouldMatchBidiLevel, unsigned char bidiLevelOfRun) const;
    bool atRightBoundaryOfBidiRun(ShouldMatchBidiLevel, unsigned char bidiLevelOfRun) const;

    SingleThreadWeakPtr<const RenderObject> m_renderer;
    InlineIterator::LeafBoxIterator m_box;
    unsigned m_offset { 0 };

    mutable std::optional<InlineIterator::LeafBoxIterator> m_previousLeafOnLine;
    mutable std::optional<InlineIterator::LeafBoxIterator> m_nextLeafOnLine;
};

bool renderObjectContainsPosition(const RenderObject*, const Position&);

} // namespace WebCore
