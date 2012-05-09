/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ScrollingTreeState.h"

#if ENABLE(THREADED_SCROLLING)

namespace WebCore {

PassOwnPtr<ScrollingTreeState> ScrollingTreeState::create()
{
    return adoptPtr(new ScrollingTreeState);
}

ScrollingTreeState::ScrollingTreeState()
    : m_changedProperties(0)
    , m_wheelEventHandlerCount(0)
    , m_shouldUpdateScrollLayerPositionOnMainThread(false)
    , m_horizontalScrollElasticity(ScrollElasticityNone)
    , m_verticalScrollElasticity(ScrollElasticityNone)
    , m_hasEnabledHorizontalScrollbar(false)
    , m_hasEnabledVerticalScrollbar(false)
    , m_horizontalScrollbarMode(ScrollbarAuto)
    , m_verticalScrollbarMode(ScrollbarAuto)
{
}

ScrollingTreeState::~ScrollingTreeState()
{
}

void ScrollingTreeState::setViewportRect(const IntRect& viewportRect)
{
    if (m_viewportRect == viewportRect)
        return;

    m_viewportRect = viewportRect;
    m_changedProperties |= ViewportRect;
}

void ScrollingTreeState::setContentsSize(const IntSize& contentsSize)
{
    if (m_contentsSize == contentsSize)
        return;

    m_contentsSize = contentsSize;
    m_changedProperties |= ContentsSize;
}

void ScrollingTreeState::setNonFastScrollableRegion(const Region& nonFastScrollableRegion)
{
    if (m_nonFastScrollableRegion == nonFastScrollableRegion)
        return;

    m_nonFastScrollableRegion = nonFastScrollableRegion;
    m_changedProperties |= NonFastScrollableRegion;
}

void ScrollingTreeState::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (m_wheelEventHandlerCount == wheelEventHandlerCount)
        return;

    m_wheelEventHandlerCount = wheelEventHandlerCount;
    m_changedProperties |= WheelEventHandlerCount;
}

void ScrollingTreeState::setShouldUpdateScrollLayerPositionOnMainThread(bool shouldUpdateScrollLayerPositionOnMainThread)
{
    if (m_shouldUpdateScrollLayerPositionOnMainThread == shouldUpdateScrollLayerPositionOnMainThread)
        return;

    m_shouldUpdateScrollLayerPositionOnMainThread = shouldUpdateScrollLayerPositionOnMainThread;
    m_changedProperties |= ShouldUpdateScrollLayerPositionOnMainThread;
}

void ScrollingTreeState::setHorizontalScrollElasticity(ScrollElasticity horizontalScrollElasticity)
{
    if (m_horizontalScrollElasticity == horizontalScrollElasticity)
        return;

    m_horizontalScrollElasticity = horizontalScrollElasticity;
    m_changedProperties |= HorizontalScrollElasticity;
}

void ScrollingTreeState::setVerticalScrollElasticity(ScrollElasticity verticalScrollElasticity)
{
    if (m_verticalScrollElasticity == verticalScrollElasticity)
        return;

    m_verticalScrollElasticity = verticalScrollElasticity;
    m_changedProperties |= VerticalScrollElasticity;
}

void ScrollingTreeState::setHasEnabledHorizontalScrollbar(bool hasEnabledHorizontalScrollbar)
{
    if (m_hasEnabledHorizontalScrollbar == hasEnabledHorizontalScrollbar)
        return;

    m_hasEnabledHorizontalScrollbar = hasEnabledHorizontalScrollbar;
    m_changedProperties |= HasEnabledHorizontalScrollbar;
}

void ScrollingTreeState::setHasEnabledVerticalScrollbar(bool hasEnabledVerticalScrollbar)
{
    if (m_hasEnabledVerticalScrollbar == hasEnabledVerticalScrollbar)
        return;

    m_hasEnabledVerticalScrollbar = hasEnabledVerticalScrollbar;
    m_changedProperties |= HasEnabledVerticalScrollbar;
}

void ScrollingTreeState::setHorizontalScrollbarMode(ScrollbarMode horizontalScrollbarMode)
{
    if (m_horizontalScrollbarMode == horizontalScrollbarMode)
        return;

    m_horizontalScrollbarMode = horizontalScrollbarMode;
    m_changedProperties |= HorizontalScrollbarMode;
}

void ScrollingTreeState::setVerticalScrollbarMode(ScrollbarMode verticalScrollbarMode)
{
    if (m_verticalScrollbarMode == verticalScrollbarMode)
        return;

    m_verticalScrollbarMode = verticalScrollbarMode;
    m_changedProperties |= VerticalScrollbarMode;
}

void ScrollingTreeState::setRequestedScrollPosition(const IntPoint& requestedScrollPosition)
{
    m_requestedScrollPosition = requestedScrollPosition;
    m_changedProperties |= RequestedScrollPosition;
}

void ScrollingTreeState::setScrollOrigin(const IntPoint& scrollOrigin)
{
    if (m_scrollOrigin == scrollOrigin)
        return;

    m_scrollOrigin = scrollOrigin;
    m_changedProperties |= ScrollOrigin;
}

PassOwnPtr<ScrollingTreeState> ScrollingTreeState::commit()
{
    OwnPtr<ScrollingTreeState> treeState = adoptPtr(new ScrollingTreeState(*this));
    m_changedProperties = 0;

    return treeState.release();
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
