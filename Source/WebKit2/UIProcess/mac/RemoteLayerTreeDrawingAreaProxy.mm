/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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
#import <WebCore/WebCoreCALayerExtras.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeDrawingAreaProxy::RemoteLayerTreeDrawingAreaProxy(WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeRemoteLayerTree, webPageProxy)
    , m_remoteLayerTreeHost()
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

FloatRect RemoteLayerTreeDrawingAreaProxy::scaledExposedRect() const
{
#if PLATFORM(IOS)
    return m_webPageProxy->exposedContentRect();
#else
    FloatRect scaledExposedRect = exposedRect();
    float scale = 1 / m_webPageProxy->pageScaleFactor();
    scaledExposedRect.scale(scale, scale);
    return scaledExposedRect;
#endif
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
    if (m_remoteLayerTreeHost.updateLayerTree(layerTreeTransaction))
        m_webPageProxy->setAcceleratedCompositingRootLayer(m_remoteLayerTreeHost.rootLayer());

#if ENABLE(ASYNC_SCROLLING)
    m_webPageProxy->scrollingCoordinatorProxy()->updateScrollingTree(scrollingTreeTransaction);
#endif
#if PLATFORM(IOS)
    m_webPageProxy->didCommitLayerTree(layerTreeTransaction);
#endif

    showDebugIndicator(m_webPageProxy->preferences().tiledScrollingIndicatorVisible());

    if (m_debugIndicatorLayerTreeHost) {
        float scale = indicatorScale(layerTreeTransaction.contentsSize());
        bool rootLayerChanged = m_debugIndicatorLayerTreeHost->updateLayerTree(layerTreeTransaction, scale);
        updateDebugIndicator(layerTreeTransaction.contentsSize(), rootLayerChanged, scale);
        m_debugIndicatorLayerTreeHost->rootLayer().name = @"Indicator host root";
    }
}

static const float indicatorInset = 10;

#if PLATFORM(MAC)
void RemoteLayerTreeDrawingAreaProxy::setExposedRect(const WebCore::FloatRect& r)
{
    DrawingAreaProxy::setExposedRect(r);
    updateDebugIndicatorPosition();
}
#endif

FloatPoint RemoteLayerTreeDrawingAreaProxy::indicatorLocation() const
{
    if (m_webPageProxy->delegatesScrolling()) {
#if PLATFORM(IOS)
        FloatPoint tiledMapLocation = m_webPageProxy->unobscuredContentRect().location();
        float absoluteInset = indicatorInset / m_webPageProxy->displayedContentScale();
        tiledMapLocation += FloatSize(absoluteInset, absoluteInset);
#else
        FloatPoint tiledMapLocation = exposedRect().location();
        tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
        float scale = 1 / m_webPageProxy->pageScaleFactor();;
        tiledMapLocation.scale(scale, scale);
#endif
        return tiledMapLocation;
    }
    
    return FloatPoint(indicatorInset, indicatorInset);
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicatorPosition()
{
    if (!m_tileMapHostLayer)
        return;

    [m_tileMapHostLayer setPosition:indicatorLocation()];
}

float RemoteLayerTreeDrawingAreaProxy::indicatorScale(IntSize contentsSize) const
{
    // Pick a good scale.
    IntSize viewSize = m_webPageProxy->viewSize();

    float scale = 1;
    if (!contentsSize.isEmpty()) {
        float widthScale = std::min<float>((viewSize.width() - 2 * indicatorInset) / contentsSize.width(), 0.05);
        scale = std::min(widthScale, static_cast<float>(viewSize.height() - 2 * indicatorInset) / contentsSize.height());
    }
    
    return scale;
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicator()
{
    // FIXME: we should also update live information during scale.
    updateDebugIndicatorPosition();
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicator(IntSize contentsSize, bool rootLayerChanged, float scale)
{
    // Make sure we're the last sublayer.
    CALayer *rootLayer = m_remoteLayerTreeHost.rootLayer();
    [m_tileMapHostLayer removeFromSuperlayer];
    [rootLayer addSublayer:m_tileMapHostLayer.get()];

    // Pick a good scale.
    IntSize viewSize = m_webPageProxy->viewSize();

    [m_tileMapHostLayer setBounds:FloatRect(FloatPoint(), contentsSize)];
    [m_tileMapHostLayer setPosition:indicatorLocation()];
    [m_tileMapHostLayer setTransform:CATransform3DMakeScale(scale, scale, 1)];

    if (rootLayerChanged) {
        [m_tileMapHostLayer setSublayers:@[]];
        [m_tileMapHostLayer addSublayer:m_debugIndicatorLayerTreeHost->rootLayer()];
        [m_tileMapHostLayer addSublayer:m_exposedRectIndicatorLayer.get()];
    }
    
    const float indicatorBorderWidth = 1;
    float counterScaledBorder = indicatorBorderWidth / scale;

    [m_exposedRectIndicatorLayer setBorderWidth:counterScaledBorder];

    if (m_webPageProxy->delegatesScrolling()) {
        FloatRect scaledExposedRect = this->scaledExposedRect();
        [m_exposedRectIndicatorLayer setPosition:scaledExposedRect.location()];
        [m_exposedRectIndicatorLayer setBounds:FloatRect(FloatPoint(), scaledExposedRect.size())];
    } else {
        // FIXME: Get the correct scroll position.
        [m_exposedRectIndicatorLayer setBounds:FloatRect(FloatPoint(), viewSize)];
    }
}

void RemoteLayerTreeDrawingAreaProxy::showDebugIndicator(bool show)
{
    if (show == !!m_debugIndicatorLayerTreeHost)
        return;
    
    if (!show) {
        [m_tileMapHostLayer removeFromSuperlayer];
        m_tileMapHostLayer = nullptr;
        m_exposedRectIndicatorLayer = nullptr;
        m_debugIndicatorLayerTreeHost = nullptr;
        return;
    }
    
    m_debugIndicatorLayerTreeHost = std::make_unique<RemoteLayerTreeHost>();
    m_debugIndicatorLayerTreeHost->setIsDebugLayerTreeHost(true);

    m_tileMapHostLayer = adoptNS([[CALayer alloc] init]);
    [m_tileMapHostLayer setName:@"Tile map host"];
    [m_tileMapHostLayer web_disableAllActions];
    [m_tileMapHostLayer setAnchorPoint:CGPointZero];
    [m_tileMapHostLayer setOpacity:0.8];
    [m_tileMapHostLayer setMasksToBounds:YES];
    [m_tileMapHostLayer setBorderWidth:2];

    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    {
        const CGFloat components[] = { 1, 1, 1, 0.6 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));
        [m_tileMapHostLayer setBackgroundColor:color.get()];

        const CGFloat borderCmponents[] = { 0, 0, 0, 1 };
        RetainPtr<CGColorRef> borderColor = adoptCF(CGColorCreate(colorSpace.get(), borderCmponents));
        [m_tileMapHostLayer setBorderColor:borderColor.get()];
    }
    
    m_exposedRectIndicatorLayer = adoptNS([[CALayer alloc] init]);
    [m_exposedRectIndicatorLayer web_disableAllActions];
    [m_exposedRectIndicatorLayer setAnchorPoint:CGPointZero];

    {
        const CGFloat components[] = { 0, 1, 0, 1 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));
        [m_exposedRectIndicatorLayer setBorderColor:color.get()];
    }
}

} // namespace WebKit
