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

#import "config.h"
#import "PlatformCALayerRemoteCustom.h"

#import "LayerHostingContext.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "WebProcess.h"
#import <AVFoundation/AVFoundation.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/PlatformCALayerCocoa.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebLayer.h>
#import <wtf/RetainPtr.h>

#if ENABLE(MODEL_PROCESS)
#import <WebCore/ModelContext.h>
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebKit {
using namespace WebCore;

static NSString * const platformCALayerPointer = @"WKPlatformCALayer";

Ref<PlatformCALayerRemote> PlatformCALayerRemoteCustom::create(PlatformLayer *platformLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemoteCustom(PlatformCALayerCocoa::layerTypeForPlatformLayer(platformLayer), platformLayer, owner, context));
    context.layerDidEnterContext(layer.get(), layer->layerType());
    return WTFMove(layer);
}

#if ENABLE(MODEL_PROCESS)
Ref<PlatformCALayerRemote> PlatformCALayerRemoteCustom::create(Ref<WebCore::ModelContext> modelContext, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemoteCustom(WebCore::PlatformCALayer::LayerType::LayerTypeCustom, modelContext, owner, context));
    context.layerDidEnterContext(layer.get(), layer->layerType());
    return WTFMove(layer);
}
#endif

#if HAVE(AVKIT)
Ref<PlatformCALayerRemote> PlatformCALayerRemoteCustom::create(WebCore::HTMLVideoElement& videoElement, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemoteCustom(videoElement, owner, context));
    context.layerDidEnterContext(layer.get(), layer->layerType(), videoElement);
    return WTFMove(layer);
}
#endif

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(WebCore::PlatformCALayer::LayerType layerType, LayerHostingContextID hostedContextIdentifier, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(layerType, owner, context)
{
    m_layerHostingContext = LayerHostingContext::createTransportLayerForRemoteHosting(hostedContextIdentifier);
}

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(HTMLVideoElement& videoElement, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemoteCustom(PlatformCALayer::LayerType::LayerTypeAVPlayerLayer, videoElement.layerHostingContext().contextID, owner, context)
{
    m_hasVideo = true;
}

#if ENABLE(MODEL_PROCESS)
PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(WebCore::PlatformCALayer::LayerType layerType, Ref<WebCore::ModelContext> modelContext, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemoteCustom(layerType, modelContext->modelContentsLayerHostingContextIdentifier().toRawValue(), owner, context)
{
    m_modelContext = modelContext.ptr();
}
#endif

PlatformCALayerRemoteCustom::PlatformCALayerRemoteCustom(LayerType layerType, PlatformLayer * customLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(layerType, owner, context)
{
    m_layerHostingContext = LayerHostingContext::create({
#if PLATFORM(IOS_FAMILY)
        context.canShowWhileLocked()
#endif
    });

    m_layerHostingContext->setRootLayer(customLayer);
    [customLayer setValue:[NSValue valueWithPointer:this] forKey:platformCALayerPointer];

    m_platformLayer = customLayer;
    [customLayer web_disableAllActions];

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
    return m_layerHostingContext->cachedContextID();
}

void PlatformCALayerRemoteCustom::populateCreationProperties(RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeContext& context, PlatformCALayer::LayerType type)
{
    PlatformCALayerRemote::populateCreationProperties(properties, context, type);
    ASSERT(std::holds_alternative<RemoteLayerTreeTransaction::LayerCreationProperties::NoAdditionalData>(properties.additionalData));

#if ENABLE(MODEL_PROCESS)
    if (m_modelContext) {
        properties.additionalData = *m_modelContext;
        return;
    }
#endif

    properties.additionalData = RemoteLayerTreeTransaction::LayerCreationProperties::CustomData {
        .hostingContextID = hostingContextID(),
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        .sendRightAnnotated = sendRightAnnotated(),
#endif
        .hostingDeviceScaleFactor = context.deviceScaleFactor(),
        .preservesFlip = true
    };
}

Ref<WebCore::PlatformCALayer> PlatformCALayerRemoteCustom::clone(PlatformCALayerClient* owner) const
{
    RetainPtr<CALayer> clonedLayer;
    bool copyContents = true;

    if (layerType() == PlatformCALayer::LayerType::LayerTypeAVPlayerLayer) {
        
        if (PAL::isAVFoundationFrameworkAvailable() && [platformLayer() isKindOfClass:PAL::getAVPlayerLayerClassSingleton()]) {
            clonedLayer = adoptNS([PAL::allocAVPlayerLayerInstance() init]);

            RetainPtr destinationPlayerLayer = static_cast<AVPlayerLayer *>(clonedLayer.get());
            RetainPtr sourcePlayerLayer = static_cast<AVPlayerLayer *>(platformLayer());
            RunLoop::mainSingleton().dispatch([destinationPlayerLayer = WTFMove(destinationPlayerLayer), sourcePlayerLayer = WTFMove(sourcePlayerLayer)] {
                [destinationPlayerLayer setPlayer:[sourcePlayerLayer player]];
            });
        } else {
            // On iOS, the AVPlayerLayer is inside a WebVideoContainerLayer. This code needs to share logic with MediaPlayerPrivateAVFoundationObjC::createAVPlayerLayer().
            clonedLayer = adoptNS([[CALayer alloc] init]);
        }

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
    PlatformCALayerRemote::setNeedsDisplayInRect(rect);
}

void PlatformCALayerRemoteCustom::setNeedsDisplay()
{
    PlatformCALayerRemote::setNeedsDisplay();
}

} // namespace WebKit
