/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingTreeNodeMac_h
#define ScrollingTreeNodeMac_h

#if ENABLE(THREADED_SCROLLING)

#include "ScrollElasticityController.h"
#include "ScrollingTreeNode.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

namespace WebCore {

class ScrollingTreeNodeMac : public ScrollingTreeNode, private ScrollElasticityControllerClient {
public:
    explicit ScrollingTreeNodeMac(ScrollingTree*);
    virtual ~ScrollingTreeNodeMac();

private:
    // ScrollingTreeNode member functions.
    virtual void update(ScrollingTreeState*) OVERRIDE;
    virtual void handleWheelEvent(const PlatformWheelEvent&) OVERRIDE;

    // ScrollElasticityController member functions.
    virtual bool allowsHorizontalStretching() OVERRIDE;
    virtual bool allowsVerticalStretching() OVERRIDE;
    virtual IntSize stretchAmount() OVERRIDE;
    virtual bool pinnedInDirection(const FloatSize&) OVERRIDE;
    virtual bool canScrollHorizontally() OVERRIDE;
    virtual bool canScrollVertically() OVERRIDE;
    virtual bool shouldRubberBandInDirection(ScrollDirection) OVERRIDE;
    virtual IntPoint absoluteScrollPosition() OVERRIDE;
    virtual void immediateScrollBy(const FloatSize&) OVERRIDE;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) OVERRIDE;
    virtual void startSnapRubberbandTimer() OVERRIDE;
    virtual void stopSnapRubberbandTimer() OVERRIDE;

    IntPoint scrollPosition() const;
    void setScrollPosition(const IntPoint&);
    void setScrollPositionWithoutContentEdgeConstraints(const IntPoint&);

    void setScrollLayerPosition(const IntPoint&);

    IntPoint minimumScrollPosition() const;
    IntPoint maximumScrollPosition() const;

    void scrollBy(const IntSize&);
    void scrollByWithoutContentEdgeConstraints(const IntSize&);

    void updateMainFramePinState(const IntPoint& scrollPosition);

    void logExposedUnfilledArea();

    ScrollElasticityController m_scrollElasticityController;
    RetainPtr<CFRunLoopTimerRef> m_snapRubberbandTimer;

    RetainPtr<CALayer> m_scrollLayer;
    IntPoint m_probableMainThreadScrollPosition;
};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingTreeNodeMac_h
