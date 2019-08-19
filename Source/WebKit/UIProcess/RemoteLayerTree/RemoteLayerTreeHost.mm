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

    for (const auto& createdLayer : transaction.createdLayers())
        createLayer(createdLayer);

    bool rootLayerChanged = false;
    auto* rootNode = nodeForID(transaction.rootLayerID());
    
    if (!rootNode)
        RELEASE_LOG_IF_ALLOWED("%p RemoteLayerTreeHost::updateLayerTree - failed to find root layer with ID %llu", this, transaction.rootLayerID());

    if (m_rootNode != rootNode) {
        m_rootNode = rootNode;
        rootLayerChanged = true;
    }

    struct LayerAndClone {
        GraphicsLayer::PlatformLayerID layerID;
        GraphicsLayer::PlatformLayerID cloneLayerID;
    };
    Vector<LayerAndClone> clonesToUpdate;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    // Can't use the iOS code on macOS yet: rdar://problem/31247730
    auto layerContentsType = RemoteLayerBackingStore::LayerContentsType::IOSurface;
#else
    auto layerContentsType = m_drawingArea->hasDebugIndicator() ? RemoteLayerBackingStore::LayerContentsType::IOSurface : RemoteLayerBackingStore::LayerContentsType::CAMachPort;
#endif
    
    for (auto& changedLayer : transaction.changedLayerProperties()) {
        auto layerID = changedLayer.key;
        const RemoteLayerTreeTransaction::LayerProperties& properties = *changedLayer.value;

        auto* node = nodeForID(layerID);
        ASSERT(node);

        if (!node) {
            // We have evidence that this can still happen, but don't know how (see r241899 for one already-fixed cause).
            RELEASE_LOG_IF_ALLOWED("%p RemoteLayerTreeHost::updateLayerTree - failed to find layer with ID %llu", this, layerID);
            continue;
        }

        if (properties.changedProperties.contains(RemoteLayerTreeTransaction::ClonedContentsChanged) && properties.clonedLayerID)
            clonesToUpdate.append({ layerID, properties.clonedLayerID });

        RemoteLayerTreePropertyApplier::applyProperties(*node, this, properties, m_nodes, layerContentsType);

        if (m_isDebugLayerTreeHost) {
            if (properties.changedProperties.contains(RemoteLayerTreeTransaction::BorderWidthChanged))
                node->layer().borderWidth = properties.borderWidth / indicatorScaleFactor;
            node->layer().masksToBounds = false;
        }
    }
    
    for (const auto& layerAndClone : clonesToUpdate)
        layerForID(layerAndClone.layerID).contents = layerForID(layerAndClone.cloneLayerID).contents;

    for (auto& destroyedLayer : transaction.destroyedLayers())
        layerWillBeRemoved(destroyedLayer);

    // Drop the contents of any layers which were unparented; the Web process will re-send
    // the backing store in the commit that reparents them.
    for (auto& newlyUnreachableLayerID : transaction.layerIDsWithNewlyUnreachableBackingStore())
        layerForID(newlyUnreachableLayerID).contents = nullptr;

    return rootLayerChanged;
}

RemoteLayerTreeNode* RemoteLayerTreeHost::nodeForID(GraphicsLayer::PlatformLayerID layerID) const
{
    if (!layerID)
        return nullptr;

    return m_nodes.get(layerID);
}

void RemoteLayerTreeHost::layerWillBeRemoved(WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    auto animationDelegateIter = m_animationDelegates.find(layerID);
    if (animationDelegateIter != m_animationDelegates.end()) {
        [animationDelegateIter->value invalidate];
        m_animationDelegates.remove(animationDelegateIter);
    }

    auto embeddedViewIter = m_layerToEmbeddedViewMap.find(layerID);
    if (embeddedViewIter != m_layerToEmbeddedViewMap.end()) {
        m_embeddedViews.remove(embeddedViewIter->value);
        m_layerToEmbeddedViewMap.remove(embeddedViewIter);
    }

    m_nodes.remove(layerID);
}

void RemoteLayerTreeHost::animationDidStart(WebCore::GraphicsLayer::PlatformLayerID layerID, CAAnimation *animation, MonotonicTime startTime)
{
    if (!m_drawingArea)
        return;

    CALayer *layer = layerForID(layerID);
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

    CALayer *layer = layerForID(layerID);
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
    for (auto& keyAndNode : m_nodes) {
        m_animationDelegates.remove(keyAndNode.key);
        keyAndNode.value->detachFromParent();
    }

    m_nodes.clear();
    m_embeddedViews.clear();
    m_layerToEmbeddedViewMap.clear();
    m_rootNode = nullptr;
}

CALayer *RemoteLayerTreeHost::layerWithIDForTesting(uint64_t layerID) const
{
    return layerForID(layerID);
}

CALayer *RemoteLayerTreeHost::layerForID(WebCore::GraphicsLayer::PlatformLayerID layerID) const
{
    auto* node = nodeForID(layerID);
    if (!node)
        return nil;
    return node->layer();
}

CALayer *RemoteLayerTreeHost::rootLayer() const
{
    if (!m_rootNode)
        return nil;
    return m_rootNode->layer();
}

void RemoteLayerTreeHost::createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    ASSERT(!m_nodes.contains(properties.layerID));

    auto node = makeNode(properties);

    m_nodes.add(properties.layerID, WTFMove(node));
}

#if !PLATFORM(IOS_FAMILY)
std::unique_ptr<RemoteLayerTreeNode> RemoteLayerTreeHost::makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    auto makeWithLayer = [&] (RetainPtr<CALayer> layer) {
        return makeUnique<RemoteLayerTreeNode>(properties.layerID, WTFMove(layer));
    };
    auto makeAdoptingLayer = [&] (CALayer* layer) {
        return makeWithLayer(adoptNS(layer));
    };

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
    case PlatformCALayer::LayerTypeScrollContainerLayer:
    case PlatformCALayer::LayerTypeEditableImageLayer:
        return RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);

    case PlatformCALayer::LayerTypeTransformLayer:
        return makeAdoptingLayer([[CATransformLayer alloc] init]);

    case PlatformCALayer::LayerTypeBackdropLayer:
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
#if ENABLE(FILTERS_LEVEL_2)
        return makeAdoptingLayer([[CABackdropLayer alloc] init]);
#else
        ASSERT_NOT_REACHED();
        return RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);
#endif
    case PlatformCALayer::LayerTypeCustom:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        if (m_isDebugLayerTreeHost)
            return RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);
        return makeWithLayer([CALayer _web_renderLayerWithContextID:properties.hostingContextID]);

    case PlatformCALayer::LayerTypeShapeLayer:
        return makeAdoptingLayer([[CAShapeLayer alloc] init]);
            
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}
#endif

void RemoteLayerTreeHost::detachRootLayer()
{
    if (!m_rootNode)
        return;
    m_rootNode->detachFromParent();
    m_rootNode = nullptr;
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
    recursivelyMapIOSurfaceBackingStore(rootLayer());
#endif
}

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
