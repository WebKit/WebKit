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

#import "RemoteLayerTreeHost.h"
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

#if ENABLE(OVERLAY_REGIONS_REMOTE_EFFECT)
#import "UIKitSPI.h"
#import "WKBaseScrollView.h"
#import <wtf/cocoa/VectorCocoa.h>
#endif

#if ENABLE(THREADED_ANIMATIONS)
#import "RemoteAnimation.h"
#import "RemoteAnimationStack.h"
#endif

namespace WebKit {

static NSString *const WKRemoteLayerTreeNodePropertyKey = @"WKRemoteLayerTreeNode";
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
static NSString *const WKInteractionRegionContainerKey = @"WKInteractionRegionContainer";
#endif
#if ENABLE(OVERLAY_REGIONS_REMOTE_EFFECT)
// FIXME: rdar://169697770
static NSString *const WKOverlayRegionEffectKey = @"overlayRegion";
static NSString *const WKLookToScrollExclusionEffectName = @"lookToScrollExclusionEffect";
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeNode);

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::create(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
{
    return adoptRef(*new RemoteLayerTreeNode(layerID, hostIdentifier, WTF::move(layer)));
}

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer(WTF::move(layer))
{
    initializeLayer();
    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
}

#if PLATFORM(IOS_FAMILY)

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::create(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
{
    return adoptRef(*new RemoteLayerTreeNode(layerID, hostIdentifier, WTF::move(uiView)));
}

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer([uiView.get() layer])
    , m_uiView(WTF::move(uiView))
{
    initializeLayer();
}
#endif

RemoteLayerTreeNode::~RemoteLayerTreeNode()
{
    RetainPtr layer = this->layer();
#if ENABLE(THREADED_ANIMATIONS)
    if (RefPtr animationStack = m_animationStack)
        animationStack->clear(layer.get());
#endif
    [layer setValue:nil forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    removeInteractionRegionsContainer();
#endif
}

Ref<RemoteLayerTreeNode> RemoteLayerTreeNode::createWithPlainLayer(WebCore::PlatformLayerIdentifier layerID)
{
    RetainPtr<CALayer> layer = adoptNS([[WKCompositingLayer alloc] init]);
    return RemoteLayerTreeNode::create(layerID, std::nullopt, WTF::move(layer));
}

void RemoteLayerTreeNode::detachFromParent()
{
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    removeInteractionRegionsContainer();
#endif
#if PLATFORM(IOS_FAMILY)
    if (RetainPtr view = uiView()) {
        [view removeFromSuperview];
        return;
    }
#endif
    [protect(layer()) removeFromSuperlayer];
}

void RemoteLayerTreeNode::setEventRegion(const WebCore::EventRegion& eventRegion)
{
#if ENABLE(OVERLAY_REGIONS_REMOTE_EFFECT)
    bool regionChanged = (m_eventRegion != eventRegion);
#endif
    m_eventRegion = eventRegion;
#if ENABLE(OVERLAY_REGIONS_REMOTE_EFFECT)
    if (regionChanged)
        updateExclusionRegion(EventRegionChanged::Yes);
#endif
}

void RemoteLayerTreeNode::initializeLayer()
{
    RetainPtr layer = this->layer();
    [layer setValue:[NSValue valueWithPointer:this] forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    if (![layer isKindOfClass:[CATransformLayer class]])
        [layer setHitTestsContentsAlphaChannel:YES];
#endif
}

void RemoteLayerTreeNode::applyBackingStore(RemoteLayerTreeHost* host, RemoteLayerBackingStoreProperties& properties)
{
    if (asyncContentsIdentifier() && properties.contentsRenderingResourceIdentifier() && *asyncContentsIdentifier() >= *properties.contentsRenderingResourceIdentifier())
        return;

    RetainPtr<UIView> hostingView;
#if PLATFORM(IOS_FAMILY)
    hostingView = uiView();
#endif

    properties.applyBackingStoreToNode(*this, host->replayDynamicContentScalingDisplayListsIntoBackingStore(), hostingView.get());

    if (auto identifier = properties.contentsRenderingResourceIdentifier())
        setAsyncContentsIdentifier(*identifier);
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
        } else
            break; // Don't go above views we don't own.

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

#if ENABLE(OVERLAY_REGIONS_REMOTE_EFFECT)
void RemoteLayerTreeNode::setIsLookToScrollExclusion(bool value)
{
    if (value == m_isLookToScrollExclusion)
        return;

    m_isLookToScrollExclusion = value;
    updateExclusionRegion();
}

void RemoteLayerTreeNode::updateExclusionRegion(EventRegionChanged eventRegionChanged)
{
    bool shouldHaveEffect = m_isLookToScrollExclusion
        && m_hasVisibleRect
        && !eventRegion().region().isEmpty();

    if (eventRegionChanged == EventRegionChanged::No && shouldHaveEffect == m_hasLookToScrollExclusionEffect)
        return;

    RetainPtr overlayView = uiView();
    if (!overlayView)
        return;

    if (shouldHaveEffect) {
        CARemoteEffect *effect = [CARemoteExternalEffect effectWithName:WKLookToScrollExclusionEffectName];
        auto& region = eventRegion().region();

        bool isFullLayerRegion = false;
        if (region.isRect()) {
            CGRect layerBounds = [layer() bounds];
            auto regionBounds = region.bounds();

            isFullLayerRegion = std::abs(regionBounds.x() - layerBounds.origin.x) <= 1
                && std::abs(regionBounds.y() - layerBounds.origin.y) <= 1
                && std::abs(regionBounds.width() - layerBounds.size.width) <= 1
                && std::abs(regionBounds.height() - layerBounds.size.height) <= 1;
        }

        if (!isFullLayerRegion) {
            effect.userInfo = @{ @"effectiveRects": createNSArray(region.rects(), [] (const auto& rect) {
                return @[@(rect.x()), @(rect.y()), @(rect.width()), @(rect.height())];
            }).autorelease() };
        }

        [overlayView _requestRemoteEffects:@[effect] forKey:WKOverlayRegionEffectKey];
    } else if (m_hasLookToScrollExclusionEffect)
        [overlayView _removeRemoteEffectsForKey:WKOverlayRegionEffectKey];

    m_hasLookToScrollExclusionEffect = shouldHaveEffect;
}

void RemoteLayerTreeNode::updateExclusionRegionAndDescendants(bool isExclusion)
{
    if (m_isLookToScrollExclusion == isExclusion)
        return;

    if (RetainPtr view = uiView()) {
        if ([view isKindOfClass:[WKBaseScrollView class]])
            return;
    }

    setIsLookToScrollExclusion(isExclusion);

    for (CALayer *sublayer in layer().sublayers) {
        if (RefPtr subnode = forCALayer(sublayer)) {
            if (subnode->isFixedSubtreeRoot())
                continue;
            subnode->updateExclusionRegionAndDescendants(isExclusion);
        }
    }
}

void RemoteLayerTreeNode::visibleRectChangedForOverlayRegions()
{
    bool hasVisibleRect = !!m_visibleRect;
    if (m_hasVisibleRect == hasVisibleRect)
        return;

    m_hasVisibleRect = hasVisibleRect;
    updateExclusionRegion();
}

void RemoteLayerTreeNode::updateOverlayRegionAfterHierarchyChange()
{
    for (CALayer *sublayer in layer().sublayers) {
        if (RefPtr subnode = forCALayer(sublayer)) {
            if (subnode->isFixedSubtreeRoot())
                continue;
            subnode->updateExclusionRegionAndDescendants(m_isLookToScrollExclusion);
        }
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
    RetainPtr<NSString> name = layer.name ? layer.name : @"";
    RetainPtr layerDescription = adoptNS([[NSString alloc] initWithFormat:@" layerID = %llu \"%@\"", layerID ? layerID->object().toUInt64() : 0, name.get()]);
    return [description stringByAppendingString:layerDescription.get()];
}

void RemoteLayerTreeNode::addToHostingNode(RemoteLayerTreeNode& hostingNode)
{
#if PLATFORM(IOS_FAMILY)
    [protect(hostingNode.uiView()) addSubview:protect(uiView()).get()];
#else
    [protect(hostingNode.layer()) addSublayer:protect(layer()).get()];
#endif
}

void RemoteLayerTreeNode::removeFromHostingNode()
{
#if PLATFORM(IOS_FAMILY)
    [protect(uiView()) removeFromSuperview];
#else
    [protect(layer()) removeFromSuperlayer];
#endif
}

#if ENABLE(THREADED_ANIMATIONS)
void RemoteLayerTreeNode::setAcceleratedEffectsAndBaseValues(const WebCore::AcceleratedEffects& effects, const WebCore::AcceleratedEffectValues& baseValues, RemoteLayerTreeHost& host)
{
    ASSERT(isUIThread());

    RetainPtr layer = this->layer();
    if (RefPtr animationStack = m_animationStack)
        animationStack->clear(layer.get());
    host.animationsWereRemovedFromNode(*this);

    m_hasHighImpactMonotonicAnimations = false;

    if (effects.isEmpty())
        return;

    Ref animationStack = RemoteAnimationStack::create(effects.map([&](const Ref<WebCore::AcceleratedEffect>& effect) {
        TimelineID timelineID { effect->timelineIdentifier(), m_layerID.processIdentifier() };
        RefPtr timeline = host.timeline(timelineID);
        RELEASE_ASSERT(timeline, "Remote layer tree animations should have a pre-registered timeline.");
        if (!m_hasHighImpactMonotonicAnimations && timeline->isMonotonic())
            m_hasHighImpactMonotonicAnimations = effect->hasHighImpact();
        return RemoteAnimation::create(Ref { effect }.get(), *timeline);
    }), baseValues.clone(), layer.get().bounds);
    m_animationStack = animationStack.copyRef();

#if PLATFORM(IOS_FAMILY)
    animationStack->applyEffectsFromMainThread(layer.get(), backdropRootIsOpaque());
#else
    animationStack->initEffectsFromMainThread(layer.get());
#endif

    host.animationsWereAddedToNode(*this);
}
#endif

}
