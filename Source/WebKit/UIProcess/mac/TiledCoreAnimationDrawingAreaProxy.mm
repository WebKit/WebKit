/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#import "ColorSpaceData.h"
#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>

namespace WebKit {
using namespace IPC;
using namespace WebCore;

TiledCoreAnimationDrawingAreaProxy::TiledCoreAnimationDrawingAreaProxy(WebPageProxy& webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiledCoreAnimation, webPageProxy)
    , m_isWaitingForDidUpdateGeometry(false)
{
}

TiledCoreAnimationDrawingAreaProxy::~TiledCoreAnimationDrawingAreaProxy()
{
    m_callbacks.invalidate(CallbackBase::Error::OwnerWasInvalidated);
}

void TiledCoreAnimationDrawingAreaProxy::deviceScaleFactorDidChange()
{
    m_webPageProxy.process().send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy.deviceScaleFactor()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy.isValid())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::colorSpaceDidChange()
{
    m_webPageProxy.process().send(Messages::DrawingArea::SetColorSpace(m_webPageProxy.colorSpace()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::viewLayoutSizeDidChange()
{
    if (!m_webPageProxy.isValid())
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

void TiledCoreAnimationDrawingAreaProxy::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&)
{
    // This should never be called.
    ASSERT_NOT_REACHED();
}

void TiledCoreAnimationDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    IntSize viewLayoutSize = m_webPageProxy.viewLayoutSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentViewLayoutSize != viewLayoutSize)
        sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID)
{
    Seconds activityStateUpdateTimeout = Seconds::fromMilliseconds(250);
    m_webPageProxy.process().connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::DidUpdateActivityState>(m_webPageProxy.pageID(), activityStateUpdateTimeout, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

void TiledCoreAnimationDrawingAreaProxy::intrinsicContentSizeDidChange(const IntSize& newIntrinsicContentSize)
{
    if (m_webPageProxy.viewLayoutSize().width() > 0)
        m_webPageProxy.intrinsicContentSizeDidChange(newIntrinsicContentSize);
}

void TiledCoreAnimationDrawingAreaProxy::willSendUpdateGeometry()
{
    m_lastSentViewLayoutSize = m_webPageProxy.viewLayoutSize();
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;
}

MachSendRight TiledCoreAnimationDrawingAreaProxy::createFence()
{
    if (!m_webPageProxy.isValid())
        return MachSendRight();

    RetainPtr<CAContext> rootLayerContext = [m_webPageProxy.acceleratedCompositingRootLayer() context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!m_webPageProxy.process().connection())
        return MachSendRight();

    // Don't fence if we have incoming synchronous messages, because we may not
    // be able to reply to the message until the fence times out.
    if (m_webPageProxy.process().connection()->hasIncomingSyncMessage())
        return MachSendRight();

    MachSendRight fencePort = MachSendRight::adopt([rootLayerContext createFencePort]);

    // Invalidate the fence if a synchronous message arrives while it's installed,
    // because we won't be able to reply during the fence-wait.
    uint64_t callbackID = m_webPageProxy.process().connection()->installIncomingSyncMessageCallback([rootLayerContext] {
        [rootLayerContext invalidateFences];
    });
    RefPtr<WebPageProxy> retainedPage = &m_webPageProxy;
    [CATransaction addCommitHandler:[callbackID, retainedPage] {
        if (!retainedPage->isValid())
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
    m_webPageProxy.process().send(Messages::DrawingArea::UpdateGeometry(m_size, true /* flushSynchronously */, createFence()), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::adjustTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.process().send(Messages::DrawingArea::AdjustTransientZoom(scale, origin), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::commitTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.process().send(Messages::DrawingArea::CommitTransientZoom(scale, origin), m_webPageProxy.pageID());
}

void TiledCoreAnimationDrawingAreaProxy::dispatchAfterEnsuringDrawing(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    if (!m_webPageProxy.isValid()) {
        callback(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    m_webPageProxy.process().send(Messages::DrawingArea::AddTransactionCallbackID(m_callbacks.put(WTFMove(callback), nullptr)), m_webPageProxy.pageID());
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
