/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/ScrollingTreeFrameScrollingNode.h>

namespace WebKit {

class ScrollingTreeScrollingNodeDelegateIOS;

class ScrollingTreeFrameScrollingNodeRemoteIOS : public WebCore::ScrollingTreeFrameScrollingNode {
public:
    static Ref<ScrollingTreeFrameScrollingNodeRemoteIOS> create(WebCore::ScrollingTree&, WebCore::ScrollingNodeType, WebCore::ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeRemoteIOS();

private:
    ScrollingTreeFrameScrollingNodeRemoteIOS(WebCore::ScrollingTree&, WebCore::ScrollingNodeType, WebCore::ScrollingNodeID);

    void commitStateBeforeChildren(const WebCore::ScrollingStateNode&) override;
    void commitStateAfterChildren(const WebCore::ScrollingStateNode&) override;

    FloatPoint minimumScrollPosition() const override;
    FloatPoint maximumScrollPosition() const override;

    WebCore::FloatPoint scrollPosition() const override;
    void setScrollPosition(const WebCore::FloatPoint&, WebCore::ScrollPositionClamp = WebCore::ScrollPositionClamp::ToContentEdges) override;
    void setScrollLayerPosition(const WebCore::FloatPoint&, const WebCore::FloatRect& layoutViewport) override;

    void updateChildNodesAfterScroll(const FloatPoint&);

    void updateLayersAfterDelegatedScroll(const WebCore::FloatPoint& scrollPosition) override;
    void updateLayersAfterViewportChange(const WebCore::FloatRect& layoutViewport, double scale) override;
    void updateLayersAfterAncestorChange(const WebCore::ScrollingTreeNode& changedNode, const WebCore::FloatRect& layoutViewport, const WebCore::FloatSize& cumulativeDelta) override;

    std::unique_ptr<ScrollingTreeScrollingNodeDelegateIOS> m_scrollingNodeDelegate;

    RetainPtr<CALayer> m_counterScrollingLayer;
    RetainPtr<CALayer> m_headerLayer;
    RetainPtr<CALayer> m_footerLayer;
};

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
