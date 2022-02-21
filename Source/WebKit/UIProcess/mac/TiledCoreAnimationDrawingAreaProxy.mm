/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TiledCoreAnimationDrawingAreaProxy.h"

#if !PLATFORM(IOS_FAMILY)

#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>

namespace WebKit {
using namespace IPC;
using namespace WebCore;

TiledCoreAnimationDrawingAreaProxy::TiledCoreAnimationDrawingAreaProxy(WebPageProxy& webPageProxy, WebProcessProxy& process)
    : DrawingAreaProxy(DrawingAreaType::TiledCoreAnimation, webPageProxy, process)
    , m_isWaitingForDidUpdateGeometry(false)
{
}

TiledCoreAnimationDrawingAreaProxy::~TiledCoreAnimationDrawingAreaProxy()
{
    m_callbacks.invalidate(CallbackBase::Error::OwnerWasInvalidated);
}

void TiledCoreAnimationDrawingAreaProxy::deviceScaleFactorDidChange()
{
    send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy.deviceScaleFactor()));
}

void TiledCoreAnimationDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::colorSpaceDidChange()
{
    send(Messages::DrawingArea::SetColorSpace(m_webPageProxy.colorSpace()));
}

void TiledCoreAnimationDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didFirstLayerFlush(uint64_t /* backingStoreStateID */, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.didFirstLayerFlush(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    IntSize minimumSizeForAutoLayout = m_webPageProxy.minimumSizeForAutoLayout();
    IntSize sizeToContentAutoSizeMaximumSize = m_webPageProxy.sizeToContentAutoSizeMaximumSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentMinimumSizeForAutoLayout != minimumSizeForAutoLayout || m_lastSentSizeToContentAutoSizeMaximumSize != sizeToContentAutoSizeMaximumSize)
        sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID)
{
    Seconds activityStateUpdateTimeout = Seconds::fromMilliseconds(250);
    process().connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::DidUpdateActivityState>(m_webPageProxy.webPageID(), activityStateUpdateTimeout, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

void TiledCoreAnimationDrawingAreaProxy::willSendUpdateGeometry()
{
    m_lastSentMinimumSizeForAutoLayout = m_webPageProxy.minimumSizeForAutoLayout();
    m_lastSentSizeToContentAutoSizeMaximumSize = m_webPageProxy.sizeToContentAutoSizeMaximumSize();
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;
}

MachSendRight TiledCoreAnimationDrawingAreaProxy::createFence()
{
    if (!m_webPageProxy.hasRunningProcess())
        return MachSendRight();

    RetainPtr<CAContext> rootLayerContext = [m_webPageProxy.acceleratedCompositingRootLayer() context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!process().connection())
        return MachSendRight();

    // Don't fence if we have incoming synchronous messages, because we may not
    // be able to reply to the message until the fence times out.
    if (process().connection()->hasIncomingSyncMessage())
        return MachSendRight();

    MachSendRight fencePort = MachSendRight::adopt([rootLayerContext createFencePort]);

    // Invalidate the fence if a synchronous message arrives while it's installed,
    // because we won't be able to reply during the fence-wait.
    uint64_t callbackID = process().connection()->installIncomingSyncMessageCallback([rootLayerContext] {
        [rootLayerContext invalidateFences];
    });
    RefPtr<WebPageProxy> retainedPage = &m_webPageProxy;
    [CATransaction addCommitHandler:[callbackID, retainedPage] {
        if (!retainedPage->hasRunningProcess())
            return;
        if (Connection* connection = retainedPage->process().connection())
            connection->uninstallIncomingSyncMessageCallback(callbackID);
    } forPhase:kCATransactionPhasePostCommit];

    return fencePort;
}

void TiledCoreAnimationDrawingAreaProxy::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);

    willSendUpdateGeometry();
    send(Messages::DrawingArea::UpdateGeometry(m_size, true /* flushSynchronously */, createFence()));
}

void TiledCoreAnimationDrawingAreaProxy::adjustTransientZoom(double scale, FloatPoint origin)
{
    send(Messages::DrawingArea::AdjustTransientZoom(scale, origin));
}

void TiledCoreAnimationDrawingAreaProxy::commitTransientZoom(double scale, FloatPoint origin)
{
    send(Messages::DrawingArea::CommitTransientZoom(scale, origin));
}

void TiledCoreAnimationDrawingAreaProxy::dispatchAfterEnsuringDrawing(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    if (!m_webPageProxy.hasRunningProcess()) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    send(Messages::DrawingArea::AddTransactionCallbackID(m_callbacks.put(WTFMove(callback), nullptr)));
}

void TiledCoreAnimationDrawingAreaProxy::dispatchPresentationCallbacksAfterFlushingLayers(const Vector<CallbackID>& callbackIDs)
{
    for (auto& callbackID : callbackIDs) {
        if (auto callback = m_callbacks.take<VoidCallback>(callbackID))
            callback->performCallback();
    }
}

} // namespace WebKit

#endif // !PLATFORM(IOS_FAMILY)
