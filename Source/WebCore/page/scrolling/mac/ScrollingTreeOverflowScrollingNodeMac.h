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

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreeScrollingNodeDelegateMac.h"

OBJC_CLASS CALayer;

namespace WebCore {

class WEBCORE_EXPORT ScrollingTreeOverflowScrollingNodeMac : public ScrollingTreeOverflowScrollingNode {
public:
    static Ref<ScrollingTreeOverflowScrollingNodeMac> create(ScrollingTree&, ScrollingNodeID);
    virtual ~ScrollingTreeOverflowScrollingNodeMac();

protected:
    ScrollingTreeOverflowScrollingNodeMac(ScrollingTree&, ScrollingNodeID);

    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    
    FloatPoint adjustedScrollPosition(const FloatPoint&, ScrollClamping) const override;

    bool startAnimatedScrollToPosition(FloatPoint destinationPosition) override;
    void stopAnimatedScroll() override;

    void currentScrollPositionChanged(ScrollType, ScrollingLayerPositionAction) final;
    void willDoProgrammaticScroll(const FloatPoint&) final;

    void repositionScrollingLayers() override;
    void repositionRelatedLayers() override;
    void serviceScrollAnimation(MonotonicTime) override;

    WheelEventHandlingResult handleWheelEvent(const PlatformWheelEvent&, EventTargeting) override;

private:
    void willBeDestroyed() final;

    ScrollingTreeScrollingNodeDelegateMac m_delegate;
};

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
