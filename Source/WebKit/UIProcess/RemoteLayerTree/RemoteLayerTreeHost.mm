/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#import "LayerProperties.h"
#import "Logging.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreePropertyApplier.h"
#import "RemoteLayerTreeTransaction.h"
#import "ShareableBitmap.h"
#import "VideoPresentationManagerProxy.h"
#import "WKAnimationDelegate.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WindowKind.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformLayer.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/cocoa/QuartzCoreSoftLink.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIView.h>
#endif

namespace WebKit {
using namespace WebCore;

#define REMOTE_LAYER_TREE_HOST_RELEASE_LOG(...) RELEASE_LOG(ViewState, __VA_ARGS__)

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

RemoteLayerBackingStoreProperties::LayerContentsType RemoteLayerTreeHost::layerContentsType() const
{
    // If a surface will be referenced by multiple layers (as in the tile debug indicator), CAMachPort cannot be used.
    if (m_drawingArea->hasDebugIndicator())
        return RemoteLayerBackingStoreProperties::LayerContentsType::IOSurface;

    // If e.g. SceneKit will be doing an in-process snapshot of the layer tree, CAMachPort cannot be used: rdar://problem/47481972
    if (m_drawingArea->page().windowKind() == WindowKind::InProcessSnapshotting)
        return RemoteLayerBackingStoreProperties::LayerContentsType::IOSurface;

    if (PAL::canLoad_QuartzCore_CAIOSurfaceCreate())
        return RemoteLayerBackingStoreProperties::LayerContentsType::CachedIOSurface;
#if HAVE(MACH_PORT_CALAYER_CONTENTS)
    return RemoteLayerBackingStoreProperties::LayerContentsType::CAMachPort;
#else
    return RemoteLayerBackingStoreProperties::LayerContentsType::IOSurface;
#endif
}

bool RemoteLayerTreeHost::replayDynamicContentScalingDisplayListsIntoBackingStore() const
{
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    return m_drawingArea->page().preferences().replayCGDisplayListsIntoBackingStore();
#else
    return false;
#endif
}

bool RemoteLayerTreeHost::css3DTransformInteroperabilityEnabled() const
{
    return m_drawingArea->page().preferences().css3DTransformInteroperabilityEnabled();
}

bool RemoteLayerTreeHost::threadedAnimationResolutionEnabled() const
{
    return m_drawingArea->page().preferences().threadedAnimationResolutionEnabled();
}

#if PLATFORM(MAC)
bool RemoteLayerTreeHost::updateBannerLayers(const RemoteLayerTreeTransaction& transaction)
{
    auto scrolledContentsLayer = layerForID(transaction.scrolledContentsLayerID());
    if (!scrolledContentsLayer)
        return false;

    auto updateBannerLayer = [](CALayer *bannerLayer, CALayer *scrolledContentsLayer) -> bool {
        if (!bannerLayer)
            return false;

        if ([bannerLayer superlayer] == scrolledContentsLayer)
            return false;

        [scrolledContentsLayer addSublayer:bannerLayer];
        return true;
    };

    bool headerBannerLayerChanged = updateBannerLayer(m_drawingArea->page().headerBannerLayer(), scrolledContentsLayer);
    bool footerBannerLayerChanged = updateBannerLayer(m_drawingArea->page().footerBannerLayer(), scrolledContentsLayer);
    return headerBannerLayerChanged || footerBannerLayerChanged;
}
#endif

bool RemoteLayerTreeHost::updateLayerTree(const RemoteLayerTreeTransaction& transaction, float indicatorScaleFactor)
{
    if (!m_drawingArea)
        return false;

    for (const auto& createdLayer : transaction.createdLayers())
        createLayer(createdLayer);

    bool rootLayerChanged = false;
    auto* rootNode = nodeForID(transaction.rootLayerID());
    
    if (!rootNode)
        REMOTE_LAYER_TREE_HOST_RELEASE_LOG("%p RemoteLayerTreeHost::updateLayerTree - failed to find root layer with ID %llu", this, transaction.rootLayerID().object().toUInt64());

    if (m_rootNode != rootNode && transaction.isMainFrameProcessTransaction()) {
        m_rootNode = rootNode;
        rootLayerChanged = true;
    }

    struct LayerAndClone {
        PlatformLayerIdentifier layerID;
        PlatformLayerIdentifier cloneLayerID;
    };
    Vector<LayerAndClone> clonesToUpdate;

    auto layerContentsType = this->layerContentsType();
    for (auto& [layerID, propertiesPointer] : transaction.changedLayerProperties()) {
        const auto& properties = *propertiesPointer;

        auto* node = nodeForID(layerID);
        ASSERT(node);

        if (!node) {
            // We have evidence that this can still happen, but don't know how (see r241899 for one already-fixed cause).
            REMOTE_LAYER_TREE_HOST_RELEASE_LOG("%p RemoteLayerTreeHost::updateLayerTree - failed to find layer with ID %llu", this, layerID.object().toUInt64());
            continue;
        }

        RemoteLayerTreePropertyApplier::applyHierarchyUpdates(*node, properties, m_nodes);
    }

    if (auto contextHostedID = transaction.remoteContextHostedIdentifier()) {
        m_hostedLayers.set(*contextHostedID, rootNode->layerID());
        rootNode->setRemoteContextHostedIdentifier(*contextHostedID);
        if (auto* remoteRootNode = nodeForID(m_hostingLayers.get(*contextHostedID)))
            [remoteRootNode->layer() addSublayer:rootNode->layer()];
    }

    for (auto& changedLayer : transaction.changedLayerProperties()) {
        auto layerID = changedLayer.key;
        const auto& properties = *changedLayer.value;

        auto* node = nodeForID(layerID);
        ASSERT(node);

        if (!node) {
            // We have evidence that this can still happen, but don't know how (see r241899 for one already-fixed cause).
            REMOTE_LAYER_TREE_HOST_RELEASE_LOG("%p RemoteLayerTreeHost::updateLayerTree - failed to find layer with ID %llu", this, layerID.object().toUInt64());
            continue;
        }

        if (properties.changedProperties.contains(LayerChange::ClonedContentsChanged) && properties.clonedLayerID)
            clonesToUpdate.append({ layerID, *properties.clonedLayerID });

        RemoteLayerTreePropertyApplier::applyProperties(*node, this, properties, m_nodes, layerContentsType);

        if (m_isDebugLayerTreeHost) {
            if (properties.changedProperties.contains(LayerChange::BorderWidthChanged))
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

#if PLATFORM(MAC)
    if (updateBannerLayers(transaction))
        rootLayerChanged = true;
#endif

    return rootLayerChanged;
}

void RemoteLayerTreeHost::asyncSetLayerContents(PlatformLayerIdentifier layerID, ImageBufferBackendHandle&& handle, const WebCore::RenderingResourceIdentifier& identifier)
{
    auto* node = nodeForID(layerID);
    if (!node)
        return;

    RetainPtr<id> contents = RemoteLayerBackingStoreProperties::layerContentsBufferFromBackendHandle(WTFMove(handle), layerContentsType());
    node->layer().contents = contents.get();
    node->setAsyncContentsIdentifier(identifier);
}

RemoteLayerTreeNode* RemoteLayerTreeHost::nodeForID(PlatformLayerIdentifier layerID) const
{
    if (!layerID)
        return nullptr;

    return m_nodes.get(layerID);
}

void RemoteLayerTreeHost::layerWillBeRemoved(WebCore::PlatformLayerIdentifier layerID)
{
    auto animationDelegateIter = m_animationDelegates.find(layerID);
    if (animationDelegateIter != m_animationDelegates.end()) {
        [animationDelegateIter->value invalidate];
        m_animationDelegates.remove(animationDelegateIter);
    }

    if (auto node = m_nodes.take(layerID)) {
        if (auto hostingIdentifier = node->remoteContextHostingIdentifier())
            m_hostingLayers.remove(*hostingIdentifier);
        if (auto hostedIdentifier = node->remoteContextHostedIdentifier())
            m_hostedLayers.remove(*hostedIdentifier);
    }

#if HAVE(AVKIT)
    auto videoLayerIter = m_videoLayers.find(layerID);
    if (videoLayerIter != m_videoLayers.end()) {
        if (auto videoManager = m_drawingArea->page().videoPresentationManager())
            videoManager->willRemoveLayerForID(videoLayerIter->value);
        m_videoLayers.remove(videoLayerIter);
    }
#endif

    m_nodes.remove(layerID);
}

void RemoteLayerTreeHost::animationDidStart(WebCore::PlatformLayerIdentifier layerID, CAAnimation *animation, MonotonicTime startTime)
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

void RemoteLayerTreeHost::animationDidEnd(WebCore::PlatformLayerIdentifier layerID, CAAnimation *animation)
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
    m_rootNode = nullptr;
}

CALayer *RemoteLayerTreeHost::layerWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    return layerForID(layerID);
}

CALayer *RemoteLayerTreeHost::layerForID(WebCore::PlatformLayerIdentifier layerID) const
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

