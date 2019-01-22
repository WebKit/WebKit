/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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

#include "DrawingAreaProxy.h"
#include "RemoteLayerTreeHost.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

OBJC_CLASS WKOneShotDisplayLinkHandler;

namespace WebKit {

class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxy final : public DrawingAreaProxy {
public:
    explicit RemoteLayerTreeDrawingAreaProxy(WebPageProxy&);
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return *m_remoteLayerTreeHost; }
    std::unique_ptr<RemoteLayerTreeHost> detachRemoteLayerTreeHost();

    void acceleratedAnimationDidStart(uint64_t layerID, const String& key, MonotonicTime startTime);
    void acceleratedAnimationDidEnd(uint64_t layerID, const String& key);

    uint64_t nextLayerTreeTransactionID() const { return m_pendingLayerTreeTransactionID + 1; }
    uint64_t lastCommittedLayerTreeTransactionID() const { return m_transactionIDForPendingCACommit; }

    void didRefreshDisplay();
    
    bool hasDebugIndicator() const { return !!m_debugIndicatorLayerTreeHost; }

    bool isAlwaysOnLoggingAllowed() const;

    CALayer *layerWithIDForTesting(uint64_t) const;

private:
    void sizeDidChange() override;
    void deviceScaleFactorDidChange() override;
    void didUpdateGeometry() override;
    
    // For now, all callbacks are called before committing changes, because that's what the only client requires.
    // Once we have other callbacks, it may make sense to have a before-commit/after-commit option.
    void dispatchAfterEnsuringDrawing(WTF::Function<void (CallbackBase::Error)>&&) override;

#if PLATFORM(MAC)
    void setViewExposedRect(Optional<WebCore::FloatRect>) override;
#endif

#if PLATFORM(IOS_FAMILY)
    WKOneShotDisplayLinkHandler *displayLinkHandler();
#endif

    float indicatorScale(WebCore::IntSize contentsSize) const;
    void updateDebugIndicator() override;
    void updateDebugIndicator(WebCore::IntSize contentsSize, bool rootLayerChanged, float scale, const WebCore::IntPoint& scrollPosition);
    void updateDebugIndicatorPosition();
    void initializeDebugIndicator();

    void waitForDidUpdateActivityState(ActivityStateChangeID) override;
    void hideContentUntilPendingUpdate() override;
    void hideContentUntilAnyUpdate() override;
    bool hasVisibleContent() const override;

    void prepareForAppSuspension() final;
    
    WebCore::FloatPoint indicatorLocation() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message handlers
    void willCommitLayerTree(uint64_t transactionID);
    void commitLayerTree(const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&);
    
    void sendUpdateGeometry();

    std::unique_ptr<RemoteLayerTreeHost> m_remoteLayerTreeHost;
    bool m_isWaitingForDidUpdateGeometry { false };
    enum DidUpdateMessageState { DoesNotNeedDidUpdate, NeedsDidUpdate, MissedCommit };
    DidUpdateMessageState m_didUpdateMessageState { DoesNotNeedDidUpdate };

    WebCore::IntSize m_lastSentSize;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    uint64_t m_pendingLayerTreeTransactionID { 0 };
    uint64_t m_lastVisibleTransactionID { 0 };
    uint64_t m_transactionIDForPendingCACommit { 0 };
    uint64_t m_transactionIDForUnhidingContent { 0 };
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };

    CallbackMap m_callbacks;

    RetainPtr<WKOneShotDisplayLinkHandler> m_displayLinkHandler;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(RemoteLayerTreeDrawingAreaProxy, DrawingAreaTypeRemoteLayerTree)
