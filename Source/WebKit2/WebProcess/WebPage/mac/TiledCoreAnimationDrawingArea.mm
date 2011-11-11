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
#import "TiledCoreAnimationDrawingArea.h"

#import "DrawingAreaProxyMessages.h"
#import "LayerTreeContext.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContext.h>
#import <WebKitSystemInterface.h>

@interface CATransaction (Details)
+ (void)synchronize;
@end

using namespace WebCore;

@interface WKContentLayer : CALayer {
    WebKit::WebPage *_webPage;
}

- (id)initWithWebPage:(WebKit::WebPage *)webPage;

@end

@implementation WKContentLayer

- (id)initWithWebPage:(WebKit::WebPage *)webPage
{
    self = [super init];
    if (self)
        self->_webPage = webPage;

    return self;
}

- (void)drawInContext:(CGContextRef)context
{
    _webPage->layoutIfNeeded();

    CGRect clipRect = CGContextGetClipBoundingBox(context);

    GraphicsContext graphicsContext(context);
    _webPage->drawRect(graphicsContext, enclosingIntRect(NSRectFromCGRect(clipRect)));
}

@end

namespace WebKit {

PassOwnPtr<TiledCoreAnimationDrawingArea> TiledCoreAnimationDrawingArea::create(WebPage* webPage, const WebPageCreationParameters& parameters)
{
    return adoptPtr(new TiledCoreAnimationDrawingArea(webPage, parameters));
}

TiledCoreAnimationDrawingArea::TiledCoreAnimationDrawingArea(WebPage* webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeTiledCoreAnimation, webPage)
{
    m_rootLayer = [CALayer layer];

    CGRect rootLayerFrame = m_webPage->bounds();
    m_rootLayer.get().frame = rootLayerFrame;
    m_rootLayer.get().opaque = YES;
    m_rootLayer.get().geometryFlipped = YES;

    // Give the root layer a background color so it's visible on screen.
    m_rootLayer.get().backgroundColor = CGColorCreateGenericRGB(1, 0, 0, 1);

    m_contentLayer.adoptNS([[WKContentLayer alloc] initWithWebPage:webPage]);
    m_contentLayer.get().frame = m_rootLayer.get().frame;

    [m_rootLayer.get() addSublayer:m_contentLayer.get()];

    mach_port_t serverPort = WebProcess::shared().compositingRenderServerPort();
    m_remoteLayerClient = WKCARemoteLayerClientMakeWithServerPort(serverPort);
    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), m_rootLayer.get());

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
    m_webPage->send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
}

TiledCoreAnimationDrawingArea::~TiledCoreAnimationDrawingArea()
{
}

void TiledCoreAnimationDrawingArea::setNeedsDisplay(const IntRect& rect)
{
    [m_contentLayer.get() setNeedsDisplayInRect:rect];
}

void TiledCoreAnimationDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    [m_contentLayer.get() setNeedsDisplayInRect:scrollRect];
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(GraphicsLayer*)
{
    // FIXME: Implement.
}

void TiledCoreAnimationDrawingArea::scheduleCompositingLayerSync()
{
    // FIXME: Implement
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize)
{
    m_webPage->setSize(viewSize);
    m_webPage->layoutIfNeeded();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    m_rootLayer.get().frame = CGRectMake(0, 0, viewSize.width(), viewSize.height());
    m_contentLayer.get().frame = CGRectMake(0, 0, viewSize.width(), viewSize.height());

    [CATransaction commit];
    
    [CATransaction flush];
    [CATransaction synchronize];

    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

} // namespace WebKit
