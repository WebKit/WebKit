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

#ifndef ScrollingTreeFrameScrollingNode_h
#define ScrollingTreeFrameScrollingNode_h

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingTreeScrollingNode.h"

namespace WebCore {

class PlatformWheelEvent;
class ScrollingTree;
class ScrollingStateScrollingNode;

class ScrollingTreeFrameScrollingNode : public ScrollingTreeScrollingNode {
public:
    virtual ~ScrollingTreeFrameScrollingNode();

    virtual void updateBeforeChildren(const ScrollingStateNode&) override;
    
    // FIXME: We should implement this when we support ScrollingTreeScrollingNodes as children.
    virtual void updateLayersAfterAncestorChange(const ScrollingTreeNode& /*changedNode*/, const FloatRect& /*fixedPositionRect*/, const FloatSize& /*cumulativeDelta*/) override { }

    virtual void handleWheelEvent(const PlatformWheelEvent&) = 0;
    virtual void setScrollPosition(const FloatPoint&);
    virtual void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) = 0;

    virtual void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) = 0;
    virtual void updateLayersAfterDelegatedScroll(const FloatPoint&) { }

    SynchronousScrollingReasons synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    bool shouldUpdateScrollLayerPositionSynchronously() const { return m_synchronousScrollingReasons; }

protected:
    ScrollingTreeFrameScrollingNode(ScrollingTree&, ScrollingNodeID);

    void scrollBy(const FloatSize&);
    void scrollByWithoutContentEdgeConstraints(const FloatSize&);

    float frameScaleFactor() const { return m_frameScaleFactor; }
    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }
    float topContentInset() const { return m_topContentInset; }
    
    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }
    
private:
    float m_frameScaleFactor;

    int m_headerHeight;
    int m_footerHeight;
    float m_topContentInset;
    
    SynchronousScrollingReasons m_synchronousScrollingReasons;
    ScrollBehaviorForFixedElements m_behaviorForFixed;
};

SCROLLING_NODE_TYPE_CASTS(ScrollingTreeFrameScrollingNode, isFrameScrollingNode());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeScrollingNode_h
