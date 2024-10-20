/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeNode.h"

#import "RemoteLayerTreeLayers.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIView.h>
#endif

#if PLATFORM(VISION)
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#import "RemoteLayerTreeHost.h"
#import <WebCore/AcceleratedEffectStack.h>
#endif

namespace WebKit {

static NSString *const WKRemoteLayerTreeNodePropertyKey = @"WKRemoteLayerTreeNode";
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
static NSString *const WKInteractionRegionContainerKey = @"WKInteractionRegionContainer";
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeNode);

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::create(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
{
    return adoptRef(*new RemoteLayerTreeNode(layerID, hostIdentifier, WTFMove(layer)));
}

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer(WTFMove(layer))
{
    initializeLayer();
    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
}

#if PLATFORM(IOS_FAMILY)

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::create(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
{
    return adoptRef(*new RemoteLayerTreeNode(layerID, hostIdentifier, WTFMove(uiView)));
}

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer([uiView.get() layer])
    , m_uiView(WTFMove(uiView))
{
    initializeLayer();
}
#endif

RemoteLayerTreeNode::~RemoteLayerTreeNode()
{
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    if (m_effectStack)
        m_effectStack->clear(layer());
#endif
    [layer() setValue:nil forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    removeInteractionRegionsContainer();
#endif
}

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::createWithPlainLayer(WebCore::PlatformLayerIdentifier layerID)
{
    RetainPtr<CALayer> layer = adoptNS([[WKCompositingLayer alloc] init]);
    return RemoteLayerTreeNode::create(layerID, std::nullopt, WTFMove(layer));
}

void RemoteLayerTreeNode::detachFromParent()
{
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    removeInteractionRegionsContainer();
#endif
#if PLATFORM(IOS_FAMILY)
    if (auto view = uiView()) {
        [view removeFromSuperview];
        return;
    }
#endif
    [layer() removeFromSuperlayer];
}

void RemoteLayerTreeNode::setEventRegion(const WebCore::EventRegion& eventRegion)
{
    m_eventRegion = eventRegion;
}

void RemoteLayerTreeNode::initializeLayer()
{
    [layer() setValue:[NSValue valueWithPointer:this] forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    if (![layer() isKindOfClass:[CATransformLayer class]])
        [layer() setHitTestsContentsAlphaChannel:YES];
#endif
}

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
CALayer* RemoteLayerTreeNode::ensureInteractionRegionsContainer()
{
    if (m_interactionRegionsContainer)
        return [m_interactionRegionsContainer layer];

    m_interactionRegionsContainer = adoptNS([[UIView alloc] init]);
    [[m_interactionRegionsContainer layer] setName:@"InteractionRegions Container"];
    [[m_interactionRegionsContainer layer] setValue:@YES forKey:WKInteractionRegionContainerKey];

    repositionInteractionRegionsContainerIfNeeded();
    propagateInteractionRegionsChangeInHierarchy(InteractionRegionsInSubtree::Yes);

    return [m_interactionRegionsContainer layer];
}

void RemoteLayerTreeNode::removeInteractionRegionsContainer()
{
    if (!m_interactionRegionsContainer)
        return;

    [m_interactionRegionsContainer removeFromSuperview];
    m_interactionRegionsContainer = nullptr;

    propagateInteractionRegionsChangeInHierarchy(InteractionRegionsInSubtree::Unknown);
}

void RemoteLayerTreeNode::updateInteractionRegionAfterHierarchyChange()
{
    repositionInteractionRegionsContainerIfNeeded();

    bool hasInteractionRegionsDescendant = false;
    for (CALayer *sublayer in layer().sublayers) {
        if (auto *subnode = forCALayer(sublayer)) {
            if (subnode->hasInteractionRegions()) {
                hasInteractionRegionsDescendant = true;
                break;
            }
        }
    }

    if (m_hasInteractionRegionsDescendant == hasInteractionRegionsDescendant)
        return;

    setHasInteractionRegionsDescendant(hasInteractionRegionsDescendant);
    propagateInteractionRegionsChangeInHierarchy(hasInteractionRegionsDescendant ? InteractionRegionsInSubtree::Yes : InteractionRegionsInSubtree::Unknown);
}

bool RemoteLayerTreeNode::hasInteractionRegions() const
{
    return m_hasInteractionRegionsDescendant || m_interactionRegionsContainer;
}

void RemoteLayerTreeNode::repositionInteractionRegionsContainerIfNeeded()
{
    if (!m_interactionRegionsContainer)
        return;
    ASSERT(uiView());

    NSUInteger insertionPoint = 0;
    for (UIView *subview in uiView().subviews) {
        if ([subview.layer valueForKey:WKInteractionRegionContainerKey])
            continue;

        if (auto *subnode = forCALayer(subview.layer)) {
            if (subnode->hasInteractionRegions())
                break;
        }

        insertionPoint++;
    }

    // We searched through the subviews, so insertionPoint is always <= subviews.count.
    ASSERT(insertionPoint <= uiView().subviews.count);
    bool shouldAppendView = insertionPoint == uiView().subviews.count;
    if (shouldAppendView || [uiView().subviews objectAtIndex:insertionPoint] != m_interactionRegionsContainer) {
        [m_interactionRegionsContainer removeFromSuperview];
        [uiView() insertSubview:m_interactionRegionsContainer.get() atIndex:insertionPoint];
    }
}

void RemoteLayerTreeNode::propagateInteractionRegionsChangeInHierarchy(InteractionRegionsInSubtree interactionRegionsInSubtree)
{
    for (auto* parentNode = forCALayer(layer().superlayer); parentNode; parentNode = forCALayer(parentNode->layer().superlayer)) {
        parentNode->repositionInteractionRegionsContainerIfNeeded();

        bool originalFlag = parentNode->hasInteractionRegionsDescendant();

        if (originalFlag && interactionRegionsInSubtree == InteractionRegionsInSubtree::Yes)
            break;

        if (interactionRegionsInSubtree == InteractionRegionsInSubtree::Yes) {
            parentNode->setHasInteractionRegionsDescendant(true);
            continue;
        }

        bool hasInteractionRegionsDescendant = false;
        for (CALayer *sublayer in parentNode->layer().sublayers) {
            if (auto *subnode = forCALayer(sublayer)) {
                if (subnode->hasInteractionRegions()) {
                    hasInteractionRegionsDescendant = true;
                    break;
                }
            }
        }

        if (originalFlag == hasInteractionRegionsDescendant)
            break;

        parentNode->setHasInteractionRegionsDescendant(hasInteractionRegionsDescendant);
        if (hasInteractionRegionsDescendant)
            interactionRegionsInSubtree = InteractionRegionsInSubtree::Yes;
    }
}
#endif

std::optional<WebCore::PlatformLayerIdentifier> RemoteLayerTreeNode::layerID(CALayer *layer)
{
    RefPtr node = forCALayer(layer);
    return node ? std::optional { node->layerID() } : std::nullopt;
}

RemoteLayerTreeNode* RemoteLayerTreeNode::forCALayer(CALayer *layer)
{
    return static_cast<RemoteLayerTreeNode*>([[layer valueForKey:WKRemoteLayerTreeNodePropertyKey] pointerValue]);
}

NSString *RemoteLayerTreeNode::appendLayerDescription(NSString *description, CALayer *layer)
{
    auto layerID = WebKit::RemoteLayerTreeNode::layerID(layer);
    NSString *layerDescription = [NSString stringWithFormat:@" layerID = %llu \"%@\"", layerID ? layerID->object().toUInt64() : 0, layer.name ? layer.name : @""];
    return [description stringByAppendingString:layerDescription];
}

void RemoteLayerTreeNode::addToHostingNode(RemoteLayerTreeNode& hostingNode)
{
#if PLATFORM(IOS_FAMILY)
    [hostingNode.uiView() addSubview:uiView()];
#else
    [hostingNode.layer() addSublayer:layer()];
#endif
}

void RemoteLayerTreeNode::removeFromHostingNode()
{
#if PLATFORM(IOS_FAMILY)
    [uiView() removeFromSuperview];
#else
    [layer() removeFromSuperlayer];
#endif
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void RemoteLayerTreeNode::setAcceleratedEffectsAndBaseValues(const WebCore::AcceleratedEffects& effects, const WebCore::AcceleratedEffectValues& baseValues, RemoteLayerTreeHost& host)
{
    ASSERT(isUIThread());

    if (m_effectStack)
        m_effectStack->clear(layer());
    host.animationsWereRemovedFromNode(*this);

    if (effects.isEmpty())
        return;

    m_effectStack = RemoteAcceleratedEffectStack::create(layer().bounds, host.acceleratedTimelineTimeOrigin(m_layerID.processIdentifier()));

    auto clonedEffects = effects;
    auto clonedBaseValues = baseValues.clone();

    m_effectStack->setEffects(WTFMove(clonedEffects));
    m_effectStack->setBaseValues(WTFMove(clonedBaseValues));

#if PLATFORM(IOS_FAMILY)
    m_effectStack->applyEffectsFromMainThread(layer(), host.animationCurrentTime(m_layerID.processIdentifier()), backdropRootIsOpaque());
#else
    m_effectStack->initEffectsFromMainThread(layer(), host.animationCurrentTime(m_layerID.processIdentifier()));
#endif

    host.animationsWereAddedToNode(*this);
}
#endif

}
