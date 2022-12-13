/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteScrollingCoordinatorProxy.h"
#include <WebCore/WheelEventDeltaFilter.h>

namespace WebKit {

class RemoteScrollingCoordinatorProxyMac final : public RemoteScrollingCoordinatorProxy {
public:
    explicit RemoteScrollingCoordinatorProxyMac(WebPageProxy&);

private:
    WebCore::PlatformWheelEvent filteredWheelEvent(const WebCore::PlatformWheelEvent&) override;

    void didReceiveWheelEvent(bool) override;
    bool scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&) override;
    void hasNodeWithAnimatedScrollChanged(bool) override;
    void displayDidRefresh(WebCore::PlatformDisplayID) override;

    void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&) override;
    void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&) override;

    std::unique_ptr<WebCore::WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_REMOTE_SCROLLING_COORDINATOR_PROXY(RemoteScrollingCoordinatorProxyMac, isRemoteScrollingCoordinatorProxyMac());

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
