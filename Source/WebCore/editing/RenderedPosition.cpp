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

#include "config.h"
#include "RenderedPosition.h"

#include "CaretRectComputation.h"
#include "InlineRunAndOffset.h"
#include "LegacyInlineTextBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static inline const RenderObject* rendererFromPosition(const Position& position)
{
    ASSERT(position.isNotNull());
    Node* rendererNode = nullptr;
    switch (position.anchorType()) {
    case Position::PositionIsOffsetInAnchor:
        rendererNode = position.computeNodeAfterPosition();
        if (!rendererNode || !rendererNode->renderer())
            rendererNode = position.anchorNode()->lastChild();
        break;

    case Position::PositionIsBeforeAnchor:
    case Position::PositionIsAfterAnchor:
        break;

    case Position::PositionIsBeforeChildren:
        rendererNode = position.anchorNode()->firstChild();
        break;
    case Position::PositionIsAfterChildren:
        rendererNode = position.anchorNode()->lastChild();
        break;
    }
    if (!rendererNode || !rendererNode->renderer())
        rendererNode = position.anchorNode();
    return rendererNode->renderer();
}

RenderedPosition::RenderedPosition()
{
}

RenderedPosition::RenderedPosition(const RenderObject* renderer, LayoutIntegration::RunIterator run, unsigned offset)
    : m_renderer(renderer)
    , m_run(run)
    , m_offset(offset)
{
}

RenderedPosition::RenderedPosition(const VisiblePosition& position)
    : RenderedPosition(position.deepEquivalent(), position.affinity())
{
}

RenderedPosition::RenderedPosition(const Position& position, Affinity affinity)
{
    if (position.isNull())
        return;

    auto runAndOffset = position.inlineRunAndOffset(affinity);
    m_run = runAndOffset.run;
    m_offset = runAndOffset.offset;
    if (m_run)
        m_renderer = &m_run->renderer();
    else
        m_renderer = rendererFromPosition(position);
}

LayoutIntegration::RunIterator RenderedPosition::previousLeafOnLine() const
{
    if (!m_previousLeafOnLine)
        m_previousLeafOnLine = m_run.previousOnLineIgnoringLineBreak();
    return *m_previousLeafOnLine;
}

LayoutIntegration::RunIterator RenderedPosition::nextLeafOnLine() const
{
    if (!m_nextLeafOnLine)
        m_nextLeafOnLine = m_run.nextOnLineIgnoringLineBreak();
    return *m_nextLeafOnLine;
}

bool RenderedPosition::isEquivalent(const RenderedPosition& other) const
{
    return (m_renderer == other.m_renderer && m_run == other.m_run && m_offset == other.m_offset)
        || (atLeftmostOffsetInBox() && other.atRightmostOffsetInBox() && previousLeafOnLine() == other.m_run)
        || (atRightmostOffsetInBox() && other.atLeftmostOffsetInBox() && nextLeafOnLine() == other.m_run);
}

unsigned char RenderedPosition::bidiLevelOnLeft() const
{
    auto run = atLeftmostOffsetInBox() ? previousLeafOnLine() : m_run;
    return run ? run->bidiLevel() : 0;
}

unsigned char RenderedPosition::bidiLevelOnRight() const
{
    auto run = atRightmostOffsetInBox() ? nextLeafOnLine() : m_run;
    return run ? run->bidiLevel() : 0;
}

RenderedPosition RenderedPosition::leftBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_run || bidiLevelOfRun > m_run->bidiLevel())
        return RenderedPosition();

    auto run = m_run;
    do {
        auto prev = run.previousOnLineIgnoringLineBreak();
        if (!prev || prev->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&run->renderer(), run, run->leftmostCaretOffset());
        run = prev;
    } while (run);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

RenderedPosition RenderedPosition::rightBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_run || bidiLevelOfRun > m_run->bidiLevel())
        return RenderedPosition();

    auto run = m_run;
    do {
        auto next = run.nextOnLineIgnoringLineBreak();
        if (!next || next->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&run->renderer(), run, run->rightmostCaretOffset());
        run = next;
    } while (run);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

bool RenderedPosition::atLeftBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_run)
        return false;

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < m_run->bidiLevel();
        return m_run->bidiLevel() >= bidiLevelOfRun && (!previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return nextLeafOnLine() && m_run->bidiLevel() < nextLeafOnLine()->bidiLevel();
        return nextLeafOnLine() && m_run->bidiLevel() < bidiLevelOfRun && nextLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

bool RenderedPosition::atRightBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_run)
        return false;

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < m_run->bidiLevel();
        return m_run->bidiLevel() >= bidiLevelOfRun && (!nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return previousLeafOnLine() && m_run->bidiLevel() < previousLeafOnLine()->bidiLevel();
        return previousLeafOnLine() && m_run->bidiLevel() < bidiLevelOfRun && previousLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

Position RenderedPosition::positionAtLeftBoundaryOfBiDiRun() const
{
    ASSERT(atLeftBoundaryOfBidiRun());

    if (atLeftmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->node(), m_offset);

    return makeDeprecatedLegacyPosition(nextLeafOnLine()->renderer().node(), nextLeafOnLine()->leftmostCaretOffset());
}

Position RenderedPosition::positionAtRightBoundaryOfBiDiRun() const
{
    ASSERT(atRightBoundaryOfBidiRun());

    if (atRightmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->node(), m_offset);

    return makeDeprecatedLegacyPosition(previousLeafOnLine()->renderer().node(), previousLeafOnLine()->rightmostCaretOffset());
}

IntRect RenderedPosition::absoluteRect(CaretRectMode caretRectMode) const
{
    if (isNull())
        return IntRect();

    IntRect localRect = snappedIntRect(computeLocalCaretRect(*m_renderer, { m_run, m_offset }, caretRectMode));
    return localRect == IntRect() ? IntRect() : m_renderer->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

bool renderObjectContainsPosition(const RenderObject* target, const Position& position)
{
    for (auto* renderer = rendererFromPosition(position); renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderer == target)
            return true;
    }
    return false;
}

};
