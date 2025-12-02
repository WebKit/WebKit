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
#include "ImageBufferSetIdentifier.h"
#include "RemoteLayerTreeHost.h"
#include "TransactionID.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakHashMap.h>

namespace WebKit {

class RemoteLayerTreeTransaction;
class RemotePageDrawingAreaProxy;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorTransaction;
struct MainFrameData;
struct PageData;
struct RemoteLayerTreeCommitBundle;

#if ENABLE(THREADED_ANIMATIONS)
class RemoteAnimationStack;
class RemoteAnimationTimeline;
#endif

class RemoteLayerTreeDrawingAreaProxy : public DrawingAreaProxy, public RefCounted<RemoteLayerTreeDrawingAreaProxy> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDrawingAreaProxy);
    WTF_MAKE_NONCOPYABLE(RemoteLayerTreeDrawingAreaProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeDrawingAreaProxy);
public:
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return *m_remoteLayerTreeHost; }
    std::unique_ptr<RemoteLayerTreeHost> detachRemoteLayerTreeHost();

    virtual std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const = 0;

    void acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier, const String& key, MonotonicTime startTime);
    void acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier, const String& key);

    TransactionID nextMainFrameLayerTreeTransactionID() const;
    TransactionID lastCommittedMainFrameLayerTreeTransactionID() const;

    virtual void didRefreshDisplay();
    virtual void setDisplayLinkWantsFullSpeedUpdates(bool) { }

    bool hasDebugIndicator() const { return !!m_debugIndicatorLayerTreeHost; }

    CALayer *layerWithIDForTesting(WebCore::PlatformLayerIdentifier) const;

    void viewWillStartLiveResize() final;
    void viewWillEndLiveResize() final;

#if ENABLE(THREADED_ANIMATIONS)
    void animationsWereAddedToNode(RemoteLayerTreeNode&);
    void animationsWereRemovedFromNode(RemoteLayerTreeNode&);
    void updateTimelineRegistration(WebCore::ProcessIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>&, MonotonicTime);
    RefPtr<const RemoteAnimationTimeline> timeline(const TimelineID&) const;
    RefPtr<const RemoteAnimationStack> animationStackForNodeWithIDForTesting(WebCore::PlatformLayerIdentifier) const;
#endif

    // For testing.
    unsigned countOfTransactionsWithNonEmptyLayerChanges() const { return m_countOfTransactionsWithNonEmptyLayerChanges; }

#if ENABLE(TOUCH_EVENT_REGIONS)
    WebCore::TrackingType eventTrackingTypeForPoint(WebCore::EventTrackingRegions::EventType, WebCore::IntPoint);
#endif

    void drawSlowFrameIndicator(WebCore::GraphicsContext&);

    struct CommitLayerTreePending {
        size_t requestedNotifyPendingCommitLayerTree { 1 };
        size_t requestedCommitLayerTree { 1 };
        bool missedDisplayDidRefresh { false };
    };
    struct NeedsDisplayDidRefresh { };
    struct Idle { };

    bool allowMultipleCommitLayerTreePending();

protected:
    RemoteLayerTreeDrawingAreaProxy(WebPageProxy&, WebProcessProxy&);

    void updateDebugIndicatorPosition();

    bool shouldCoalesceVisualEditorStateUpdates() const override { return true; }

    // displayDidRefresh is sent to the WebProcess, and it responds
    // with a commitLayerTree message (ideally before the next
    // displayDidRefresh, otherwise we mark it as missed and send
    // it when commitLayerTree does arrive).
    struct ProcessState {
        WTF_MAKE_NONCOPYABLE(ProcessState);
        ProcessState() = default;
        ProcessState(ProcessState&&) = default;
        ProcessState& operator=(ProcessState&&) = default;

        bool canSendDisplayDidRefresh(RemoteLayerTreeDrawingAreaProxy&);

        Variant<Idle, CommitLayerTreePending, NeedsDisplayDidRefresh> commitLayerTreeMessageState;
        std::optional<TransactionID> pendingLayerTreeTransactionID;
        std::optional<TransactionID> committedLayerTreeTransactionID;
    };

    ProcessState& processStateForConnection(IPC::Connection&);
    const ProcessState& processStateForIdentifier(WebCore::ProcessIdentifier) const;
    IPC::Connection* connectionForIdentifier(WebCore::ProcessIdentifier);
    void forEachProcessState(NOESCAPE Function<void(ProcessState&, WebProcessProxy&)>&&);

    std::unique_ptr<RemoteLayerTreeHost> m_remoteLayerTreeHost;
