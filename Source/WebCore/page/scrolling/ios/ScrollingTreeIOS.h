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

#ifndef ScrollingTreeIOS_h
#define ScrollingTreeIOS_h

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AsyncScrollingCoordinator;

class ScrollingTreeIOS : public ScrollingTree {
public:
    static RefPtr<ScrollingTreeIOS> create(AsyncScrollingCoordinator*);
    virtual ~ScrollingTreeIOS();

    virtual void commitNewTreeState(PassOwnPtr<ScrollingStateTree>) override;

    // No wheel events on iOS
    virtual void handleWheelEvent(const PlatformWheelEvent&) override { }
    virtual EventResult tryToHandleWheelEvent(const PlatformWheelEvent&) override { return DidNotHandleEvent; }

    virtual void invalidate() override;

private:
    explicit ScrollingTreeIOS(AsyncScrollingCoordinator*);
    virtual bool isScrollingTreeIOS() const override { return true; }

    virtual PassOwnPtr<ScrollingTreeNode> createNode(ScrollingNodeType, ScrollingNodeID) override;

    virtual void updateMainFrameScrollPosition(const IntPoint& scrollPosition, SetOrSyncScrollingLayerPosition = SyncScrollingLayerPosition) override;

    RefPtr<AsyncScrollingCoordinator> m_scrollingCoordinator;
};

SCROLLING_TREE_TYPE_CASTS(ScrollingTreeIOS, isScrollingTreeIOS());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeIOS_h
