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

#include "ScrollingTreeFrameScrollingNode.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class WEBCORE_EXPORT ScrollingTreeFrameScrollingNodeIOS : public ScrollingTreeFrameScrollingNode {
public:
    static Ref<ScrollingTreeFrameScrollingNodeIOS> create(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeIOS();

protected:
    ScrollingTreeFrameScrollingNodeIOS(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    // ScrollingTreeNode member functions.
    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    void commitStateAfterChildren(const ScrollingStateNode&) override;

    ScrollingEventResult handleWheelEvent(const PlatformWheelEvent&) override;

    FloatPoint scrollPosition() const override;
    void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) override;

    void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) override;
    void updateLayersAfterDelegatedScroll(const FloatPoint&) override;
    void updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta) override;

    void setScrollPosition(const FloatPoint&) override;
    void setScrollLayerPosition(const FloatPoint&, const FloatRect& layoutViewport) override;

    FloatPoint minimumScrollPosition() const override;
    FloatPoint maximumScrollPosition() const override;

    CALayer *scrollLayer() const;

private:
    void updateChildNodesAfterScroll(const FloatPoint&);

    RetainPtr<CALayer> m_scrollLayer;
    RetainPtr<CALayer> m_counterScrollingLayer;
    RetainPtr<CALayer> m_headerLayer;
    RetainPtr<CALayer> m_footerLayer;
    FloatPoint m_probableMainThreadScrollPosition;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
