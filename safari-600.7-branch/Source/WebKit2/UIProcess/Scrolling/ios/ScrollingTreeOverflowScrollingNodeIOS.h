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

#ifndef ScrollingTreeOverflowScrollingNodeIOS_h
#define ScrollingTreeOverflowScrollingNodeIOS_h

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS)

#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/ScrollingTreeOverflowScrollingNode.h>

OBJC_CLASS WKOverflowScrollViewDelegate;

namespace WebKit {

class ScrollingTreeOverflowScrollingNodeIOS : public WebCore::ScrollingTreeOverflowScrollingNode {
public:
    static PassRefPtr<ScrollingTreeOverflowScrollingNodeIOS> create(WebCore::ScrollingTree&, WebCore::ScrollingNodeID);
    virtual ~ScrollingTreeOverflowScrollingNodeIOS();

    void overflowScrollWillStart();
    void overflowScrollDidEnd();
    void overflowScrollViewWillStartPanGesture();
    void scrollViewDidScroll(const WebCore::FloatPoint&, bool inUserInteration);

    CALayer *scrollLayer() const { return m_scrollLayer.get(); }

private:
    ScrollingTreeOverflowScrollingNodeIOS(WebCore::ScrollingTree&, WebCore::ScrollingNodeID);

    virtual void updateBeforeChildren(const WebCore::ScrollingStateNode&) override;
    virtual void updateAfterChildren(const WebCore::ScrollingStateNode&) override;
    
    virtual WebCore::FloatPoint scrollPosition() const override;

    virtual void setScrollLayerPosition(const WebCore::FloatPoint&) override;

    virtual void updateLayersAfterViewportChange(const WebCore::FloatRect& fixedPositionRect, double scale) { }
    virtual void updateLayersAfterDelegatedScroll(const WebCore::FloatPoint& scrollPosition) override;

    virtual void updateLayersAfterAncestorChange(const WebCore::ScrollingTreeNode& changedNode, const WebCore::FloatRect& fixedPositionRect, const WebCore::FloatSize& cumulativeDelta) override;

    virtual void handleWheelEvent(const WebCore::PlatformWheelEvent&) override { }

    void updateChildNodesAfterScroll(const WebCore::FloatPoint&);
    
    RetainPtr<CALayer> m_scrollLayer;
    RetainPtr<CALayer> m_scrolledContentsLayer;

    RetainPtr<WKOverflowScrollViewDelegate> m_scrollViewDelegate;
    bool m_updatingFromStateNode;
};

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS)

#endif // ScrollingTreeOverflowScrollingNodeIOS_h
