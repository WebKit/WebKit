/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"

namespace WebCore {

class AsyncScrollingCoordinator;

class ScrollingTreeIOS final : public ScrollingTree {
public:
    static Ref<ScrollingTreeIOS> create(AsyncScrollingCoordinator&);
    virtual ~ScrollingTreeIOS();

private:
    explicit ScrollingTreeIOS(AsyncScrollingCoordinator&);

    bool isScrollingTreeIOS() const final { return true; }

    // No wheel events on iOS
    ScrollingEventResult handleWheelEvent(const PlatformWheelEvent&) final { return ScrollingEventResult::DidNotHandleEvent; }
    ScrollingEventResult tryToHandleWheelEvent(const PlatformWheelEvent&) final { return ScrollingEventResult::DidNotHandleEvent; }

    void invalidate() final;

    Ref<ScrollingTreeNode> createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID) final;
    void scrollingTreeNodeDidScroll(ScrollingNodeID, const FloatPoint& scrollPosition, const Optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync) final;
    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, unsigned horizontal, unsigned vertical) final;
    FloatRect fixedPositionRect() final;

    RefPtr<AsyncScrollingCoordinator> m_scrollingCoordinator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(WebCore::ScrollingTreeIOS, isScrollingTreeIOS())

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
