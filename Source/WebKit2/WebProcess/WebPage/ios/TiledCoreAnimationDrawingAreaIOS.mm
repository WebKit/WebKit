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
#import "TiledCoreAnimationDrawingAreaIOS.h"

#import "DrawingAreaProxyMessages.h"
#import "WebPage.h"
#import <QuartzCore/CAContext.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/MainFrame.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformCALayer.h>

using namespace WebCore;

namespace WebKit {

TiledCoreAnimationDrawingAreaIOS::~TiledCoreAnimationDrawingAreaIOS()
{
    m_layerFlushScheduler.invalidate();
}

TiledCoreAnimationDrawingAreaIOS::TiledCoreAnimationDrawingAreaIOS(WebPage* webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeTiledCoreAnimationIOS, webPage)
    , m_layerFlushScheduler(this)
{
    Page* page = webPage->corePage();

    page->settings().setForceCompositingMode(true);
    page->settings().setScrollingCoordinatorEnabled(true);
    page->settings().setDelegatesPageScaling(true);
    
    m_rootLayer = [CALayer layer];
    CGRect rootLayerFrame = m_webPage->bounds();
    m_rootLayer.get().frame = rootLayerFrame;
    m_rootLayer.get().opaque = YES;

    RetainPtr<NSMutableDictionary> options = adoptNS([[NSMutableDictionary alloc] init]);

    [options.get() setObject:@YES forKey:kCAContextIgnoresHitTest];

    m_context = [CAContext remoteContextWithOptions:options.get()];
    [m_context.get() setLevel:-CGFLOAT_MAX];
    
    [m_context.get() setLayer:m_rootLayer.get()];

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = [m_context.get() contextId];

    m_webPage->send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));

    page->settings().setShowDebugBorders(true);
    page->settings().setShowRepaintCounter(false);
}

void TiledCoreAnimationDrawingAreaIOS::setNeedsDisplay()
{
    notImplemented();
}

void TiledCoreAnimationDrawingAreaIOS::setNeedsDisplayInRect(const WebCore::IntRect&)
{
    notImplemented();
}

void TiledCoreAnimationDrawingAreaIOS::scroll(const IntRect&, const IntSize&)
{
    notImplemented();
}

void TiledCoreAnimationDrawingAreaIOS::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    CALayer *rootCompositingLayer = graphicsLayer ? graphicsLayer->platformLayer() : nil;
    if (!rootCompositingLayer)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    
    m_rootLayer.get().sublayers = [NSArray arrayWithObject:rootCompositingLayer];

    [CATransaction commit];
}

void TiledCoreAnimationDrawingAreaIOS::scheduleCompositingLayerFlush()
{
    m_layerFlushScheduler.schedule();
}

bool TiledCoreAnimationDrawingAreaIOS::shouldUseTiledBacking(const GraphicsLayer*) const
{
    return true;
}

void TiledCoreAnimationDrawingAreaIOS::notifyAnimationStarted(const GraphicsLayer*, double time)
{
}
    
void TiledCoreAnimationDrawingAreaIOS::notifyFlushRequired(const GraphicsLayer*)
{
}

void TiledCoreAnimationDrawingAreaIOS::paintContents(const GraphicsLayer*, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
}

float TiledCoreAnimationDrawingAreaIOS::minimumDocumentScale() const
{
    return 1.0;
}

bool TiledCoreAnimationDrawingAreaIOS::allowCompositingLayerVisualDegradation() const
{
    return false;
}

bool TiledCoreAnimationDrawingAreaIOS::flushLayers()
{
    @autoreleasepool {
        m_webPage->layoutIfNeeded();
        return m_webPage->corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes();
    }
}

void TiledCoreAnimationDrawingAreaIOS::updateGeometry(const IntSize& viewSize, const WebCore::IntSize& layerPosition)
{
    m_webPage->setSize(viewSize);
    m_webPage->layoutIfNeeded();
}

void TiledCoreAnimationDrawingAreaIOS::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
}

} // namespace WebKit
