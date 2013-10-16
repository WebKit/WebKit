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
#import "RemoteLayerTreeHost.h"

#import "RemoteLayerTreeHostMessages.h"
#import "RemoteLayerTreeTransaction.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/PlatformLayer.h>

#import <QuartzCore/QuartzCore.h>

using namespace WebCore;

static NSDictionary *nullActionsDictionary()
{
    NSNull* nullValue = [NSNull null];
    return @{
        @"anchorPoint" : nullValue,
        @"anchorPointZ" : nullValue,
        @"bounds" : nullValue,
        @"contents" : nullValue,
        @"contentsRect" : nullValue,
        @"opacity" : nullValue,
        @"position" : nullValue,
        @"shadowColor" : nullValue,
        @"sublayerTransform" : nullValue,
        @"sublayers" : nullValue,
        @"transform" : nullValue,
        @"zPosition" : nullValue };
}

namespace WebKit {

RemoteLayerTreeHost::RemoteLayerTreeHost(WebPageProxy* webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_rootLayer(nullptr)
{
    m_webPageProxy->process()->addMessageReceiver(Messages::RemoteLayerTreeHost::messageReceiverName(), m_webPageProxy->pageID(), this);
}

RemoteLayerTreeHost::~RemoteLayerTreeHost()
{
    m_webPageProxy->process()->removeMessageReceiver(Messages::RemoteLayerTreeHost::messageReceiverName(), m_webPageProxy->pageID());
}

void RemoteLayerTreeHost::commit(const RemoteLayerTreeTransaction& transaction)
{
    CALayer *rootLayer = getOrCreateLayer(transaction.rootLayerID());
    if (m_rootLayer != rootLayer) {
        m_rootLayer = rootLayer;
        m_webPageProxy->setAcceleratedCompositingRootLayer(m_rootLayer);
    }

#ifndef NDEBUG
    transaction.dump();
#endif

    for (auto changedLayer : transaction.changedLayers()) {
        RemoteLayerTreeTransaction::LayerID layerID = changedLayer.key;
        const auto& properties = changedLayer.value;

        CALayer *sublayer = getOrCreateLayer(layerID);

        if (properties.changedProperties & RemoteLayerTreeTransaction::NameChanged)
            [sublayer setName:properties.name];

        if (properties.changedProperties & RemoteLayerTreeTransaction::PositionChanged) {
            [sublayer setPosition:CGPointMake(properties.position.x(), properties.position.y())];
            [sublayer setZPosition:properties.position.z()];
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged) {
            [sublayer setAnchorPoint:CGPointMake(properties.anchorPoint.x(), properties.anchorPoint.y())];
            [sublayer setAnchorPointZ:properties.anchorPoint.z()];
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::SizeChanged)
            [sublayer setBounds:FloatRect(FloatPoint(), properties.size)];

        if (properties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged) {
            CGFloat components[4];
            properties.backgroundColor.getRGBA(components[0], components[1], components[2], components[3]);

            RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
            RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));

            [sublayer setBackgroundColor:color.get()];
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            RetainPtr<NSMutableArray> children = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.children.size()]);
            for (auto child : properties.children)
                [children addObject:getOrCreateLayer(child)];
            [sublayer setSublayers:children.get()];
        }
    }

    for (auto destroyedLayer : transaction.destroyedLayers())
        m_layers.remove(destroyedLayer);
}

CALayer *RemoteLayerTreeHost::getOrCreateLayer(RemoteLayerTreeTransaction::LayerID layerID)
{
    RetainPtr<CALayer>& layer = m_layers.add(layerID, nullptr).iterator->value;
    if (!layer) {
        layer = adoptNS([[CALayer alloc] init]);
        [layer setStyle:@{ @"actions" : nullActionsDictionary() }];
    }

    return layer.get();
}

} // namespace WebKit
