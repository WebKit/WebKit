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
#import <QuartzCore/QuartzCore.h>
#import <WebCore/PlatformLayer.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebKitSystemInterface.h>

#if PLATFORM(IOS)
#import <UIKit/UIView.h>
#endif

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
    for (auto& delegate : m_animationDelegates.values())
        [delegate.get() invalidate];

    clearLayers();
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

    typedef std::pair<GraphicsLayer::PlatformLayerID, GraphicsLayer::PlatformLayerID> LayerIDPair;
    Vector<LayerIDPair> clonesToUpdate;
    
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

        if (properties.changedProperties & RemoteLayerTreeTransaction::ClonedContentsChanged && properties.clonedLayerID)
            clonesToUpdate.append(LayerIDPair(layerID, properties.clonedLayerID));

        if (m_isDebugLayerTreeHost) {
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers);

            if (properties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
                asLayer(layer).borderWidth = properties.borderWidth / indicatorScaleFactor;
            asLayer(layer).masksToBounds = false;
        } else
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers);
    }
    
    for (const auto& layerPair : clonesToUpdate) {
        LayerOrView *layer = getLayer(layerPair.first);
        LayerOrView *clonedLayer = getLayer(layerPair.second);
        asLayer(layer).contents = asLayer(clonedLayer).contents;
    }

    for (auto& destroyedLayer : transaction.destroyedLayers())
        layerWillBeRemoved(destroyedLayer);

    // Drop the contents of any layers which were unparented; the Web process will re-send
    // the backing store in the commit that reparents them.
    for (auto& newlyUnreachableLayerID : transaction.layerIDsWithNewlyUnreachableBackingStore())
        asLayer(getLayer(newlyUnreachableLayerID)).contents = nullptr;

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

void RemoteLayerTreeHost::animationDidStart(WebCore::GraphicsLayer::PlatformLayerID layerID, CAAnimation *animation, double startTime)
{
    CALayer *layer = asLayer(getLayer(layerID));
    if (!layer)
        return;

    String animationKey;
    for (NSString *key in [layer animationKeys]) {
        if ([layer animationForKey:key] == animation) {
            animationKey = key;
            break;
        }
    }

    if (!animationKey.isEmpty())
        m_drawingArea.acceleratedAnimationDidStart(layerID, animationKey, startTime);
}

void RemoteLayerTreeHost::clearLayers()
{
    for (auto& idLayer : m_layers) {
        m_animationDelegates.remove(idLayer.key);
#if PLATFORM(IOS)
        [idLayer.value.get() removeFromSuperview];
#else
        [asLayer(idLayer.value.get()) removeFromSuperlayer];
#endif
    }

    m_layers.clear();

    if (m_rootLayer)
        m_rootLayer = nullptr;
}

static NSString* const WKLayerIDPropertyKey = @"WKLayerID";

void RemoteLayerTreeHost::setLayerID(CALayer *layer, WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    [layer setValue:[NSNumber numberWithUnsignedLongLong:layerID] forKey:WKLayerIDPropertyKey];
}

WebCore::GraphicsLayer::PlatformLayerID RemoteLayerTreeHost::layerID(CALayer* layer)
{
    return [[layer valueForKey:WKLayerIDPropertyKey] unsignedLongLongValue];
}

#if !PLATFORM(IOS)
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
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeWebGLLayer:
        if (!m_isDebugLayerTreeHost)
            layer = WKMakeRenderLayer(properties.hostingContextID);
        else
            layer = adoptNS([[CALayer alloc] init]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    setLayerID(layer.get(), properties.layerID);

    return layer.get();
}
#endif

void RemoteLayerTreeHost::detachRootLayer()
{
#if PLATFORM(IOS)
    [m_rootLayer removeFromSuperview];
#else
    [asLayer(m_rootLayer) removeFromSuperlayer];
#endif
    m_rootLayer = nullptr;
}

} // namespace WebKit
