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

#if ENABLE(ASYNC_SCROLLING)

#include "RemoteScrollingCoordinator.h"
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingTree.h>

namespace WebKit {

class RemoteScrollingCoordinatorProxy;

class RemoteScrollingTree final : public WebCore::ScrollingTree {
public:
    static Ref<RemoteScrollingTree> create(RemoteScrollingCoordinatorProxy&);
    virtual ~RemoteScrollingTree();

    bool isRemoteScrollingTree() const override { return true; }
    EventResult tryToHandleWheelEvent(const WebCore::PlatformWheelEvent&) override;

    const RemoteScrollingCoordinatorProxy& scrollingCoordinatorProxy() const { return m_scrollingCoordinatorProxy; }

    void scrollingTreeNodeDidScroll(WebCore::ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, const std::optional<WebCore::FloatPoint>& layoutViewportOrigin, WebCore::ScrollingLayerPositionAction = WebCore::ScrollingLayerPositionAction::Sync) override;
    void scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, bool representsProgrammaticScroll) override;

    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, unsigned horizontal, unsigned vertical) override;

private:
    explicit RemoteScrollingTree(RemoteScrollingCoordinatorProxy&);

#if PLATFORM(MAC)
    void handleWheelEventPhase(WebCore::PlatformWheelEventPhase) override;
#endif

#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect fixedPositionRect() override;
    void scrollingTreeNodeWillStartPanGesture() override;
    void scrollingTreeNodeWillStartScroll() override;
    void scrollingTreeNodeDidEndScroll() override;
#endif

    Ref<WebCore::ScrollingTreeNode> createScrollingTreeNode(WebCore::ScrollingNodeType, WebCore::ScrollingNodeID) override;
    
    RemoteScrollingCoordinatorProxy& m_scrollingCoordinatorProxy;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(WebKit::RemoteScrollingTree, isRemoteScrollingTree());

#endif // ENABLE(ASYNC_SCROLLING)
