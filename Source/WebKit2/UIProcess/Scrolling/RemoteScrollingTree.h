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

#ifndef RemoteScrollingTree_h
#define RemoteScrollingTree_h

#if ENABLE(ASYNC_SCROLLING)

#include "RemoteScrollingCoordinator.h"
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingTree.h>

namespace WebKit {

class RemoteScrollingCoordinatorProxy;

class RemoteScrollingTree : public WebCore::ScrollingTree {
public:
    static RefPtr<RemoteScrollingTree> create(RemoteScrollingCoordinatorProxy&);
    virtual ~RemoteScrollingTree();

    virtual bool isRemoteScrollingTree() const override { return true; }
    virtual EventResult tryToHandleWheelEvent(const WebCore::PlatformWheelEvent&) override;

    const RemoteScrollingCoordinatorProxy& scrollingCoordinatorProxy() const { return m_scrollingCoordinatorProxy; }

private:
    explicit RemoteScrollingTree(RemoteScrollingCoordinatorProxy&);

#if PLATFORM(MAC)
    virtual void handleWheelEventPhase(WebCore::PlatformWheelEventPhase) override;
#endif
    virtual void updateMainFrameScrollPosition(const WebCore::IntPoint& scrollPosition, WebCore::SetOrSyncScrollingLayerPosition = WebCore::SyncScrollingLayerPosition) override;

    virtual PassOwnPtr<WebCore::ScrollingTreeNode> createNode(WebCore::ScrollingNodeType, WebCore::ScrollingNodeID);
    
    RemoteScrollingCoordinatorProxy& m_scrollingCoordinatorProxy;
};

SCROLLING_TREE_TYPE_CASTS(RemoteScrollingTree, isRemoteScrollingTree());

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)

#endif // RemoteScrollingTree_h
