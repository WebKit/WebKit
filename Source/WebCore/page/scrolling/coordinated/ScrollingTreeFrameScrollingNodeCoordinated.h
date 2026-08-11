/*
 * Copyright (C) 2012, 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2019, 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include "ScrollingTreeFrameScrollingNode.h"
#include <wtf/RefPtr.h>

namespace WebCore {
class CoordinatedPlatformLayer;
class ScrollingTreeScrollingNodeDelegateCoordinated;

class ScrollingTreeFrameScrollingNodeCoordinated final : public ScrollingTreeFrameScrollingNode {
public:
    static Ref<ScrollingTreeFrameScrollingNode> create(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeCoordinated();

    RefPtr<CoordinatedPlatformLayer> rootContentsLayer() const { return m_rootContentsLayer; }

private:
    ScrollingTreeFrameScrollingNodeCoordinated(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    ScrollingTreeScrollingNodeDelegateCoordinated& delegate() const;

    bool commitStateBeforeChildren(const ScrollingStateNode&) override;

    WheelEventHandlingResult handleWheelEvent(const PlatformWheelEvent&, EventTargeting) override;
    void currentScrollPositionChanged(ScrollType, ScrollingLayerPositionAction) override;
    void repositionScrollingLayers() override;
    void repositionRelatedLayers() override;

    RefPtr<CoordinatedPlatformLayer> m_rootContentsLayer;
    RefPtr<CoordinatedPlatformLayer> m_counterScrollingLayer;
    RefPtr<CoordinatedPlatformLayer> m_insetClipLayer;
    RefPtr<CoordinatedPlatformLayer> m_contentShadowLayer;
    RefPtr<CoordinatedPlatformLayer> m_headerLayer;
    RefPtr<CoordinatedPlatformLayer> m_footerLayer;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
