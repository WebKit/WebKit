/*
 * Copyright (C) 2012, 2014-2015 Apple Inc. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "ScrollbarThemeMac.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeScrollingNodeDelegateMac.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class WEBCORE_EXPORT ScrollingTreeFrameScrollingNodeMac : public ScrollingTreeFrameScrollingNode {
public:
    static Ref<ScrollingTreeFrameScrollingNode> create(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeMac();

protected:
    ScrollingTreeFrameScrollingNodeMac(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    void releaseReferencesToScrollerImpsOnTheMainThread();

    // ScrollingTreeNode member functions.
    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    void commitStateAfterChildren(const ScrollingStateNode&) override;

    ScrollingEventResult handleWheelEvent(const PlatformWheelEvent&) override;

    FloatPoint scrollPosition() const override;
    void setScrollPosition(const FloatPoint&, ScrollPositionClamp = ScrollPositionClamp::ToContentEdges) override;

    void updateLayersAfterViewportChange(const FloatRect& layoutViewport, double scale) override;

    void setScrollLayerPosition(const FloatPoint&, const FloatRect& layoutViewport) override;

    FloatPoint minimumScrollPosition() const override;
    FloatPoint maximumScrollPosition() const override;

    void updateMainFramePinState(const FloatPoint& scrollPosition);

    unsigned exposedUnfilledArea() const;

private:
    RetainPtr<CALayer> m_rootContentsLayer;
    RetainPtr<CALayer> m_counterScrollingLayer;
    RetainPtr<CALayer> m_insetClipLayer;
    RetainPtr<CALayer> m_contentShadowLayer;
    RetainPtr<CALayer> m_headerLayer;
    RetainPtr<CALayer> m_footerLayer;
    RetainPtr<NSScrollerImp> m_verticalScrollerImp;
    RetainPtr<NSScrollerImp> m_horizontalScrollerImp;
    FloatPoint m_probableMainThreadScrollPosition;
    
    ScrollingTreeScrollingNodeDelegateMac m_delegate;
    
    bool m_lastScrollHadUnfilledPixels { false };
    bool m_hadFirstUpdate { false };
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
