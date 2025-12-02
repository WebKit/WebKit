/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderHighlight.h"

#include "Document.h"
#include "FrameSelection.h"
#include "Highlight.h"
#include "Logging.h"
#include "Position.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "TextBoxSelectableRange.h"
#include "VisibleSelection.h"
#include <wtf/text/TextStream.h>

namespace WebCore {



RenderRangeIterator::RenderRangeIterator(RenderObject* start)
    : m_current(start)
{
    checkForSpanner();
}

RenderObject* RenderRangeIterator::current() const
{
    return m_current.get();
}

RenderObject* RenderRangeIterator::next()
{
    CheckedPtr span = m_spannerStack.isEmpty() ? nullptr : m_spannerStack.last()->spanner();
    m_current = CheckedRef { *m_current }->nextInPreOrder(span.get());
    checkForSpanner();
    if (!m_current && span) {
        CheckedPtr placeholder = m_spannerStack.last();
        m_spannerStack.removeLast();
        m_current = placeholder->nextInPreOrder();
        checkForSpanner();
    }
    return m_current.get();
}

void RenderRangeIterator::checkForSpanner()
{
    CheckedPtr placeholder = dynamicDowncast<RenderMultiColumnSpannerPlaceholder>(m_current.get());
    if (!placeholder)
        return;
    m_spannerStack.append(placeholder.get());
    m_current = placeholder->spanner();
}

static CheckedPtr<RenderObject> rendererAfterOffset(const RenderObject& renderer, unsigned offset)
{
    CheckedPtr child = renderer.childAt(offset);
    return child ? child : renderer.nextInPreOrderAfterChildren();
}

void RenderHighlight::setRenderRange(const RenderRange& renderRange)
{
    ASSERT(renderRange.start() && renderRange.end());
    m_renderRange = renderRange;
}

bool RenderHighlight::setRenderRange(const HighlightRange& highlightRange)
{
    if (highlightRange.startPosition().isNull() || highlightRange.endPosition().isNull())
        return false;

    auto startPosition = highlightRange.startPosition();
    auto endPosition = highlightRange.endPosition();

    if (!startPosition.containerNode() || !endPosition.containerNode())
        return false;

    CheckedPtr startRenderer = startPosition.containerNode()->renderer();
    CheckedPtr endRenderer = endPosition.containerNode()->renderer();

    if (!startRenderer || !endRenderer)
        return false;

    unsigned startOffset = startPosition.computeOffsetInContainerNode();
    unsigned endOffset = endPosition.computeOffsetInContainerNode();

    setRenderRange({ startRenderer.get(), endRenderer.get(), startOffset, endOffset });
    return true;
}

RenderObject::HighlightState RenderHighlight::highlightStateForRenderer(const RenderObject& renderer)
{
    if (m_isSelection)
        return renderer.selectionState();

    if (&renderer == m_renderRange.start()) {
        if (m_renderRange.start() && m_renderRange.end() && m_renderRange.start() == m_renderRange.end())
            return RenderObject::HighlightState::Both;
        if (m_renderRange.start())
            return RenderObject::HighlightState::Start;
    }
    if (&renderer == m_renderRange.end())
        return RenderObject::HighlightState::End;

    CheckedPtr rangeEnd = m_renderRange.end();
    CheckedPtr highlightEnd = rangeEnd ? rendererAfterOffset(*rangeEnd, m_renderRange.endOffset()) : nullptr;

    RenderRangeIterator highlightIterator(m_renderRange.start());
    for (CheckedPtr currentRenderer = m_renderRange.start(); currentRenderer && currentRenderer != highlightEnd; currentRenderer = highlightIterator.next()) {
        if (currentRenderer == m_renderRange.start())
            continue;
        if (!currentRenderer->canBeSelectionLeaf())
            continue;
        if (&renderer == currentRenderer)
            return RenderObject::HighlightState::Inside;
    }
    return RenderObject::HighlightState::None;
}

RenderObject::HighlightState RenderHighlight::highlightStateForTextBox(const RenderText& renderer, const TextBoxSelectableRange& textBoxRange)
{
    auto state = highlightStateForRenderer(renderer);

    if (state == RenderObject::HighlightState::None || state == RenderObject::HighlightState::Inside)
        return state;

    auto startOffset = this->startOffset();
    auto endOffset = this->endOffset();

    // The position after a hard line break is considered to be past its end.
    ASSERT(textBoxRange.start + textBoxRange.length >= (textBoxRange.isLineBreak ? 1 : 0));
    unsigned lastSelectable = textBoxRange.start + textBoxRange.length - (textBoxRange.isLineBreak ? 1 : 0);

    bool containsStart = state != RenderObject::HighlightState::End && startOffset >= textBoxRange.start && startOffset < textBoxRange.start + textBoxRange.length;
    bool containsEnd = state != RenderObject::HighlightState::Start && endOffset > textBoxRange.start && endOffset <= lastSelectable;
    if (containsStart && containsEnd)
        return RenderObject::HighlightState::Both;
    if (containsStart)
        return RenderObject::HighlightState::Start;
    if (containsEnd)
        return RenderObject::HighlightState::End;
    if ((state == RenderObject::HighlightState::End || startOffset < textBoxRange.start) && (state == RenderObject::HighlightState::Start || endOffset > lastSelectable))
        return RenderObject::HighlightState::Inside;

    return RenderObject::HighlightState::None;
}

std::pair<unsigned, unsigned> RenderHighlight::rangeForTextBox(const RenderText& renderer, const TextBoxSelectableRange& textBoxRange)
{
    auto state = highlightStateForTextBox(renderer, textBoxRange);

    switch (state) {
    case RenderObject::HighlightState::Inside:
        return textBoxRange.clamp(0, std::numeric_limits<unsigned>::max());
    case RenderObject::HighlightState::Start:
        return textBoxRange.clamp(startOffset(), std::numeric_limits<unsigned>::max());
    case RenderObject::HighlightState::End:
        return textBoxRange.clamp(0, endOffset());
    case RenderObject::HighlightState::Both:
        return textBoxRange.clamp(startOffset(), endOffset());
    case RenderObject::HighlightState::None:
        return { 0, 0 };
    };

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