    if (css3DTransformInteroperabilityEnabled() && [node->layer() respondsToSelector:@selector(setUsesWebKitBehavior:)]) {
        [node->layer() setUsesWebKitBehavior:YES];
        if ([node->layer() isKindOfClass:[CATransformLayer class]])
            [node->layer() setSortsSublayers:YES];
        else
            [node->layer() setSortsSublayers:NO];
    }

    if (auto* hostIdentifier = std::get_if<WebCore::LayerHostingContextIdentifier>(&properties.additionalData)) {
        m_hostingLayers.set(*hostIdentifier, properties.layerID);
        if (auto* hostedNode = nodeForID(m_hostedLayers.get(*hostIdentifier)))
            [node->layer() addSublayer:hostedNode->layer()];
    }

    m_nodes.add(properties.layerID, WTFMove(node));
}

#if !PLATFORM(IOS_FAMILY)
std::unique_ptr<RemoteLayerTreeNode> RemoteLayerTreeHost::makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties& properties)
{
    auto makeWithLayer = [&] (RetainPtr<CALayer>&& layer) {
        return makeUnique<RemoteLayerTreeNode>(properties.layerID, properties.hostIdentifier(), WTFMove(layer));
    };

    switch (properties.type) {
    case PlatformCALayer::LayerType::LayerTypeLayer:
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeRootLayer:
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
#endif
    case PlatformCALayer::LayerType::LayerTypeHost:
    case PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer: {
        auto layer = RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);
        // So that the scrolling thread's performance logging code can find all the tiles, mark this as being a tile.
        if (properties.type == PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer)
            [layer->layer() setValue:@YES forKey:@"isTile"];
        return layer;
    }

    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
        return makeWithLayer(adoptNS([[CATransformLayer alloc] init]));

    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
        return makeWithLayer(adoptNS([[CABackdropLayer alloc] init]));
    case PlatformCALayer::LayerType::LayerTypeCustom:
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
        if (m_isDebugLayerTreeHost)
            return RemoteLayerTreeNode::createWithPlainLayer(properties.layerID);

#if HAVE(AVKIT)
        if (properties.videoElementData) {
            if (auto videoManager = m_drawingArea->page().videoPresentationManager()) {
                m_videoLayers.add(properties.layerID, properties.videoElementData->playerIdentifier);
                return makeWithLayer(videoManager->createLayerWithID(properties.videoElementData->playerIdentifier, properties.hostingContextID(), properties.videoElementData->initialSize, properties.videoElementData->naturalSize, properties.hostingDeviceScaleFactor()));
            }
        }
#endif

        return makeWithLayer([CALayer _web_renderLayerWithContextID:properties.hostingContextID() shouldPreserveFlip:properties.preservesFlip()]);

    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
        return makeWithLayer(adoptNS([[CAShapeLayer alloc] init]));
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

void RemoteLayerTreeHost::detachRootLayer()
{
    if (!m_rootNode)
        return;
    m_rootNode->detachFromParent();
    m_rootNode = nullptr;
}

static void recursivelyMapIOSurfaceBackingStore(CALayer *layer)
{
    if (layer.contents && CFGetTypeID((__bridge CFTypeRef)layer.contents) == CAMachPortGetTypeID()) {
        MachSendRight port = MachSendRight::create(CAMachPortGetPort((__bridge CAMachPortRef)layer.contents));
        auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(port));
        layer.contents = surface ? surface->asLayerContents() : nil;
    }

    for (CALayer *sublayer in layer.sublayers)
        recursivelyMapIOSurfaceBackingStore(sublayer);
}

void RemoteLayerTreeHost::mapAllIOSurfaceBackingStore()
{
    recursivelyMapIOSurfaceBackingStore(rootLayer());
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void RemoteLayerTreeHost::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    m_drawingArea->animationsWereAddedToNode(node);
}

void RemoteLayerTreeHost::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    m_drawingArea->animationsWereRemovedFromNode(node);
}

Seconds RemoteLayerTreeHost::acceleratedTimelineTimeOrigin() const
{
    return m_drawingArea->acceleratedTimelineTimeOrigin();
}
#endif

} // namespace WebKit

#undef REMOTE_LAYER_TREE_HOST_RELEASE_LOG
