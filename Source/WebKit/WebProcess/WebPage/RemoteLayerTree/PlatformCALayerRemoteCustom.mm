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
#import <WebCore/PlatformCALayerCocoa.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVPlayerLayer)

namespace WebKit {
using namespace WebCore;

static NSString * const platformCALayerPointer = @"WKPlatformCALayer";

Ref<PlatformCALayerRemote> PlatformCALayerRemoteCustom::create(PlatformLayer *platformLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemoteCustom(PlatformCALayerCocoa::layerTypeForPlatformLayer(platformLayer), platformLayer, owner, context));
    context.layerDidEnterContext(layer.get(), layer->layerType());
    return WTFMove(layer);
}

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(LayerType layerType, PlatformLayer * customLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(layerType, owner, context)
{
    switch (context.layerHostingMode()) {
    case LayerHostingMode::InProcess:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::singleton().compositingRenderServerPort());
        break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    case LayerHostingMode::OutOfProcess:
        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
#if PLATFORM(IOS_FAMILY)
        if (layerType == LayerTypeAVPlayerLayer) {
            float scaleFactor = context.deviceScaleFactor();
            // Set a scale factor here to make convertRect:toLayer:nil take scale factor into account. <rdar://problem/18316542>.
            // This scale factor is inverted in the hosting process.
            [customLayer setTransform:CATransform3DMakeScale(scaleFactor, scaleFactor, 1)];
        }
#endif
        break;
#endif
    }

    m_layerHostingContext->setRootLayer(customLayer);
    [customLayer setValue:[NSValue valueWithPointer:this] forKey:platformCALayerPointer];

    m_platformLayer = customLayer;
    [customLayer web_disableAllActions];

    m_providesContents = layerType == LayerTypeContentsProvidedLayer;

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

Ref<WebCore::PlatformCALayer> PlatformCALayerRemoteCustom::clone(PlatformCALayerClient* owner) const
{
    RetainPtr<CALayer> clonedLayer;
    bool copyContents = true;

    if (layerType() == LayerTypeAVPlayerLayer) {
        
        if ([platformLayer() isKindOfClass:getAVPlayerLayerClass()]) {
            clonedLayer = adoptNS([allocAVPlayerLayerInstance() init]);

            AVPlayerLayer *destinationPlayerLayer = static_cast<AVPlayerLayer *>(clonedLayer.get());
            AVPlayerLayer *sourcePlayerLayer = static_cast<AVPlayerLayer *>(platformLayer());
            dispatch_async(dispatch_get_main_queue(), [destinationPlayerLayer, sourcePlayerLayer] {
                [destinationPlayerLayer setPlayer:[sourcePlayerLayer player]];
            });
        } else {
            // On iOS, the AVPlayerLayer is inside a WebVideoContainerLayer. This code needs to share logic with MediaPlayerPrivateAVFoundationObjC::createAVPlayerLayer().
            clonedLayer = adoptNS([[CALayer alloc] init]);
        }

        copyContents = false;
    } else if (layerType() == LayerTypeContentsProvidedLayer) {
        clonedLayer = adoptNS([[CALayer alloc] init]);
        // FIXME: currently copying WebGL contents breaks the original layer.
        copyContents = false;
    }

    auto clone = adoptRef(*new PlatformCALayerRemoteCustom(layerType(), clonedLayer.get(), owner, *context()));
    context()->layerDidEnterContext(clone.get(), clone->layerType());

    updateClonedLayerProperties(clone.get(), copyContents);

    clone->setClonedLayer(this);
    return WTFMove(clone);
}

CFTypeRef PlatformCALayerRemoteCustom::contents() const
{
    return (__bridge CFTypeRef)[m_platformLayer contents];
}

void PlatformCALayerRemoteCustom::setContents(CFTypeRef contents)
{
    [m_platformLayer setContents:(__bridge id)contents];
}

void PlatformCALayerRemoteCustom::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (m_providesContents)
        [m_platformLayer setNeedsDisplayInRect:rect];
    else
        PlatformCALayerRemote::setNeedsDisplayInRect(rect);
}

void PlatformCALayerRemoteCustom::setNeedsDisplay()
{
    if (m_providesContents)
        [m_platformLayer setNeedsDisplay];
    else
        PlatformCALayerRemote::setNeedsDisplay();
}

} // namespace WebKit
