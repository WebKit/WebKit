/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#import "LayerHostingContext.h"

#import "LayerTreeContext.h"
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>

namespace WebKit {

std::unique_ptr<LayerHostingContext> LayerHostingContext::createForPort(const MachSendRight& serverPort)
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();

    NSDictionary *options = @{
        kCAContextPortNumber : @(serverPort.sendRight()),
#if PLATFORM(MAC)
        kCAContextCIFilterBehavior : @"ignore",
#endif
    };

    layerHostingContext->m_layerHostingMode = LayerHostingMode::InProcess;
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:options];
    layerHostingContext->m_cachedContextID = layerHostingContext->contextID();
    return layerHostingContext;
}

#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
std::unique_ptr<LayerHostingContext> LayerHostingContext::createForExternalHostingProcess(const LayerHostingContextOptions& options)
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();
    layerHostingContext->m_layerHostingMode = LayerHostingMode::OutOfProcess;

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    // Use a very large display ID to ensure that the context is never put on-screen
    // without being explicitly parented. See <rdar://problem/16089267> for details.
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:@{
        kCAContextSecure: @(options.canShowWhileLocked),
#if HAVE(CORE_ANIMATION_RENDER_SERVER)
        kCAContextIgnoresHitTest : @YES,
        kCAContextDisplayId : @10000
#endif
    }];
#elif !PLATFORM(MACCATALYST)
    [CAContext setAllowsCGSConnections:NO];
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:@{
        kCAContextCIFilterBehavior :  @"ignore",
    }];
#else
    layerHostingContext->m_context = [CAContext contextWithCGSConnection:CGSMainConnectionID() options:@{
        kCAContextCIFilterBehavior : @"ignore",
    }];
#endif
    layerHostingContext->m_cachedContextID = layerHostingContext->contextID();
    return layerHostingContext;
}

#if PLATFORM(MAC)
std::unique_ptr<LayerHostingContext> LayerHostingContext::createForExternalPluginHostingProcess()
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();
    layerHostingContext->m_layerHostingMode = LayerHostingMode::OutOfProcess;
    layerHostingContext->m_context = [CAContext contextWithCGSConnection:CGSMainConnectionID() options:@{ kCAContextCIFilterBehavior : @"ignore" }];
    return layerHostingContext;
}
#endif

std::unique_ptr<LayerHostingContext> LayerHostingContext::createTransportLayerForRemoteHosting(LayerHostingContextID contextID)
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();
    layerHostingContext->m_layerHostingMode = LayerHostingMode::OutOfProcess;
    layerHostingContext->m_cachedContextID = contextID;
    return layerHostingContext;
}

RetainPtr<CALayer> LayerHostingContext::createPlatformLayerForHostingContext(LayerHostingContextID contextID)
{
    return [CALayer _web_renderLayerWithContextID:contextID shouldPreserveFlip:NO];
}

#endif // HAVE(OUT_OF_PROCESS_LAYER_HOSTING)

LayerHostingContext::LayerHostingContext()
{
}

LayerHostingContext::~LayerHostingContext()
{
}

void LayerHostingContext::setRootLayer(CALayer *rootLayer)
{
    [m_context setLayer:rootLayer];
}

CALayer *LayerHostingContext::rootLayer() const
{
    return [m_context layer];
}

LayerHostingContextID LayerHostingContext::contextID() const
{
    return [m_context contextId];
}

void LayerHostingContext::invalidate()
{
    [m_context invalidate];
}

void LayerHostingContext::setColorSpace(CGColorSpaceRef colorSpace)
{
    [m_context setColorSpace:colorSpace];
}

CGColorSpaceRef LayerHostingContext::colorSpace() const
{
    return [m_context colorSpace];
}

#if PLATFORM(MAC)
void LayerHostingContext::setColorMatchUntaggedContent(bool colorMatchUntaggedContent)
{
    [m_context setColorMatchUntaggedContent:colorMatchUntaggedContent];
}

bool LayerHostingContext::colorMatchUntaggedContent() const
{
    return [m_context colorMatchUntaggedContent];
}
#endif

void LayerHostingContext::setFencePort(mach_port_t fencePort)
{
    [m_context setFencePort:fencePort];
}

MachSendRight LayerHostingContext::createFencePort()
{
    return MachSendRight::adopt([m_context createFencePort]);
}

void LayerHostingContext::updateCachedContextID(LayerHostingContextID contextID)
{
    m_cachedContextID = contextID;
}

LayerHostingContextID LayerHostingContext::cachedContextID()
{
    return m_cachedContextID;
}

void LayerHostingContext::beginTransaction()
{
    [CATransaction begin];
}

void LayerHostingContext::commitTransaction()
{
    [CATransaction flush];
    [CATransaction commit];
}

uint32_t LayerHostingContext::assignToSlotAndTransfer(RetainPtr<id>&& obj, LayerHostingContextID destContext)
{
    if (!m_slots.size() && [m_context respondsToSelector:@selector(createSlotArray:slots:)]) {
        m_slots.grow(100);
        [m_context createSlotArray:100 slots:m_slots.data()];
    }

    // Assign the object to the slot (which increments the use-count for
    // IOSurfaces, and transfer ownership of the slot to the context that
    // will be used for rendering. The slot should get deleted once it's been
    // used in the rendering tree, removing the use-count.
    uint32_t slot;
    if (m_slots.size())
        slot = m_slots.takeLast();
    else
        slot = [m_context createSlot];
    [m_context setObject:obj.get() forSlot:slot];

    [m_context transferSlot:slot toContextWithId:destContext];

    return slot;
}

} // namespace WebKit
