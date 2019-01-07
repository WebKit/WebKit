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

#include "ScrollingTreeScrollingNode.h"

namespace WebCore {

class PlatformWheelEvent;
class ScrollingTree;
class ScrollingStateScrollingNode;

class ScrollingTreeFrameScrollingNode : public ScrollingTreeScrollingNode {
public:
    virtual ~ScrollingTreeFrameScrollingNode();

    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    
    // FIXME: We should implement this when we support ScrollingTreeScrollingNodes as children.
    void updateLayersAfterAncestorChange(const ScrollingTreeNode& /*changedNode*/, const FloatRect& /*fixedPositionRect*/, const FloatSize& /*cumulativeDelta*/) override { }

    void handleWheelEvent(const PlatformWheelEvent&) override = 0;
    void setScrollPosition(const FloatPoint&) override;
    void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) override = 0;

    void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) override = 0;
    void updateLayersAfterDelegatedScroll(const FloatPoint&) override { }

    SynchronousScrollingReasons synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    bool shouldUpdateScrollLayerPositionSynchronously() const { return m_synchronousScrollingReasons; }
    bool fixedElementsLayoutRelativeToFrame() const { return m_fixedElementsLayoutRelativeToFrame; }

    FloatSize viewToContentsOffset(const FloatPoint& scrollPosition) const;
    FloatRect layoutViewportForScrollPosition(const FloatPoint& visibleContentOrigin, float scale) const;

    FloatRect fixedPositionRect() { return FloatRect(lastCommittedScrollPosition(), scrollableAreaSize()); };

protected:
    ScrollingTreeFrameScrollingNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    void scrollBy(const FloatSize&);
    void scrollByWithoutContentEdgeConstraints(const FloatSize&);

    float frameScaleFactor() const { return m_frameScaleFactor; }
    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }
    float topContentInset() const { return m_topContentInset; }

    FloatRect layoutViewport() const { return m_layoutViewport; };
    void setLayoutViewport(const FloatRect& r) { m_layoutViewport = r; };

    FloatPoint minLayoutViewportOrigin() const { return m_minLayoutViewportOrigin; }
    FloatPoint maxLayoutViewportOrigin() const { return m_maxLayoutViewportOrigin; }

    ScrollBehaviorForFixedElements scrollBehaviorForFixedElements() const { return m_behaviorForFixed; }

private:
    WEBCORE_EXPORT void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

    FloatRect m_layoutViewport;
    FloatPoint m_minLayoutViewportOrigin;
    FloatPoint m_maxLayoutViewportOrigin;
    
    float m_frameScaleFactor { 1 };
    float m_topContentInset { 0 };

    int m_headerHeight { 0 };
    int m_footerHeight { 0 };
    
    SynchronousScrollingReasons m_synchronousScrollingReasons { 0 };
    ScrollBehaviorForFixedElements m_behaviorForFixed { StickToDocumentBounds };
    
    bool m_fixedElementsLayoutRelativeToFrame { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_NODE(ScrollingTreeFrameScrollingNode, isFrameScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING)
