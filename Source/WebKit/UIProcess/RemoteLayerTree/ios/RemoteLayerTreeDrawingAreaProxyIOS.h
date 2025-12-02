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

#include "RemoteLayerTreeDrawingAreaProxy.h"

#if PLATFORM(IOS_FAMILY)

OBJC_CLASS WKDisplayLinkHandler;

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxyIOS final : public RemoteLayerTreeDrawingAreaProxy {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDrawingAreaProxyIOS);
    WTF_MAKE_NONCOPYABLE(RemoteLayerTreeDrawingAreaProxyIOS);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeDrawingAreaProxyIOS);
public:
    static Ref<RemoteLayerTreeDrawingAreaProxyIOS> create(WebPageProxy&, WebProcessProxy&);
    virtual ~RemoteLayerTreeDrawingAreaProxyIOS();

    bool isRemoteLayerTreeDrawingAreaProxyIOS() const final { return true; }

    void scheduleDisplayRefreshCallbacksForAnimation();
    void pauseDisplayRefreshCallbacksForAnimation();

    UIView *viewWithLayerIDForTesting(WebCore::PlatformLayerIdentifier) const;

private:
    RemoteLayerTreeDrawingAreaProxyIOS(WebPageProxy&, WebProcessProxy&);

    WebCore::DelegatedScrollingMode delegatedScrollingMode() const override;

    std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const override;

    void setPreferredFramesPerSecond(IPC::Connection&, WebCore::FramesPerSecond) override;
    void scheduleDisplayRefreshCallbacks() override;
    void pauseDisplayRefreshCallbacks() override;

    void didRefreshDisplay() override;

    std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() override;

    WKDisplayLinkHandler *displayLinkHandler();

    RetainPtr<WKDisplayLinkHandler> m_displayLinkHandler;

    bool m_needsDisplayRefreshCallbacksForDrawing { false };
    bool m_needsDisplayRefreshCallbacksForAnimation { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteLayerTreeDrawingAreaProxyIOS)
    static bool isType(const WebKit::DrawingAreaProxy& proxy) { return proxy.isRemoteLayerTreeDrawingAreaProxyIOS(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // PLATFORM(IOS_FAMILY)
