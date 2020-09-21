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

#include "InlineTextBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static inline RenderObject* rendererFromPosition(const Position& position)
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

RenderedPosition::RenderedPosition(const VisiblePosition& position)
    : RenderedPosition(position.deepEquivalent(), position.affinity())
{
}

RenderedPosition::RenderedPosition(const Position& position, Affinity affinity)
    : m_previousLeafOnLine(uncachedInlineBox())
    , m_nextLeafOnLine(uncachedInlineBox())
{
    if (position.isNull())
        return;
    auto box = position.inlineBoxAndOffset(affinity);
    m_inlineBox = box.box;
    m_offset = box.offset;
    if (m_inlineBox)
        m_renderer = &m_inlineBox->renderer();
    else
        m_renderer = rendererFromPosition(position);
}

InlineBox* RenderedPosition::previousLeafOnLine() const
{
    if (m_previousLeafOnLine == uncachedInlineBox())
        m_previousLeafOnLine = m_inlineBox->previousLeafOnLineIgnoringLineBreak();
    return m_previousLeafOnLine;
}

InlineBox* RenderedPosition::nextLeafOnLine() const
{
    if (m_nextLeafOnLine == uncachedInlineBox())
        m_nextLeafOnLine = m_inlineBox->nextLeafOnLineIgnoringLineBreak();
    return m_nextLeafOnLine;
}

bool RenderedPosition::isEquivalent(const RenderedPosition& other) const
{
    return (m_renderer == other.m_renderer && m_inlineBox == other.m_inlineBox && m_offset == other.m_offset)
        || (atLeftmostOffsetInBox() && other.atRightmostOffsetInBox() && previousLeafOnLine() == other.m_inlineBox)
        || (atRightmostOffsetInBox() && other.atLeftmostOffsetInBox() && nextLeafOnLine() == other.m_inlineBox);
}

unsigned char RenderedPosition::bidiLevelOnLeft() const
{
    InlineBox* box = atLeftmostOffsetInBox() ? previousLeafOnLine() : m_inlineBox;
    return box ? box->bidiLevel() : 0;
}

unsigned char RenderedPosition::bidiLevelOnRight() const
{
    InlineBox* box = atRightmostOffsetInBox() ? nextLeafOnLine() : m_inlineBox;
    return box ? box->bidiLevel() : 0;
}

RenderedPosition RenderedPosition::leftBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_inlineBox || bidiLevelOfRun > m_inlineBox->bidiLevel())
        return RenderedPosition();

    InlineBox* box = m_inlineBox;
    do {
        InlineBox* prev = box->previousLeafOnLineIgnoringLineBreak();
        if (!prev || prev->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&box->renderer(), box, box->caretLeftmostOffset());
        box = prev;
    } while (box);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

RenderedPosition RenderedPosition::rightBoundaryOfBidiRun(unsigned char bidiLevelOfRun)
{
    if (!m_inlineBox || bidiLevelOfRun > m_inlineBox->bidiLevel())
        return RenderedPosition();

    InlineBox* box = m_inlineBox;
    do {
        InlineBox* next = box->nextLeafOnLineIgnoringLineBreak();
        if (!next || next->bidiLevel() < bidiLevelOfRun)
            return RenderedPosition(&box->renderer(), box, box->caretRightmostOffset());
        box = next;
    } while (box);

    ASSERT_NOT_REACHED();
    return RenderedPosition();
}

bool RenderedPosition::atLeftBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_inlineBox)
        return false;

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < m_inlineBox->bidiLevel();
        return m_inlineBox->bidiLevel() >= bidiLevelOfRun && (!previousLeafOnLine() || previousLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return nextLeafOnLine() && m_inlineBox->bidiLevel() < nextLeafOnLine()->bidiLevel();
        return nextLeafOnLine() && m_inlineBox->bidiLevel() < bidiLevelOfRun && nextLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

bool RenderedPosition::atRightBoundaryOfBidiRun(ShouldMatchBidiLevel shouldMatchBidiLevel, unsigned char bidiLevelOfRun) const
{
    if (!m_inlineBox)
        return false;

    if (atRightmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return !nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < m_inlineBox->bidiLevel();
        return m_inlineBox->bidiLevel() >= bidiLevelOfRun && (!nextLeafOnLine() || nextLeafOnLine()->bidiLevel() < bidiLevelOfRun);
    }

    if (atLeftmostOffsetInBox()) {
        if (shouldMatchBidiLevel == IgnoreBidiLevel)
            return previousLeafOnLine() && m_inlineBox->bidiLevel() < previousLeafOnLine()->bidiLevel();
        return previousLeafOnLine() && m_inlineBox->bidiLevel() < bidiLevelOfRun && previousLeafOnLine()->bidiLevel() >= bidiLevelOfRun;
    }

    return false;
}

Position RenderedPosition::positionAtLeftBoundaryOfBiDiRun() const
{
    ASSERT(atLeftBoundaryOfBidiRun());

    if (atLeftmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->node(), m_offset);

    return makeDeprecatedLegacyPosition(nextLeafOnLine()->renderer().node(), nextLeafOnLine()->caretLeftmostOffset());
}

Position RenderedPosition::positionAtRightBoundaryOfBiDiRun() const
{
    ASSERT(atRightBoundaryOfBidiRun());

    if (atRightmostOffsetInBox())
        return makeDeprecatedLegacyPosition(m_renderer->node(), m_offset);

    return makeDeprecatedLegacyPosition(previousLeafOnLine()->renderer().node(), previousLeafOnLine()->caretRightmostOffset());
}

IntRect RenderedPosition::absoluteRect(LayoutUnit* extraWidthToEndOfLine) const
{
    if (isNull())
        return IntRect();

    IntRect localRect = snappedIntRect(m_renderer->localCaretRect(m_inlineBox, m_offset, extraWidthToEndOfLine));
    return localRect == IntRect() ? IntRect() : m_renderer->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

bool renderObjectContainsPosition(RenderObject* target, const Position& position)
{
    for (RenderObject* renderer = rendererFromPosition(position); renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderer == target)
            return true;
    }
    return false;
}

};
