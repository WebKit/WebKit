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

#include "InlineBox.h"
#include "Position.h"
#include "VisiblePosition.h"

namespace WebCore {

static inline RenderObject* rendererFromPosition(const Position& position)
{
    ASSERT(position.isNotNull());
    Node* rendererNode = 0;
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
    : m_renderer(0)
    , m_inlineBox(0)
    , m_offset(0)
{
    if (position.isNull())
        return;
    position.getInlineBoxAndOffset(m_inlineBox, m_offset);
    if (m_inlineBox)
        m_renderer = m_inlineBox->renderer();
    else
        m_renderer = rendererFromPosition(position.deepEquivalent());
}

RenderedPosition::RenderedPosition(const Position& position, EAffinity affinity)
    : m_renderer(0)
    , m_inlineBox(0)
    , m_offset(0)
{
    if (position.isNull())
        return;
    position.getInlineBoxAndOffset(affinity, m_inlineBox, m_offset);
    if (m_inlineBox)
        m_renderer = m_inlineBox->renderer();
    else
        m_renderer = rendererFromPosition(position);
}

LayoutRect RenderedPosition::absoluteRect(int* extraWidthToEndOfLine) const
{
    if (isNull())
        return LayoutRect();

    LayoutRect localRect = m_renderer->localCaretRect(m_inlineBox, m_offset, extraWidthToEndOfLine);
    return localRect == LayoutRect() ? LayoutRect() : m_renderer->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
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