private:
#if ENABLE(TILED_CA_DRAWING_AREA)
    DrawingAreaType type() const final { return DrawingAreaType::RemoteLayerTree; }
#endif

    void remotePageProcessDidTerminate(WebCore::ProcessIdentifier) final;
    void sizeDidChange() final;
    void deviceScaleFactorDidChange(CompletionHandler<void()>&&) final;
    void minimumSizeForAutoLayoutDidChange() final;
    void sizeToContentAutoSizeMaximumSizeDidChange() final;
    void didUpdateGeometry();
    std::span<IPC::ReceiverName> messageReceiverNames() const final;

    virtual void scheduleDisplayRefreshCallbacks() { }
    virtual void pauseDisplayRefreshCallbacks() { }

    virtual void dispatchSetObscuredContentInsets() { }

    float indicatorScale(WebCore::IntSize contentsSize) const;
    void updateDebugIndicator() final;
    void updateDebugIndicator(WebCore::IntSize contentsSize, bool rootLayerChanged, float scale, const WebCore::IntPoint& scrollPosition);
    void initializeDebugIndicator();

    void initializeSlowFrameIndicator();
    void updateSlowFrameIndicator();

    void waitForDidUpdateActivityState(ActivityStateChangeID) final;
    void hideContentUntilPendingUpdate() final;
    void hideContentUntilAnyUpdate() final;
    bool hasVisibleContent() const final;

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
    virtual void setPreferredFramesPerSecond(IPC::Connection&, WebCore::FramesPerSecond) { }

    void notifyPendingCommitLayerTree(IPC::Connection&, std::optional<TransactionID>);
    void commitLayerTree(IPC::Connection&, const RemoteLayerTreeCommitBundle&, HashMap<ImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&&);
    void commitLayerTreeTransaction(IPC::Connection&, const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&, const std::optional<MainFrameData>&, const PageData&, const TransactionID&);
    virtual void didCommitLayerTree(IPC::Connection&, const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&, const std::optional<MainFrameData>&, const TransactionID&) { }

    void asyncSetLayerContents(WebCore::PlatformLayerIdentifier, RemoteLayerBackingStoreProperties&&);

    void sendUpdateGeometry();

    bool m_isWaitingForDidUpdateGeometry { false };

    void didRefreshDisplay(IPC::Connection*);
    IPC::Error didRefreshDisplay(ProcessState&, IPC::Connection&);
    bool maybePauseDisplayRefreshCallbacks();

    ProcessState m_webPageProxyProcessState;
    HashMap<WebCore::ProcessIdentifier, ProcessState> m_remotePageProcessState;

    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentMinimumSizeForAutoLayout;
    WebCore::IntSize m_lastSentSizeToContentAutoSizeMaximumSize;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    RetainPtr<CALayer> m_slowFrameIndicatorLayer;
    Deque<Seconds> m_frameDurations;

    Markable<IPC::AsyncReplyID> m_replyForUnhidingContent;
    bool m_hasDetachedRootLayer { false };

    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };

    unsigned m_countOfTransactionsWithNonEmptyLayerChanges { 0 };
};

TextStream& operator<<(TextStream&, const RemoteLayerTreeDrawingAreaProxy::Idle&);
TextStream& operator<<(TextStream&, const RemoteLayerTreeDrawingAreaProxy::NeedsDisplayDidRefresh&);
TextStream& operator<<(TextStream&, const RemoteLayerTreeDrawingAreaProxy::CommitLayerTreePending&);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(RemoteLayerTreeDrawingAreaProxy, DrawingAreaType::RemoteLayerTree)
