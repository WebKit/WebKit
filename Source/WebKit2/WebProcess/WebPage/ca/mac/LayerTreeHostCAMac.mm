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
#import "LayerTreeHostCAMac.h"

#import "WebProcess.h"
#import <QuartzCore/CATransaction.h>
#import <WebCore/GraphicsLayer.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

@interface CATransaction (Details)
+ (void)synchronize;
@end

namespace WebKit {

PassRefPtr<LayerTreeHostCAMac> LayerTreeHostCAMac::create(WebPage* webPage)
{
    RefPtr<LayerTreeHostCAMac> host = adoptRef(new LayerTreeHostCAMac(webPage));
    host->initialize();
    return host.release();
}

LayerTreeHostCAMac::LayerTreeHostCAMac(WebPage* webPage)
    : LayerTreeHostCA(webPage)
    , m_layerFlushScheduler(this)
{
}

LayerTreeHostCAMac::~LayerTreeHostCAMac()
{
    ASSERT(!m_remoteLayerClient);
}

void LayerTreeHostCAMac::platformInitialize(LayerTreeContext& layerTreeContext)
{
    mach_port_t serverPort = WebProcess::shared().compositingRenderServerPort();
    m_remoteLayerClient = WKCARemoteLayerClientMakeWithServerPort(serverPort);

    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), rootLayer()->platformLayer());

    layerTreeContext.contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
}

void LayerTreeHostCAMac::scheduleLayerFlush()
{
    m_layerFlushScheduler.schedule();
}

void LayerTreeHostCAMac::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
{
    if (layerFlushingEnabled)
        m_layerFlushScheduler.resume();
    else
        m_layerFlushScheduler.suspend();
}

void LayerTreeHostCAMac::invalidate()
{
    m_layerFlushScheduler.invalidate();

    WKCARemoteLayerClientInvalidate(m_remoteLayerClient.get());
    m_remoteLayerClient = nullptr;

    LayerTreeHostCA::invalidate();
}

void LayerTreeHostCAMac::sizeDidChange(const IntSize& newSize)
{
    LayerTreeHostCA::sizeDidChange(newSize);
    [CATransaction flush];
    [CATransaction synchronize];
}

void LayerTreeHostCAMac::forceRepaint()
{
    LayerTreeHostCA::forceRepaint();
    [CATransaction flush];
    [CATransaction synchronize];
}

void LayerTreeHostCAMac::pauseRendering()
{
    CALayer* root = rootLayer()->platformLayer();
    [root setValue:(id)kCFBooleanTrue forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidPauseNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:root forKey:@"layer"]];
}

void LayerTreeHostCAMac::resumeRendering()
{
    CALayer* root = rootLayer()->platformLayer();
    [root setValue:(id)kCFBooleanFalse forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidResumeNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:root forKey:@"layer"]];
}

bool LayerTreeHostCAMac::flushLayers()
{
    // This gets called outside of the normal event loop so wrap in an autorelease pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    performScheduledLayerFlush();
    [pool drain];

    return true;
}

void LayerTreeHostCAMac::didPerformScheduledLayerFlush()
{
    LayerTreeHostCA::didPerformScheduledLayerFlush();
}

} // namespace WebKit
