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

#if PLATFORM(MAC)

#include "DisplayLinkObserverID.h"
#include <WebCore/AnimationFrameRate.h>

namespace WebKit {

class DisplayLink;
class RemoteLayerTreeDisplayLinkClient;
class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxyMac final : public RemoteLayerTreeDrawingAreaProxy {
friend class RemoteScrollingCoordinatorProxyMac;
public:
    RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy&, WebProcessProxy&);
    ~RemoteLayerTreeDrawingAreaProxyMac();

    void didRefreshDisplay() override;

private:
    WebCore::DelegatedScrollingMode delegatedScrollingMode() const override;
    std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const override;

    void didCommitLayerTree(const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&) override;

    void adjustTransientZoom(double, WebCore::FloatPoint) override;
    void commitTransientZoom(double, WebCore::FloatPoint) override;
    
    void applyTransientZoomToLayer();
    void removeTransientZoomFromLayer();

    void scheduleDisplayRefreshCallbacks() override;
    void pauseDisplayRefreshCallbacks() override;
    void setPreferredFramesPerSecond(WebCore::FramesPerSecond) override;
    void windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<WebCore::FramesPerSecond>) override;
    void colorSpaceDidChange() override;

    void didChangeViewExposedRect() override;

    void setDisplayLinkWantsFullSpeedUpdates(bool) override;

    void removeObserver(std::optional<DisplayLinkObserverID>&);

    DisplayLink* exisingDisplayLink();
    DisplayLink& ensureDisplayLink();

    std::optional<WebCore::PlatformDisplayID> m_displayID; // Would be nice to make this non-optional, and ensure we always get one on creation.
    std::optional<WebCore::FramesPerSecond> m_displayNominalFramesPerSecond;
    WebCore::FramesPerSecond m_clientPreferredFramesPerSecond { WebCore::FullSpeedFramesPerSecond };

    std::optional<DisplayLinkObserverID> m_displayRefreshObserverID;
    std::optional<DisplayLinkObserverID> m_fullSpeedUpdateObserverID;
    std::unique_ptr<RemoteLayerTreeDisplayLinkClient> m_displayLinkClient;
    WebCore::GraphicsLayer::PlatformLayerID m_pageScalingLayerID;

    std::optional<TransactionID> m_transactionIDAfterEndingTransientZoom;
    std::optional<double> m_transientZoomScale;
    std::optional<WebCore::FloatPoint> m_transientZoomOrigin;
};

} // namespace WebKit

#endif // #if PLATFORM(MAC)

