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

#include <WebCore/ScrollingTreeOverflowScrollingNode.h>

namespace WebKit {

class ScrollingTreeScrollingNodeDelegateIOS;

class ScrollingTreeOverflowScrollingNodeIOS : public WebCore::ScrollingTreeOverflowScrollingNode {
public:
    static Ref<ScrollingTreeOverflowScrollingNodeIOS> create(WebCore::ScrollingTree&, WebCore::ScrollingNodeID);
    virtual ~ScrollingTreeOverflowScrollingNodeIOS();

private:
    ScrollingTreeOverflowScrollingNodeIOS(WebCore::ScrollingTree&, WebCore::ScrollingNodeID);

    void commitStateBeforeChildren(const WebCore::ScrollingStateNode&) override;
    void commitStateAfterChildren(const WebCore::ScrollingStateNode&) override;
    
    WebCore::FloatPoint scrollPosition() const override;

    void setScrollLayerPosition(const WebCore::FloatPoint&, const WebCore::FloatRect& layoutViewport) override;

    void updateLayersAfterViewportChange(const WebCore::FloatRect& fixedPositionRect, double scale) override { }
    void updateLayersAfterDelegatedScroll(const WebCore::FloatPoint& scrollPosition) override;

    void updateLayersAfterAncestorChange(const WebCore::ScrollingTreeNode& changedNode, const WebCore::FloatRect& fixedPositionRect, const WebCore::FloatSize& cumulativeDelta) override;

    void handleWheelEvent(const WebCore::PlatformWheelEvent&) override { }

    std::unique_ptr<ScrollingTreeScrollingNodeDelegateIOS> m_scrollingNodeDelegate;
};

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
