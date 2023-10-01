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
#include "VisiblePosition.h"

namespace WebCore {

static inline const RenderObject* rendererFromPosition(const Position& position)
{
    ASSERT(position.isNotNull());
    RefPtr<Node> rendererNode;
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

RenderedPosition::RenderedPosition(const RenderObject* renderer, InlineIterator::LeafBoxIterator box, unsigned offset)
    : m_renderer(renderer)
    , m_box(box)
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

    auto boxAndOffset = position.inlineBoxAndOffset(affinity);
    m_box = boxAndOffset.box;
    m_offset = boxAndOffset.offset;
    if (m_box)
        m_renderer = &m_box->renderer();
    else
        m_renderer = rendererFromPosition(position);
}

InlineIterator::LeafBoxIterator RenderedPosition::previousLeafOnLine() const
{
    if (!m_previousLeafOnLine)
        m_previousLeafOnLine = m_box->previousOnLineIgnoringLineBreak();
    return *m_previousLeafOnLine;
}

InlineIterator::LeafBoxIterator RenderedPosition::nextLeafOnLine() const
{
    if (!m_nextLeafOnLine)
        m_nextLeafOnLine = m_box->nextOnLineIgnoringLineBreak();
    return *m_nextLeafOnLine;
}

bool RenderedPosition::isEquivalent(const RenderedPosition& other) const
{
    return (m_renderer == other.m_renderer && m_box == other.m_box && m_offset == other.m_offset)
        || (atLeftmostOffsetInBox() && other.atRightmostOffsetInBox() && previousLeafOnLine() == other.m_box)
        || (atRightmostOffsetInBox() && other.atLeftmostOffsetInBox() && nextLeafOnLine() == other.m_box);
}

unsigned char RenderedPosition::bidiLevelOnLeft() const
{
    auto box = atLeftmostOffsetInBox() ? previousLeafOnLine() : m_box;
    return box ? box->bidiLevel() : 0;
}

unsigned char RenderedPosition::bidiLevelOnRight() const
{
    auto box = atRightmostOffsetInBox() ? nextLeafOnLine() : m_box;
    return box ? box->bidiLevel() : 0;
}

RenderedPosition RenderedPosition::leftBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_box || bidiLevelOfRun > m_box->bidiLevel())
        return RenderedPosition();

    auto box = m_box;
    do {
        auto prev = box->previousOnLineIgnoringLineBreak();
        if (!prev || prev->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&box->renderer(), box, box->leftmostCaretOffset());
        box = prev;
    } while (box);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

RenderedPosition RenderedPosition::rightBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_box || bidiLevelOfRun > m_box->bidiLevel())
        return RenderedPosition();

    auto box = m_box;
    do {
        auto next = box->nextOnLineIgnoringLineBreak();
        if (!next || next->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&box->renderer(), box, box->rightmostCaretOffset());
        box = next;
    } while (box);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

bool RenderedPosition::atLeftBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_box)
        return false;

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < m_box->bidiLevel();
        return m_box->bidiLevel() >= bidiLevelOfRun && (!previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return nextLeafOnLine() && m_box->bidiLevel() < nextLeafOnLine()->bidiLevel();
        return nextLeafOnLine() && m_box->bidiLevel() < bidiLevelOfRun && nextLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

bool RenderedPosition::atRightBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_box)
        return false;

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < m_box->bidiLevel();
        return m_box->bidiLevel() >= bidiLevelOfRun && (!nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return previousLeafOnLine() && m_box->bidiLevel() < previousLeafOnLine()->bidiLevel();
        return previousLeafOnLine() && m_box->bidiLevel() < bidiLevelOfRun && previousLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

Position RenderedPosition::positionAtLeftBoundaryOfBiDiRun() const
{
    ASSERT(atLeftBoundaryOfBidiRun());

    if (atLeftmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->protectedNode().get(), m_offset);

    return makeDeprecatedLegacyPosition(nextLeafOnLine()->renderer().protectedNode().get(), nextLeafOnLine()->leftmostCaretOffset());
}

Position RenderedPosition::positionAtRightBoundaryOfBiDiRun() const
{
    ASSERT(atRightBoundaryOfBidiRun());

    if (atRightmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->protectedNode().get(), m_offset);

    return makeDeprecatedLegacyPosition(previousLeafOnLine()->renderer().protectedNode().get(), previousLeafOnLine()->rightmostCaretOffset());
}

IntRect RenderedPosition::absoluteRect(CaretRectMode caretRectMode) const
{
    if (isNull())
        return IntRect();

    IntRect localRect = snappedIntRect(computeLocalCaretRect(*m_renderer, { m_box, m_offset }, caretRectMode));
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
