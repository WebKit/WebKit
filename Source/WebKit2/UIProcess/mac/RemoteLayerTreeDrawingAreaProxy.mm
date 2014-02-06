/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDrawingAreaProxy.h"

#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "DrawingAreaMessages.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

namespace WebKit {

RemoteLayerTreeDrawingAreaProxy::RemoteLayerTreeDrawingAreaProxy(WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeRemoteLayerTree, webPageProxy)
    , m_remoteLayerTreeHost(webPageProxy)
    , m_isWaitingForDidUpdateGeometry(false)
{
    m_webPageProxy->process().addMessageReceiver(Messages::RemoteLayerTreeDrawingAreaProxy::messageReceiverName(), m_webPageProxy->pageID(), *this);
}

RemoteLayerTreeDrawingAreaProxy::~RemoteLayerTreeDrawingAreaProxy()
{
    m_webPageProxy->process().removeMessageReceiver(Messages::RemoteLayerTreeDrawingAreaProxy::messageReceiverName(), m_webPageProxy->pageID());
}

void RemoteLayerTreeDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy->isValid())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::deviceScaleFactorDidChange()
{
    m_webPageProxy->process().send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy->deviceScaleFactor()), m_webPageProxy->pageID());
}

void RemoteLayerTreeDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentLayerPosition != m_layerPosition)
        sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sendUpdateGeometry()
{
    m_lastSentSize = m_size;
    m_lastSentLayerPosition = m_layerPosition;
    m_webPageProxy->process().send(Messages::DrawingArea::UpdateGeometry(m_size, m_layerPosition), m_webPageProxy->pageID());
    m_isWaitingForDidUpdateGeometry = true;
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(const RemoteLayerTreeTransaction& layerTreeTransaction, const RemoteScrollingCoordinatorTransaction& scrollingTreeTransaction)
{
    m_remoteLayerTreeHost.updateLayerTree(layerTreeTransaction);
#if ENABLE(ASYNC_SCROLLING)
    m_webPageProxy->scrollingCoordinatorProxy()->updateScrollingTree(scrollingTreeTransaction);
#endif
#if PLATFORM(IOS)
    m_webPageProxy->didCommitLayerTree(layerTreeTransaction);
#endif
}

} // namespace WebKit
