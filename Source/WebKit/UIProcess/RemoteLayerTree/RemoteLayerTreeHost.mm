/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "RemoteLayerTreeTransaction.h"
#import "ShareableBitmap.h"
#import "WKAnimationDelegate.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformLayer.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIView.h>
#endif

namespace WebKit {
using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(...) RELEASE_LOG_IF(m_drawingArea && m_drawingArea->isAlwaysOnLoggingAllowed(), ViewState, __VA_ARGS__)

RemoteLayerTreeHost::RemoteLayerTreeHost(RemoteLayerTreeDrawingAreaProxy& drawingArea)
    : m_drawingArea(&drawingArea)
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
    if (!m_drawingArea)
        return false;

    for (const auto& createdLayer : transaction.createdLayers()) {
        const RemoteLayerTreeTransaction::LayerProperties* properties = transaction.changedLayerProperties().get(createdLayer.layerID);
        createLayer(createdLayer, properties);
    }

    bool rootLayerChanged = false;
    LayerOrView *rootLayer = getLayer(transaction.rootLayerID());
    
    if (!rootLayer)
        RELEASE_LOG_IF_ALLOWED("%p RemoteLayerTreeHost::updateLayerTree - failed to find root layer with ID %llu", this, transaction.rootLayerID());

    if (m_rootLayer != rootLayer) {
        m_rootLayer = rootLayer;
        rootLayerChanged = true;
    }

    typedef std::pair<GraphicsLayer::PlatformLayerID, GraphicsLayer::PlatformLayerID> LayerIDPair;
    Vector<LayerIDPair> clonesToUpdate;

#if PLATFORM(MAC) || PLATFORM(IOSMAC)
    // Can't use the iOS code on macOS yet: rdar://problem/31247730
    auto layerContentsType = RemoteLayerBackingStore::LayerContentsType::IOSurface;
#else
    auto layerContentsType = m_drawingArea->hasDebugIndicator() ? RemoteLayerBackingStore::LayerContentsType::IOSurface : RemoteLayerBackingStore::LayerContentsType::CAMachPort;
#endif
    
    for (auto& changedLayer : transaction.changedLayerProperties()) {
        auto layerID = changedLayer.key;
        const RemoteLayerTreeTransaction::LayerProperties& properties = *changedLayer.value;

        LayerOrView *layer = getLayer(layerID);
        ASSERT(layer);

        RemoteLayerTreePropertyApplier::RelatedLayerMap relatedLayers;
        if (properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            for (auto& child : properties.children)
                relatedLayers.set(child, (__bridge CFTypeRef)getLayer(child));
        }

        if (properties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged && properties.maskLayerID)
            relatedLayers.set(properties.maskLayerID, (__bridge CFTypeRef)getLayer(properties.maskLayerID));

        if (properties.changedProperties & RemoteLayerTreeTransaction::ClonedContentsChanged && properties.clonedLayerID)
            clonesToUpdate.append(LayerIDPair(layerID, properties.clonedLayerID));

        if (m_isDebugLayerTreeHost) {
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers, layerContentsType);

            if (properties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
                asLayer(layer).borderWidth = properties.borderWidth / indicatorScaleFactor;
            asLayer(layer).masksToBounds = false;
        } else
            RemoteLayerTreePropertyApplier::applyProperties(layer, this, properties, relatedLayers, layerContentsType);
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
    auto iter = m_animationDelegates.find(layerID);
    if (iter != m_animationDelegates.end()) {
        [iter->value invalidate];
        m_animationDelegates.remove(iter);
    }
    m_layers.remove(layerID);
}

void RemoteLayerTreeHost::animationDidStart(WebCore::GraphicsLayer::PlatformLayerID layerID, CAAnimation *animation, MonotonicTime startTime)
{
    if (!m_drawingArea)
        return;

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
        m_drawingArea->acceleratedAnimationDidStart(layerID, animationKey, startTime);
}

void RemoteLayerTreeHost::animationDidEnd(WebCore::GraphicsLayer::PlatformLayerID layerID, CAAnimation *animation)
{
    if (!m_drawingArea)
        return;

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
        m_drawingArea->acceleratedAnimationDidEnd(layerID, animationKey);
}

void RemoteLayerTreeHost::detachFromDrawingArea()
{
    m_drawingArea = nullptr;
}

void RemoteLayerTreeHost::clearLayers()
{
    for (auto& idLayer : m_layers) {
        m_animationDelegates.remove(idLayer.key);
#if PLATFORM(IOS_FAMILY)
        [idLayer.value.get() removeFromSuperview];
#else
        [asLayer(idLayer.value.get()) removeFromSuperlayer];
#endif
    }

    m_layers.clear();
    m_rootLayer = nullptr;
}

LayerOrView* RemoteLayerTreeHost::layerWithIDForTesting(uint64_t layerID) const
{
    return getLayer(layerID);
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

#if !PLATFORM(IOS_FAMILY)
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
    case PlatformCALayer::LayerTypeScrollingLayer:
        layer = adoptNS([[CALayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        layer = adoptNS([[CATransformLayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeBackdropLayer:
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
#if ENABLE(FILTERS_LEVEL_2)
        layer = adoptNS([[CABackdropLayer alloc] init]);
#else
        ASSERT_NOT_REACHED();
        layer = adoptNS([[CALayer alloc] init]);
#endif
        break;
    case PlatformCALayer::LayerTypeCustom:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        if (!m_isDebugLayerTreeHost)
            layer = [CALayer _web_renderLayerWithContextID:properties.hostingContextID];
        else
            layer = adoptNS([[CALayer alloc] init]);
        break;
    case PlatformCALayer::LayerTypeShapeLayer:
        layer = adoptNS([[CAShapeLayer alloc] init]);
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
#if PLATFORM(IOS_FAMILY)
    [m_rootLayer removeFromSuperview];
#else
    [asLayer(m_rootLayer) removeFromSuperlayer];
#endif
    m_rootLayer = nullptr;
}

#if HAVE(IOSURFACE)
static void recursivelyMapIOSurfaceBackingStore(CALayer *layer)
{
    if (layer.contents && CFGetTypeID((__bridge CFTypeRef)layer.contents) == CAMachPortGetTypeID()) {
        MachSendRight port = MachSendRight::create(CAMachPortGetPort((__bridge CAMachPortRef)layer.contents));
        auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(port), sRGBColorSpaceRef());
        layer.contents = surface ? surface->asLayerContents() : nil;
    }

    for (CALayer *sublayer in layer.sublayers)
        recursivelyMapIOSurfaceBackingStore(sublayer);
}
#endif

void RemoteLayerTreeHost::mapAllIOSurfaceBackingStore()
{
#if HAVE(IOSURFACE)
    recursivelyMapIOSurfaceBackingStore(asLayer(m_rootLayer));
#endif
}

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
