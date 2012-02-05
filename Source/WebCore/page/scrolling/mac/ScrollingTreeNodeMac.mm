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
#include "ScrollingTreeNodeMac.h"

#if ENABLE(THREADED_SCROLLING)

#include "PlatformWheelEvent.h"
#include "ScrollingTree.h"
#include "ScrollingTreeState.h"

namespace WebCore {

PassOwnPtr<ScrollingTreeNode> ScrollingTreeNode::create(ScrollingTree* scrollingTree)
{
    return adoptPtr(new ScrollingTreeNodeMac(scrollingTree));
}

ScrollingTreeNodeMac::ScrollingTreeNodeMac(ScrollingTree* scrollingTree)
    : ScrollingTreeNode(scrollingTree)
{
}

void ScrollingTreeNodeMac::update(ScrollingTreeState* state)
{
    ScrollingTreeNode::update(state);

    if (state->changedProperties() & ScrollingTreeState::ScrollLayer)
        m_scrollLayer = state->platformScrollLayer();
}

void ScrollingTreeNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // FXIME: This needs to handle rubberbanding.
    scrollBy(IntSize(-wheelEvent.deltaX(), -wheelEvent.deltaY()));
}

IntPoint ScrollingTreeNodeMac::scrollPosition() const
{
    CGPoint scrollLayerPosition = m_scrollLayer.get().position;
    return IntPoint(-scrollLayerPosition.x, -scrollLayerPosition.y);
}

void ScrollingTreeNodeMac::setScrollPosition(const IntPoint& position)
{
    m_scrollLayer.get().position = CGPointMake(-position.x(), -position.y());
}

void ScrollingTreeNodeMac::scrollBy(const IntSize &offset)
{
    setScrollPosition(scrollPosition() + offset);

    scrollingTree()->updateMainFrameScrollPosition(scrollPosition());
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
