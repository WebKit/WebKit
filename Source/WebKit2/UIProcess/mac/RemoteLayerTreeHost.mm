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

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "RemoteLayerTreeTransaction.h"
#import "ShareableBitmap.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/PlatformLayer.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebKitSystemInterface.h>

#import <QuartzCore/QuartzCore.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeHost::RemoteLayerTreeHost(RemoteLayerTreeDrawingAreaProxy& drawingArea)
    : m_drawingArea(drawingArea)
    , m_rootLayer(nullptr)
    , m_isDebugLayerTreeHost(false)
{
}

RemoteLayerTreeHost::~RemoteLayerTreeHost()
{
}

bool RemoteLayerTreeHost::updateLayerTree(const RemoteLayerTreeTransaction& transaction, float indicatorScaleFactor)
{
    for (const auto& createdLayer : transaction.createdLayers()) {
        const RemoteLayerTreeTransaction::LayerProperties* properties = transaction.changedLayerProperties().get(createdLayer.layerID);
        createLayer(createdLayer, properties);
    }

    bool rootLayerChanged = false;
    LayerOrView *rootLayer = getLayer(transaction.rootLayerID());
    if (m_rootLayer != rootLayer) {
        m_rootLayer = rootLayer;
        rootLayerChanged = true;
    }

    for (auto& changedLayer : transaction.changedLayerProperties()) {
        auto layerID = changedLayer.key;
        const RemoteLayerTreeTransaction::LayerProperties& properties = *changedLayer.value;

        LayerOrView *layer = getLayer(layerID);
        ASSERT(layer);

        RemoteLayerTreePropertyApplier::RelatedLayerMap relatedLayers;
        if (properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            for (auto& child : properties.children)
                relatedLayers.set(child, getLayer(child));
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged && properties.maskLayerID)
            relatedLayers.set(properties.maskLayerID, getLayer(properties.maskLayerID));

        if (m_isDebugLayerTreeHost) {
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers);

            if (properties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
                asLayer(layer).borderWidth = properties.borderWidth / indicatorScaleFactor;
            asLayer(layer).masksToBounds = false;
          } else
        else
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers);
    }

    for (auto& destroyedLayer : transaction.destroyedLayers())
        layerWillBeRemoved(destroyedLayer);

    return rootLayerChanged;
}

LayerOrView *RemoteLayerTreeHost::getLayer(GraphicsLayer::PlatformLayerID layerID) const
{
    if (!layerID)
        return nil;

    return m_layers.get(layerID).get();
}

void RemoteLayerTreeHost::layerWillBeRemoved(WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    m_animationDelegates.remove(layerID);
    m_layers.remove(layerID);
}

void RemoteLayerTreeHost::animationDidStart(WebCore::GraphicsLayer::PlatformLayerID layerID, double startTime)
{
    m_drawingArea.acceleratedAnimationDidStart(layerID, startTime);
}

#if !PLATFORM(IOS)
static NSString* const WKLayerIDPropertyKey = @"WKLayerID";

WebCore::GraphicsLayer::PlatformLayerID RemoteLayerTreeHost::layerID(LayerOrView* layer)
{
    return [[layer valueForKey:WKLayerIDPropertyKey] unsignedLongLongValue];
}

LayerOrView *RemoteLayerTreeHost::createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeTransaction::LayerProperties*)
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
        if (!m_isDebugLayerTreeHost)
            layer = WKMakeRenderLayer(properties.hostingContextID);
        else
            layer = adoptNS([[CALayer alloc] init]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer web_disableAllActions];
    [layer setValue:[NSNumber numberWithUnsignedLongLong:properties.layerID] forKey:WKLayerIDPropertyKey];

    return layer.get();
}
#endif

} // namespace WebKit
