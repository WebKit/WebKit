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
#import "ShareableBitmap.h"
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

static RetainPtr<CGColorRef> cgColorFromColor(Color color)
{
    CGFloat components[4];
    color.getRGBA(components[0], components[1], components[2], components[3]);

    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGColorRef> cgColor = adoptCF(CGColorCreate(colorSpace.get(), components));
    return cgColor;
}

static NSString *toCAFilterType(PlatformCALayer::FilterType type)
{
    switch (type) {
    case PlatformCALayer::Linear:
        return kCAFilterLinear;
    case PlatformCALayer::Nearest:
        return kCAFilterNearest;
    case PlatformCALayer::Trilinear:
        return kCAFilterTrilinear;
    };

    ASSERT_NOT_REACHED();
    return 0;
}


void RemoteLayerTreeHost::commit(const RemoteLayerTreeTransaction& transaction)
{
#ifndef NDEBUG
    transaction.dump();
#endif

    for (auto createdLayer : transaction.createdLayers())
        createLayer(createdLayer);

    CALayer *rootLayer = getLayer(transaction.rootLayerID());
    if (m_rootLayer != rootLayer) {
        m_rootLayer = rootLayer;
        m_webPageProxy->setAcceleratedCompositingRootLayer(m_rootLayer);
    }

    for (auto changedLayer : transaction.changedLayers()) {
        RemoteLayerTreeTransaction::LayerID layerID = changedLayer.key;
        const auto& properties = changedLayer.value;

        CALayer *layer = getLayer(layerID);
        ASSERT(layer);

        if (properties.changedProperties & RemoteLayerTreeTransaction::NameChanged)
            layer.name = properties.name;

        if (properties.changedProperties & RemoteLayerTreeTransaction::PositionChanged) {
            layer.position = CGPointMake(properties.position.x(), properties.position.y());
            layer.zPosition = properties.position.z();
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged) {
            layer.anchorPoint = CGPointMake(properties.anchorPoint.x(), properties.anchorPoint.y());
            layer.anchorPointZ = properties.anchorPoint.z();
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::SizeChanged)
            layer.bounds = FloatRect(FloatPoint(), properties.size);

        if (properties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged)
            layer.backgroundColor = cgColorFromColor(properties.backgroundColor).get();

        if (properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            RetainPtr<NSMutableArray> children = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.children.size()]);
            for (auto child : properties.children)
                [children addObject:getLayer(child)];
            layer.sublayers = children.get();
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::BorderColorChanged)
            layer.borderColor = cgColorFromColor(properties.borderColor).get();

        if (properties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
            layer.borderWidth = properties.borderWidth;

        if (properties.changedProperties & RemoteLayerTreeTransaction::OpacityChanged)
            layer.opacity = properties.opacity;

        if (properties.changedProperties & RemoteLayerTreeTransaction::TransformChanged)
            layer.transform = properties.transform;

        if (properties.changedProperties & RemoteLayerTreeTransaction::SublayerTransformChanged)
            layer.sublayerTransform = properties.sublayerTransform;

        if (properties.changedProperties & RemoteLayerTreeTransaction::HiddenChanged)
            layer.hidden = properties.hidden;

        if (properties.changedProperties & RemoteLayerTreeTransaction::GeometryFlippedChanged)
            layer.geometryFlipped = properties.geometryFlipped;

        if (properties.changedProperties & RemoteLayerTreeTransaction::DoubleSidedChanged)
            layer.doubleSided = properties.doubleSided;

        if (properties.changedProperties & RemoteLayerTreeTransaction::MasksToBoundsChanged)
            layer.masksToBounds = properties.masksToBounds;

        if (properties.changedProperties & RemoteLayerTreeTransaction::OpaqueChanged)
            layer.opaque = properties.opaque;

        if (properties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged) {
            CALayer *maskLayer = getLayer(properties.maskLayer);
            ASSERT(!maskLayer.superlayer);
            if (!maskLayer.superlayer)
                layer.mask = maskLayer;
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::ContentsRectChanged)
            layer.contentsRect = properties.contentsRect;

        if (properties.changedProperties & RemoteLayerTreeTransaction::ContentsScaleChanged)
            layer.contentsScale = properties.contentsScale;

        if (properties.changedProperties & RemoteLayerTreeTransaction::MinificationFilterChanged)
            layer.minificationFilter = toCAFilterType(properties.minificationFilter);

        if (properties.changedProperties & RemoteLayerTreeTransaction::MagnificationFilterChanged)
            layer.magnificationFilter = toCAFilterType(properties.magnificationFilter);

        if (properties.changedProperties & RemoteLayerTreeTransaction::SpeedChanged)
            layer.speed = properties.speed;

        if (properties.changedProperties & RemoteLayerTreeTransaction::TimeOffsetChanged)
            layer.timeOffset = properties.timeOffset;

        if (properties.changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged) {
            // FIXME: Do we need Copy?
            RetainPtr<CGImageRef> image = properties.backingStore.bitmap()->makeCGImageCopy();
            layer.contents = (id)image.get();
        }
    }

    for (auto destroyedLayer : transaction.destroyedLayers())
        m_layers.remove(destroyedLayer);
}

CALayer *RemoteLayerTreeHost::getLayer(RemoteLayerTreeTransaction::LayerID layerID)
{
    return m_layers.get(layerID).get();
}


CALayer *RemoteLayerTreeHost::createLayer(RemoteLayerTreeTransaction::LayerCreationProperties properties)
{
    RetainPtr<CALayer>& layer = m_layers.add(properties.layerID, nullptr).iterator->value;

    ASSERT(!layer);

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
        layer = adoptNS([[CALayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        layer = adoptNS([[CATransformLayer alloc] init]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer setStyle:@{ @"actions" : nullActionsDictionary() }];

    return layer.get();
}

} // namespace WebKit
