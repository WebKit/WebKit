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
#include "RemoteImageBufferSetIdentifier.h"
#include "RemoteLayerTreeHost.h"
#include "TransactionID.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>
#include <wtf/WeakHashMap.h>

namespace WebKit {
class RemoteLayerTreeDrawingAreaProxy;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::RemoteLayerTreeDrawingAreaProxy> : std::true_type { };
}

namespace WebKit {

class RemoteLayerTreeTransaction;
class RemotePageDrawingAreaProxy;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxy : public DrawingAreaProxy {
public:
    RemoteLayerTreeDrawingAreaProxy(WebPageProxy&, WebProcessProxy&);
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    virtual bool isRemoteLayerTreeDrawingAreaProxyMac() const { return false; }
    virtual bool isRemoteLayerTreeDrawingAreaProxyIOS() const { return false; }

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return *m_remoteLayerTreeHost; }
    std::unique_ptr<RemoteLayerTreeHost> detachRemoteLayerTreeHost();

    virtual std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const = 0;

    void acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier, const String& key, MonotonicTime startTime);
    void acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier, const String& key);

    // FIXME(site-isolation): These always return the root's IDs. Is that ok?
    TransactionID nextLayerTreeTransactionID() const { return m_webPageProxyProcessState.pendingLayerTreeTransactionID.next(); }
    TransactionID lastCommittedLayerTreeTransactionID() const { return m_webPageProxyProcessState.transactionIDForPendingCACommit; }

    virtual void didRefreshDisplay();
    virtual void setDisplayLinkWantsFullSpeedUpdates(bool) { }

    bool hasDebugIndicator() const { return !!m_debugIndicatorLayerTreeHost; }

    CALayer *layerWithIDForTesting(WebCore::PlatformLayerIdentifier) const;

    void viewWillStartLiveResize() final;
    void viewWillEndLiveResize() final;

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    void animationsWereAddedToNode(RemoteLayerTreeNode&);
    void animationsWereRemovedFromNode(RemoteLayerTreeNode&);
    Seconds acceleratedTimelineTimeOrigin() const { return m_acceleratedTimelineTimeOrigin; }
    MonotonicTime animationCurrentTime() const { return m_animationCurrentTime; }
#endif

    // For testing.
    unsigned countOfTransactionsWithNonEmptyLayerChanges() const { return m_countOfTransactionsWithNonEmptyLayerChanges; }

protected:
    void updateDebugIndicatorPosition();

    bool shouldCoalesceVisualEditorStateUpdates() const override { return true; }

private:

    void remotePageProcessDidTerminate(WebCore::ProcessIdentifier) final;
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

    void addRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy&) final;
    void removeRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Message handlers
    // FIXME(site-isolation): We really want a Connection parameter to all of these (including
    // on the DrawingAreaProxy base class), and make sure we filter them
    // appropriately. Can we enforce that?

    // FIXME(site-isolation): We currently allow this from any subframe process and it overwrites
    // the main frame's request. We should either ignore subframe requests, or
    // properly track all the requested and filter displayDidRefresh callback rates
    // per-frame.
    virtual void setPreferredFramesPerSecond(WebCore::FramesPerSecond) { }

    void willCommitLayerTree(IPC::Connection&, TransactionID);
    void commitLayerTreeNotTriggered(IPC::Connection&, TransactionID);
    void commitLayerTree(IPC::Connection&, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>&, HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&&);
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
    struct ProcessState {
        CommitLayerTreeMessageState commitLayerTreeMessageState { Idle };
        TransactionID lastLayerTreeTransactionID;
        TransactionID pendingLayerTreeTransactionID;
#if ASSERT_ENABLED
        TransactionID lastVisibleTransactionID;
#endif
        TransactionID transactionIDForPendingCACommit;
        ActivityStateChangeID activityStateChangeID { ActivityStateChangeAsynchronous };
    };

    ProcessState& processStateForConnection(IPC::Connection&);
    void didRefreshDisplay(IPC::Connection*);
    void didRefreshDisplay(ProcessState&, IPC::Connection&);
    bool maybePauseDisplayRefreshCallbacks();

    ProcessState m_webPageProxyProcessState;
    WeakHashMap<RemotePageDrawingAreaProxy, ProcessState> m_remotePageProcessState;

    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentMinimumSizeForAutoLayout;
    WebCore::IntSize m_lastSentSizeToContentAutoSizeMaximumSize;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    IPC::AsyncReplyID m_replyForUnhidingContent;

    unsigned m_countOfTransactionsWithNonEmptyLayerChanges { 0 };

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    Seconds m_acceleratedTimelineTimeOrigin;
    MonotonicTime m_animationCurrentTime;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(RemoteLayerTreeDrawingAreaProxy, DrawingAreaType::RemoteLayerTree)
