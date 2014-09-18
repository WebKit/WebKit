/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#import "PlatformCALayerRemoteCustom.h"

#import "LayerHostingContext.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "WebProcess.h"
#import <AVFoundation/AVFoundation.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/PlatformCALayerMac.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

using namespace WebCore;

namespace WebKit {

static NSString * const platformCALayerPointer = @"WKPlatformCALayer";

PassRefPtr<PlatformCALayerRemote> PlatformCALayerRemoteCustom::create(PlatformLayer *platformLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    RefPtr<PlatformCALayerRemote> layer = adoptRef(new PlatformCALayerRemoteCustom(PlatformCALayerMac::layerTypeForPlatformLayer(platformLayer), platformLayer, owner, context));
    context.layerWasCreated(*layer, layer->layerType());

    return layer.release();
}

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(LayerType layerType, PlatformLayer * customLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(layerType, owner, context)
{
    switch (context.layerHostingMode()) {
    case LayerHostingMode::InProcess:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::shared().compositingRenderServerPort());
        break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    case LayerHostingMode::OutOfProcess:
        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
#if PLATFORM(IOS)
        float scaleFactor = context.deviceScaleFactor();
        // Set a scale factor here to make convertRect:toLayer:nil take scale factor into account. <rdar://problem/18316542>.
        // This scale factor is inverted in the hosting process.
        [customLayer setTransform:CATransform3DMakeScale(scaleFactor, scaleFactor, 1)];
#endif
        break;
#endif
    }

    m_layerHostingContext->setRootLayer(customLayer);
    [customLayer setValue:[NSValue valueWithPointer:this] forKey:platformCALayerPointer];

    m_platformLayer = customLayer;
    [customLayer web_disableAllActions];

    m_providesContents = layerType == LayerTypeWebGLLayer;

    properties().position = FloatPoint3D(customLayer.position.x, customLayer.position.y, customLayer.zPosition);
    properties().anchorPoint = FloatPoint3D(customLayer.anchorPoint.x, customLayer.anchorPoint.y, customLayer.anchorPointZ);
    properties().bounds = customLayer.bounds;
    properties().contentsRect = customLayer.contentsRect;
}

PlatformCALayerRemoteCustom::~PlatformCALayerRemoteCustom()
{
    [m_platformLayer setValue:nil forKey:platformCALayerPointer];
}

uint32_t PlatformCALayerRemoteCustom::hostingContextID()
{
    return m_layerHostingContext->contextID();
}

PassRefPtr<WebCore::PlatformCALayer> PlatformCALayerRemoteCustom::clone(PlatformCALayerClient* owner) const
{
    RetainPtr<CALayer *> clonedLayer;
    bool copyContents = true;

    if (layerType() == LayerTypeAVPlayerLayer) {
        clonedLayer = adoptNS([[getAVPlayerLayerClass() alloc] init]);

        AVPlayerLayer* destinationPlayerLayer = static_cast<AVPlayerLayer *>(clonedLayer.get());
        AVPlayerLayer* sourcePlayerLayer = static_cast<AVPlayerLayer *>(platformLayer());
        dispatch_async(dispatch_get_main_queue(), ^{
            [destinationPlayerLayer setPlayer:[sourcePlayerLayer player]];
        });
        copyContents = false;
    } else if (layerType() == LayerTypeWebGLLayer) {
        clonedLayer = adoptNS([[CALayer alloc] init]);
        // FIXME: currently copying WebGL contents breaks the original layer.
        copyContents = false;
    }

    RefPtr<PlatformCALayerRemote> clone = adoptRef(new PlatformCALayerRemoteCustom(layerType(), clonedLayer.get(), owner, *context()));
    context()->layerWasCreated(*clone, clone->layerType());

    updateClonedLayerProperties(*clone, copyContents);

    clone->setClonedLayer(this);
    return clone.release();
}

CFTypeRef PlatformCALayerRemoteCustom::contents() const
{
    return [m_platformLayer contents];
}

void PlatformCALayerRemoteCustom::setContents(CFTypeRef contents)
{
    [m_platformLayer setContents:(id)contents];
}

void PlatformCALayerRemoteCustom::setNeedsDisplay(const FloatRect* rect)
{
    if (m_providesContents) {
        if (rect)
            [m_platformLayer setNeedsDisplayInRect:*rect];
        else
            [m_platformLayer setNeedsDisplay];
    } else
        PlatformCALayerRemote::setNeedsDisplay(rect);
}

} // namespace WebKit
