/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(THREADED_SCROLLING)

#import "ColorSpaceData.h"
#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<TiledCoreAnimationDrawingAreaProxy> TiledCoreAnimationDrawingAreaProxy::create(WebPageProxy* webPageProxy)
{
    return adoptPtr(new TiledCoreAnimationDrawingAreaProxy(webPageProxy));
}

TiledCoreAnimationDrawingAreaProxy::TiledCoreAnimationDrawingAreaProxy(WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiledCoreAnimation, webPageProxy)
    , m_isWaitingForDidUpdateGeometry(false)
    , m_lastSentMinimumLayoutWidth(0)
{
}

TiledCoreAnimationDrawingAreaProxy::~TiledCoreAnimationDrawingAreaProxy()
{
}

void TiledCoreAnimationDrawingAreaProxy::deviceScaleFactorDidChange()
{
    m_webPageProxy->process()->send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy->deviceScaleFactor()), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxy::visibilityDidChange()
{
    if (!m_webPageProxy->isViewVisible())
        m_webPageProxy->process()->send(Messages::DrawingArea::SuspendPainting(), m_webPageProxy->pageID());
    else
        m_webPageProxy->process()->send(Messages::DrawingArea::ResumePainting(), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxy::layerHostingModeDidChange()
{
    m_webPageProxy->process()->send(Messages::DrawingArea::SetLayerHostingMode(m_webPageProxy->layerHostingMode()), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy->isValid())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::waitForPossibleGeometryUpdate()
{
    if (!m_isWaitingForDidUpdateGeometry)
        return;

    if (m_webPageProxy->process()->isLaunching())
        return;

    // The timeout, in seconds, we use when waiting for a DidUpdateGeometry message.
    static const double didUpdateBackingStoreStateTimeout = 0.5;
    m_webPageProxy->process()->connection()->waitForAndDispatchImmediately<Messages::DrawingAreaProxy::DidUpdateGeometry>(m_webPageProxy->pageID(), didUpdateBackingStoreStateTimeout);
}

void TiledCoreAnimationDrawingAreaProxy::colorSpaceDidChange()
{
    m_webPageProxy->process()->send(Messages::DrawingArea::SetColorSpace(m_webPageProxy->colorSpace()), m_webPageProxy->pageID());
}

void TiledCoreAnimationDrawingAreaProxy::minimumLayoutWidthDidChange()
{
    if (!m_webPageProxy->isValid())
        return;

    // We only want one UpdateGeometry message in flight at once, so if we've already sent one but
    // haven't yet received the reply we'll just return early here.
    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void TiledCoreAnimationDrawingAreaProxy::enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy->enterAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&)
{
    // This should never be called.
    ASSERT_NOT_REACHED();
}

void TiledCoreAnimationDrawingAreaProxy::updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext& layerTreeContext)
{
    m_webPageProxy->updateAcceleratedCompositingMode(layerTreeContext);
}

void TiledCoreAnimationDrawingAreaProxy::didUpdateGeometry(const IntSize& newIntrinsicContentSize)
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    double minimumLayoutWidth = m_webPageProxy->minimumLayoutWidth();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentMinimumLayoutWidth != minimumLayoutWidth)
        sendUpdateGeometry();

    if (minimumLayoutWidth > 0)
        m_webPageProxy->intrinsicContentSizeDidChange(newIntrinsicContentSize);
}

void TiledCoreAnimationDrawingAreaProxy::intrinsicContentSizeDidChange(const IntSize& newIntrinsicContentSize)
{
    if (m_webPageProxy->minimumLayoutWidth() > 0)
        m_webPageProxy->intrinsicContentSizeDidChange(newIntrinsicContentSize);
}

void TiledCoreAnimationDrawingAreaProxy::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);

    m_lastSentMinimumLayoutWidth = m_webPageProxy->minimumLayoutWidth();
    m_lastSentSize = m_size;
    m_webPageProxy->process()->send(Messages::DrawingArea::UpdateGeometry(m_size), m_webPageProxy->pageID());
    m_isWaitingForDidUpdateGeometry = true;
}

} // namespace WebKit

#endif // ENABLE(THREADED_SCROLLING)
