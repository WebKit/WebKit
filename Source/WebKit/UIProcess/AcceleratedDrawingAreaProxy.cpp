/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2016 Igalia S.L.
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

#include "config.h"
#include "AcceleratedDrawingAreaProxy.h"

#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "LayerTreeContext.h"
#include "UpdateInfo.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"
#include <WebCore/Region.h>

#if PLATFORM(WAYLAND)
#include "WaylandCompositor.h"
#include <WebCore/PlatformDisplay.h>
#endif

namespace WebKit {
using namespace WebCore;

AcceleratedDrawingAreaProxy::AcceleratedDrawingAreaProxy(WebPageProxy& webPageProxy, WebProcessProxy& process)
    : DrawingAreaProxy(DrawingAreaTypeImpl, webPageProxy, process)
{
}

AcceleratedDrawingAreaProxy::~AcceleratedDrawingAreaProxy()
{
    // Make sure to exit accelerated compositing mode.
    if (isInAcceleratedCompositingMode())
        exitAcceleratedCompositingMode();
}

bool AcceleratedDrawingAreaProxy::alwaysUseCompositing() const
{
    return m_webPageProxy.preferences().acceleratedCompositingEnabled() && m_webPageProxy.preferences().forceCompositingMode();
}

void AcceleratedDrawingAreaProxy::dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!m_webPageProxy.isValid()) {
        callbackFunction(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    RunLoop::main().dispatch([callbackFunction = WTFMove(callbackFunction)] {
        callbackFunction(CallbackBase::Error::None);
    });
}

void AcceleratedDrawingAreaProxy::sizeDidChange()
{
    backingStoreStateDidChange(RespondImmediately);
}

void AcceleratedDrawingAreaProxy::deviceScaleFactorDidChange()
{
    backingStoreStateDidChange(RespondImmediately);
}

void AcceleratedDrawingAreaProxy::visibilityDidChange()
{
    // If we don't have a backing store, go ahead and mark the backing store as being changed so
    // that when paint we'll actually wait for something to paint and not flash white.
    if (m_layerTreeContext.isEmpty())
        backingStoreStateDidChange(DoNotRespondImmediately);
}

void AcceleratedDrawingAreaProxy::waitForBackingStoreUpdateOnNextPaint()
{
    m_hasReceivedFirstUpdate = true;
}

void AcceleratedDrawingAreaProxy::didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo& updateInfo, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_nextBackingStoreStateID);
    ASSERT_ARG(backingStoreStateID, backingStoreStateID > m_currentBackingStoreStateID);
    m_currentBackingStoreStateID = backingStoreStateID;

    m_isWaitingForDidUpdateBackingStoreState = false;

    // Stop the responsiveness timer that was started in sendUpdateBackingStoreState.
    process().responsivenessTimer().stop();

    if (layerTreeContext != m_layerTreeContext) {
        if (layerTreeContext.isEmpty() && !m_layerTreeContext.isEmpty()) {
            exitAcceleratedCompositingMode();
            ASSERT(m_layerTreeContext.isEmpty());
        } else if (!layerTreeContext.isEmpty() && m_layerTreeContext.isEmpty()) {
            enterAcceleratedCompositingMode(layerTreeContext);
            ASSERT(layerTreeContext == m_layerTreeContext);
        } else {
            updateAcceleratedCompositingMode(layerTreeContext);
            ASSERT(layerTreeContext == m_layerTreeContext);
        }
    }

    if (m_nextBackingStoreStateID != m_currentBackingStoreStateID)
        sendUpdateBackingStoreState(RespondImmediately);
    else {
        m_hasReceivedFirstUpdate = true;

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
        if (m_pendingNativeSurfaceHandleForCompositing) {
            setNativeSurfaceHandleForCompositing(m_pendingNativeSurfaceHandleForCompositing);
            m_pendingNativeSurfaceHandleForCompositing = 0;
        }
#endif
    }
}

void AcceleratedDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    enterAcceleratedCompositingMode(layerTreeContext);
}

void AcceleratedDrawingAreaProxy::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo& updateInfo)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    exitAcceleratedCompositingMode();
}

void AcceleratedDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    updateAcceleratedCompositingMode(layerTreeContext);
}

