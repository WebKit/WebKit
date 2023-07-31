/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "TransactionID.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

namespace WebKit {

class RemoteScrollingCoordinatorProxy;
class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxy : public DrawingAreaProxy {
public:
    RemoteLayerTreeDrawingAreaProxy(WebPageProxy&);
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    virtual bool isRemoteLayerTreeDrawingAreaProxyMac() const { return false; }

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return *m_remoteLayerTreeHost; }
    std::unique_ptr<RemoteLayerTreeHost> detachRemoteLayerTreeHost();
    
    virtual std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const = 0;

    void acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier, const String& key, MonotonicTime startTime);
    void acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier, const String& key);

    TransactionID nextLayerTreeTransactionID() const { return m_pendingLayerTreeTransactionID.next(); }
    TransactionID lastCommittedLayerTreeTransactionID() const { return m_transactionIDForPendingCACommit; }

    virtual void didRefreshDisplay();
    virtual void setDisplayLinkWantsFullSpeedUpdates(bool) { }
    
    bool hasDebugIndicator() const { return !!m_debugIndicatorLayerTreeHost; }

    CALayer *layerWithIDForTesting(WebCore::PlatformLayerIdentifier) const;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    void updateOverlayRegionIDs(const HashSet<WebCore::PlatformLayerIdentifier> &overlayRegionIDs) { m_remoteLayerTreeHost->updateOverlayRegionIDs(overlayRegionIDs); }
#endif
    
    void viewWillStartLiveResize() final;
    void viewWillEndLiveResize() final;

    // For testing.
    unsigned countOfTransactionsWithNonEmptyLayerChanges() const { return m_countOfTransactionsWithNonEmptyLayerChanges; }
    
protected:
    void updateDebugIndicatorPosition();

    bool shouldCoalesceVisualEditorStateUpdates() const override { return true; }

private:

    void sizeDidChange() final;
    void deviceScaleFactorDidChange() final;
    void windowKindDidChange() final;
    void minimumSizeForAutoLayoutDidChange() final;
    void sizeToContentAutoSizeMaximumSizeDidChange() final;
    void didUpdateGeometry();
    std::span<IPC::ReceiverName> messageReceiverNames() const final;

    virtual void scheduleDisplayRefreshCallbacks() { }
    virtual void pauseDisplayRefreshCallbacks() { }

    float indicatorScale(WebCore::IntSize contentsSize) const;
    void updateDebugIndicator() final;
    void updateDebugIndicator(WebCore::IntSize contentsSize, bool rootLayerChanged, float scale, const WebCore::IntPoint& scrollPosition);
    void initializeDebugIndicator();

    void waitForDidUpdateActivityState(ActivityStateChangeID, WebProcessProxy&) final;
    void hideContentUntilPendingUpdate() final;
    void hideContentUntilAnyUpdate() final;
    bool hasVisibleContent() const final;

    void prepareForAppSuspension() final;
    
    WebCore::FloatPoint indicatorLocation() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Message handlers
    virtual void setPreferredFramesPerSecond(WebCore::FramesPerSecond) { }
    void willCommitLayerTree(TransactionID);
    void commitLayerTreeNotTriggered(TransactionID);
    void commitLayerTree(IPC::Connection&, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>&);
    void commitLayerTreeTransaction(IPC::Connection&, const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&);
    virtual void didCommitLayerTree(IPC::Connection&, const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&) { }

    void asyncSetLayerContents(WebCore::PlatformLayerIdentifier, ImageBufferBackendHandle&&, const WebCore::RenderingResourceIdentifier&);

    void sendUpdateGeometry();

    std::unique_ptr<RemoteLayerTreeHost> m_remoteLayerTreeHost;
    bool m_isWaitingForDidUpdateGeometry { false };

    // displayDidRefresh is sent to the WebProcess, and it responds
    // with a commitLayerTree message (ideally before the next
    // displayDidRefresh, otherwise we mark it as missed and send
    // it when commitLayerTree does arrive).
    enum CommitLayerTreeMessageState { CommitLayerTreePending, NeedsDisplayDidRefresh, MissedCommit, Idle };
    CommitLayerTreeMessageState m_commitLayerTreeMessageState { Idle };

    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentMinimumSizeForAutoLayout;
    WebCore::IntSize m_lastSentSizeToContentAutoSizeMaximumSize;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    TransactionID m_pendingLayerTreeTransactionID;
    TransactionID m_lastLayerTreeTransactionID;
#if ASSERT_ENABLED
    TransactionID m_lastVisibleTransactionID;
#endif
    TransactionID m_transactionIDForPendingCACommit;
    TransactionID m_transactionIDForUnhidingContent;
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };
    
    unsigned m_countOfTransactionsWithNonEmptyLayerChanges { 0 };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(RemoteLayerTreeDrawingAreaProxy, DrawingAreaType::RemoteLayerTree)
