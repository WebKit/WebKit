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

#import "Logging.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "RemoteLayerTreeTransaction.h"
#import "ShareableBitmap.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/PlatformLayer.h>
#import <WebKitSystemInterface.h>

#import <QuartzCore/QuartzCore.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeHost::RemoteLayerTreeHost(WebPageProxy* webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_rootLayer(nullptr)
{
}

RemoteLayerTreeHost::~RemoteLayerTreeHost()
{
}

void RemoteLayerTreeHost::updateLayerTree(const RemoteLayerTreeTransaction& transaction)
{
    LOG(RemoteLayerTree, "%s", transaction.description().data());

    for (auto createdLayer : transaction.createdLayers())
        createLayer(createdLayer);

    CALayer *rootLayer = getLayer(transaction.rootLayerID());
    if (m_rootLayer != rootLayer) {
        m_rootLayer = rootLayer;
        m_webPageProxy->setAcceleratedCompositingRootLayer(m_rootLayer);
    }

    for (auto changedLayer : transaction.changedLayers()) {
        auto layerID = changedLayer.key;
        const auto& properties = changedLayer.value;

        CALayer *layer = getLayer(layerID);
        ASSERT(layer);

        RemoteLayerTreePropertyApplier::RelatedLayerMap relatedLayers;
        if (properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            for (auto child : properties.children)
                relatedLayers.set(child, getLayer(child));
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged && properties.maskLayerID)
            relatedLayers.set(properties.maskLayerID, getLayer(properties.maskLayerID));

        RemoteLayerTreePropertyApplier::applyPropertiesToLayer(layer, properties, relatedLayers);
    }

    for (auto destroyedLayer : transaction.destroyedLayers())
        m_layers.remove(destroyedLayer);
}

CALayer *RemoteLayerTreeHost::getLayer(GraphicsLayer::PlatformLayerID layerID) const
{
    if (!layerID)
        return nil;

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
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        layer = adoptNS([[CALayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        layer = adoptNS([[CATransformLayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeCustom:
        layer = WKMakeRenderLayer(properties.hostingContextID);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer web_disableAllActions];

    return layer.get();
}

} // namespace WebKit
