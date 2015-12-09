/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedDrawingAreaProxy.h"

#include "CoordinatedLayerTreeHostProxy.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "LayerTreeContext.h"
#include "UpdateInfo.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"
#include <WebCore/Region.h>

using namespace WebCore;

namespace WebKit {

CoordinatedDrawingAreaProxy::CoordinatedDrawingAreaProxy(WebPageProxy& webPageProxy)
#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    : DrawingAreaProxy(DrawingAreaTypeCoordinated, webPageProxy)
#else
    : DrawingAreaProxy(DrawingAreaTypeImpl, webPageProxy)
#endif
    , m_currentBackingStoreStateID(0)
    , m_nextBackingStoreStateID(0)
    , m_isWaitingForDidUpdateBackingStoreState(false)
    , m_hasReceivedFirstUpdate(false)
{
    // Construct the proxy early to allow messages to be sent to the web process while AC is entered there.
    m_coordinatedLayerTreeHostProxy = std::make_unique<CoordinatedLayerTreeHostProxy>(this);
}

CoordinatedDrawingAreaProxy::~CoordinatedDrawingAreaProxy()
{
    // Make sure to exit accelerated compositing mode.
    if (isInAcceleratedCompositingMode())
        exitAcceleratedCompositingMode();
}

void CoordinatedDrawingAreaProxy::updateViewport()
{
    m_webPageProxy.setViewNeedsDisplay(viewportVisibleRect());
}

WebCore::IntRect CoordinatedDrawingAreaProxy::contentsRect() const
{
    return IntRect(IntPoint::zero(), m_webPageProxy.viewSize());
}

void CoordinatedDrawingAreaProxy::sizeDidChange()
{
    backingStoreStateDidChange(RespondImmediately);
}

void CoordinatedDrawingAreaProxy::deviceScaleFactorDidChange()
{
    backingStoreStateDidChange(RespondImmediately);
}

void CoordinatedDrawingAreaProxy::visibilityDidChange()
{
    // If we don't have a backing store, go ahead and mark the backing store as being changed so
    // that when paint we'll actually wait for something to paint and not flash white.
    if (m_layerTreeContext.isEmpty())
        backingStoreStateDidChange(DoNotRespondImmediately);
}

void CoordinatedDrawingAreaProxy::waitForBackingStoreUpdateOnNextPaint()
{
    m_hasReceivedFirstUpdate = true;
}

void CoordinatedDrawingAreaProxy::didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo& updateInfo, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_nextBackingStoreStateID);
    ASSERT_ARG(backingStoreStateID, backingStoreStateID > m_currentBackingStoreStateID);
    m_currentBackingStoreStateID = backingStoreStateID;

    m_isWaitingForDidUpdateBackingStoreState = false;

    // Stop the responsiveness timer that was started in sendUpdateBackingStoreState.
    m_webPageProxy.process().responsivenessTimer().stop();

    if (layerTreeContext != m_layerTreeContext) {
        if (!m_layerTreeContext.isEmpty()) {
            exitAcceleratedCompositingMode();
            ASSERT(m_layerTreeContext.isEmpty());
        }

        if (!layerTreeContext.isEmpty()) {
            enterAcceleratedCompositingMode(layerTreeContext);
            ASSERT(layerTreeContext == m_layerTreeContext);
        }
    }

    if (m_nextBackingStoreStateID != m_currentBackingStoreStateID)
        sendUpdateBackingStoreState(RespondImmediately);
    else
        m_hasReceivedFirstUpdate = true;
}

void CoordinatedDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    enterAcceleratedCompositingMode(layerTreeContext);
}

void CoordinatedDrawingAreaProxy::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo& updateInfo)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    exitAcceleratedCompositingMode();
}

void CoordinatedDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    updateAcceleratedCompositingMode(layerTreeContext);
}

void CoordinatedDrawingAreaProxy::backingStoreStateDidChange(RespondImmediatelyOrNot respondImmediatelyOrNot)
{
    ++m_nextBackingStoreStateID;
    sendUpdateBackingStoreState(respondImmediatelyOrNot);
}

void CoordinatedDrawingAreaProxy::sendUpdateBackingStoreState(RespondImmediatelyOrNot respondImmediatelyOrNot)
{
    ASSERT(m_currentBackingStoreStateID < m_nextBackingStoreStateID);

    if (!m_webPageProxy.isValid())
        return;

    if (m_isWaitingForDidUpdateBackingStoreState)
        return;

    if (m_webPageProxy.viewSize().isEmpty() && !m_webPageProxy.useFixedLayout())
        return;

    m_isWaitingForDidUpdateBackingStoreState = respondImmediatelyOrNot == RespondImmediately;

    m_webPageProxy.process().send(Messages::DrawingArea::UpdateBackingStoreState(m_nextBackingStoreStateID, respondImmediatelyOrNot == RespondImmediately, m_webPageProxy.deviceScaleFactor(), m_size, m_scrollOffset), m_webPageProxy.pageID());
    m_scrollOffset = IntSize();

    if (m_isWaitingForDidUpdateBackingStoreState) {
        // Start the responsiveness timer. We will stop it when we hear back from the WebProcess
        // in didUpdateBackingStoreState.
        m_webPageProxy.process().responsivenessTimer().start();
    }

    if (m_isWaitingForDidUpdateBackingStoreState && !m_layerTreeContext.isEmpty()) {
        // Wait for the DidUpdateBackingStoreState message. Normally we do this in CoordinatedDrawingAreaProxy::paint, but that
        // function is never called when in accelerated compositing mode.
        waitForAndDispatchDidUpdateBackingStoreState();
    }
}

void CoordinatedDrawingAreaProxy::waitForAndDispatchDidUpdateBackingStoreState()
{
    ASSERT(m_isWaitingForDidUpdateBackingStoreState);

    if (!m_webPageProxy.isValid())
        return;
    if (m_webPageProxy.process().state() == WebProcessProxy::State::Launching)
        return;

    // FIXME: waitForAndDispatchImmediately will always return the oldest DidUpdateBackingStoreState message that
    // hasn't yet been processed. But it might be better to skip ahead to some other DidUpdateBackingStoreState
    // message, if multiple DidUpdateBackingStoreState messages are waiting to be processed. For instance, we could
    // choose the most recent one, or the one that is closest to our current size.

    // The timeout, in seconds, we use when waiting for a DidUpdateBackingStoreState message when we're asked to paint.
    m_webPageProxy.process().connection()->waitForAndDispatchImmediately<Messages::DrawingAreaProxy::DidUpdateBackingStoreState>(m_webPageProxy.pageID(), std::chrono::milliseconds(500));
}

void CoordinatedDrawingAreaProxy::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void CoordinatedDrawingAreaProxy::setVisibleContentsRect(const WebCore::FloatRect& visibleContentsRect, const WebCore::FloatPoint& trajectoryVector)
{
    m_coordinatedLayerTreeHostProxy->setVisibleContentsRect(visibleContentsRect, trajectoryVector);
}

void CoordinatedDrawingAreaProxy::exitAcceleratedCompositingMode()
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = LayerTreeContext();
    m_webPageProxy.exitAcceleratedCompositingMode();
}

void CoordinatedDrawingAreaProxy::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