void AcceleratedDrawingAreaProxy::backingStoreStateDidChange(RespondImmediatelyOrNot respondImmediatelyOrNot)
{
    ++m_nextBackingStoreStateID;
    sendUpdateBackingStoreState(respondImmediatelyOrNot);
}

void AcceleratedDrawingAreaProxy::sendUpdateBackingStoreState(RespondImmediatelyOrNot respondImmediatelyOrNot)
{
    ASSERT(m_currentBackingStoreStateID < m_nextBackingStoreStateID);

    if (!m_webPageProxy.isValid())
        return;

    if (m_isWaitingForDidUpdateBackingStoreState)
        return;

    if (m_webPageProxy.viewSize().isEmpty() && !m_webPageProxy.useFixedLayout())
        return;

    m_isWaitingForDidUpdateBackingStoreState = respondImmediatelyOrNot == RespondImmediately;

    process().send(Messages::DrawingArea::UpdateBackingStoreState(m_nextBackingStoreStateID, respondImmediatelyOrNot == RespondImmediately, m_webPageProxy.deviceScaleFactor(), m_size, m_scrollOffset), m_webPageProxy.pageID());
    m_scrollOffset = IntSize();

    if (m_isWaitingForDidUpdateBackingStoreState) {
        // Start the responsiveness timer. We will stop it when we hear back from the WebProcess
        // in didUpdateBackingStoreState.
        process().responsivenessTimer().start();
    }

    if (m_isWaitingForDidUpdateBackingStoreState && !m_layerTreeContext.isEmpty()) {
        // Wait for the DidUpdateBackingStoreState message. Normally we do this in AcceleratedDrawingAreaProxy::paint, but that
        // function is never called when in accelerated compositing mode.
        waitForAndDispatchDidUpdateBackingStoreState();
    }
}

void AcceleratedDrawingAreaProxy::waitForAndDispatchDidUpdateBackingStoreState()
{
    ASSERT(m_isWaitingForDidUpdateBackingStoreState);

    if (!m_webPageProxy.isValid())
        return;
    if (process().state() == WebProcessProxy::State::Launching)
        return;
    if (!m_webPageProxy.isViewVisible())
        return;
#if PLATFORM(WAYLAND) && USE(EGL)
    // Never block the UI process in Wayland when waiting for DidUpdateBackingStoreState after a resize,
    // because the nested compositor needs to handle the web process requests that happens while resizing.
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland && isInAcceleratedCompositingMode())
        return;
#endif

    // FIXME: waitForAndDispatchImmediately will always return the oldest DidUpdateBackingStoreState message that
    // hasn't yet been processed. But it might be better to skip ahead to some other DidUpdateBackingStoreState
    // message, if multiple DidUpdateBackingStoreState messages are waiting to be processed. For instance, we could
    // choose the most recent one, or the one that is closest to our current size.

    // The timeout, in seconds, we use when waiting for a DidUpdateBackingStoreState message when we're asked to paint.
    process().connection()->waitForAndDispatchImmediately<Messages::DrawingAreaProxy::DidUpdateBackingStoreState>(m_webPageProxy.pageID(), Seconds::fromMilliseconds(500));
}

void AcceleratedDrawingAreaProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(alwaysUseCompositing() || !isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void AcceleratedDrawingAreaProxy::exitAcceleratedCompositingMode()
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = LayerTreeContext();
    m_webPageProxy.exitAcceleratedCompositingMode();
}

void AcceleratedDrawingAreaProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
void AcceleratedDrawingAreaProxy::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    if (!m_hasReceivedFirstUpdate) {
        m_pendingNativeSurfaceHandleForCompositing = handle;
        return;
    }
    process().send(Messages::DrawingArea::SetNativeSurfaceHandleForCompositing(handle), m_webPageProxy.pageID(), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void AcceleratedDrawingAreaProxy::destroyNativeSurfaceHandleForCompositing()
{
    if (m_pendingNativeSurfaceHandleForCompositing) {
        m_pendingNativeSurfaceHandleForCompositing = 0;
        return;
    }
    bool handled;
    process().sendSync(Messages::DrawingArea::DestroyNativeSurfaceHandleForCompositing(), Messages::DrawingArea::DestroyNativeSurfaceHandleForCompositing::Reply(handled), m_webPageProxy.pageID());
}
#endif

} // namespace WebKit
