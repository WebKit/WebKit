/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "RemoteAnimationVisibility.h"

#if ENABLE(THREADED_ANIMATIONS)

#import "RemoteAnimationStack.h"
#import "RemoteLayerTreeNode.h"
#import <QuartzCore/CALayer.h>
#import <wtf/Borrow.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIScrollView.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
// Content within this fraction of a scroll view's size outside of its visible area may soon be
// scrolled into view, so it is considered visible.
static constexpr CGFloat scrollViewVisibilityMarginRatio = 0.5;
#endif

auto RemoteAnimationVisibility::update(CALayer *webViewLayer, const Vector<AnimatedLayer>& animatedLayers, NOESCAPE const AnimationStackForNode& animationStackForNode) -> Vector<AnimatedLayer>
{
    m_needsUpdate = false;

    VisibilityCache visibilityCache;
    HashSet<WebCore::PlatformLayerIdentifier> layerIDsThatMayBeVisible;
    Vector<AnimatedLayer> animatedLayersThatBecameVisible;
    bool hasVisibleMonotonicAnimations = false;
    bool hasHighImpactMonotonicAnimationsThatMayBeVisible = false;

    for (auto& animatedLayer : borrow(animatedLayers)) {
        RefPtr node = animatedLayer.node;
        if (node && !animatedContentMayBeVisible(*node, webViewLayer, visibilityCache, animationStackForNode))
            continue;

        layerIDsThatMayBeVisible.add(animatedLayer.layerID);
        if (node && node->hasHighImpactMonotonicAnimations())
            hasHighImpactMonotonicAnimationsThatMayBeVisible = true;

        if (!animatedLayer.animationStack->hasTimeBasedAnimations())
            continue;

        hasVisibleMonotonicAnimations = true;
        if (!m_layerIDsThatMayBeVisible.contains(animatedLayer.layerID))
            animatedLayersThatBecameVisible.append(animatedLayer);
    }

    m_layerIDsThatMayBeVisible = WTF::move(layerIDsThatMayBeVisible);
    m_hasVisibleMonotonicAnimations = hasVisibleMonotonicAnimations;
    m_hasHighImpactMonotonicAnimationsThatMayBeVisible = hasHighImpactMonotonicAnimationsThatMayBeVisible;
    return animatedLayersThatBecameVisible;
}

bool RemoteAnimationVisibility::animatedContentMayBeVisible(const RemoteLayerTreeNode& node, CALayer *webViewLayer, VisibilityCache& visibilityCache, NOESCAPE const AnimationStackForNode& animationStackForNode)
{
    auto iterator = visibilityCache.find(node.layerID());
    if (iterator != visibilityCache.end())
        return iterator->value;

    auto mayBeVisible = computeAnimatedContentMayBeVisible(node, webViewLayer, visibilityCache, animationStackForNode);
    visibilityCache.set(node.layerID(), mayBeVisible);
    return mayBeVisible;
}

bool RemoteAnimationVisibility::computeAnimatedContentMayBeVisible(const RemoteLayerTreeNode& node, CALayer *webViewLayer, VisibilityCache& visibilityCache, NOESCAPE const AnimationStackForNode& animationStackForNode)
{
    // Without a known extent, the animated content could be anywhere.
    auto& animationExtent = node.animationExtent();
    if (!animationExtent)
        return true;

    RetainPtr layer = node.layer();
    RetainPtr<CALayer> ancestor = [layer superlayer];
    // This layer is not in the hierarchy yet, we'll find out once the commit is applied.
    if (!ancestor)
        return true;

    if (!webViewLayer)
        return true;

    // The extent is in the layer's coordinate space without its transform, which is what time-based animations
    // change, so map it to the superlayer's coordinate space using only the layer's position.
    auto bounds = [layer bounds];
    auto anchorPoint = [layer anchorPoint];
    auto position = [layer position];
    CGRect rect = CGRectOffset(*animationExtent, position.x - bounds.origin.x - anchorPoint.x * bounds.size.width, position.y - bounds.origin.y - anchorPoint.y * bounds.size.height);

    while (true) {
        // Layers outside of the web view's layer don't affect the visibility of its content.
        if (ancestor == webViewLayer)
            return true;

        if ([ancestor isHidden] || ![ancestor opacity])
            return false;

        if ([ancestor masksToBounds]) {
            auto clipRect = [ancestor bounds];
#if PLATFORM(IOS_FAMILY)
            if ([[ancestor delegate] isKindOfClass:UIScrollView.class])
                clipRect = CGRectInset(clipRect, -clipRect.size.width * scrollViewVisibilityMarginRatio, -clipRect.size.height * scrollViewVisibilityMarginRatio);
#endif
            rect = CGRectIntersection(rect, clipRect);
            if (CGRectIsEmpty(rect))
                return false;
        }

        // Assume this layer may be visible if the ancestor's layer itself may be.
        if (RefPtr ancestorNode = RemoteLayerTreeNode::forCALayer(ancestor.get())) {
            RefPtr animationStack = animationStackForNode(*ancestorNode);
            if (animationStack && animationStack->hasTimeBasedAnimations() && animationStack->affectsTransform())
                return animatedContentMayBeVisible(*ancestorNode, webViewLayer, visibilityCache, animationStackForNode);
        }

        RetainPtr<CALayer> superlayer = [ancestor superlayer];
        if (!superlayer)
            return true;

        rect = [ancestor convertRect:rect toLayer:superlayer.get()];
        ancestor = WTF::move(superlayer);
    }
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
