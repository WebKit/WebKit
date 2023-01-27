/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreePropertyApplier.h"

#import "PlatformCAAnimationRemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeInteractionRegionLayers.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/cocoa/VectorCocoa.h>

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#else
static void configureSeparatedLayer(CALayer *) { }
#endif
#endif

#if PLATFORM(IOS_FAMILY)
#import "RemoteLayerTreeViews.h"
#import <UIKit/UIView.h>
#import <UIKitSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
@interface UIView (WKUIViewUtilities)
- (void)_web_setSubviews:(NSArray *)subviews;
@end

@implementation UIView (WKUIViewUtilities)

- (void)_web_setSubviews:(NSArray *)newSubviews
{
    NSUInteger numOldSubviews = self.subviews.count;
    NSUInteger numNewSubviews = newSubviews.count;

    NSUInteger currIndex = 0;
    for (currIndex = 0; currIndex < numNewSubviews; ++currIndex) {
        UIView *currNewSubview = [newSubviews objectAtIndex:currIndex];

        if (currIndex < numOldSubviews) {
            UIView *existingSubview = [self.subviews objectAtIndex:currIndex];
            if (existingSubview == currNewSubview)
                continue;
        }

        // New or moved subview.
        [self insertSubview:currNewSubview atIndex:currIndex];
    }

    // Remove views at the end.
    RetainPtr<NSMutableArray> viewsToRemove;
    auto appendViewToRemove = [&viewsToRemove](UIView *view) {
        if (!viewsToRemove)
            viewsToRemove = adoptNS([[NSMutableArray alloc] init]);

        [viewsToRemove addObject:view];
    };

    NSUInteger remainingSubviews = self.subviews.count;
    for (NSUInteger i = currIndex; i < remainingSubviews; ++i) {
        UIView *subview = [self.subviews objectAtIndex:i];
        if ([subview conformsToProtocol:@protocol(WKContentControlled)])
            appendViewToRemove(subview);
    }

    if (viewsToRemove)
        [viewsToRemove makeObjectsPerformSelector:@selector(removeFromSuperview)];
}

@end
#endif

namespace WebKit {
using namespace WebCore;

static RetainPtr<CGColorRef> cgColorFromColor(const Color& color)
{
    if (!color.isValid())
        return nil;

    return cachedCGColor(color);
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

static void updateCustomAppearance(CALayer *layer, GraphicsLayer::CustomAppearance customAppearance)
{
#if HAVE(RUBBER_BANDING)
    switch (customAppearance) {
    case GraphicsLayer::CustomAppearance::None:
        ScrollbarThemeMac::removeOverhangAreaBackground(layer);
        ScrollbarThemeMac::removeOverhangAreaShadow(layer);
        break;
    case GraphicsLayer::CustomAppearance::ScrollingOverhang:
        ScrollbarThemeMac::setUpOverhangAreaBackground(layer);
        break;
    case GraphicsLayer::CustomAppearance::ScrollingShadow:
        ScrollbarThemeMac::setUpOverhangAreaShadow(layer);
        break;
    }
#else
    UNUSED_PARAM(customAppearance);
#endif
}

static void applyGeometryPropertiesToLayer(CALayer *layer, const RemoteLayerTreeTransaction::LayerProperties& properties)
{
    if (properties.changedProperties & LayerChange::PositionChanged) {
        layer.position = CGPointMake(properties.position.x(), properties.position.y());
        layer.zPosition = properties.position.z();
    }

    if (properties.changedProperties & LayerChange::AnchorPointChanged) {
        layer.anchorPoint = CGPointMake(properties.anchorPoint.x(), properties.anchorPoint.y());
        layer.anchorPointZ = properties.anchorPoint.z();
    }

    if (properties.changedProperties & LayerChange::BoundsChanged)
        layer.bounds = properties.bounds;

    if (properties.changedProperties & LayerChange::TransformChanged)
        layer.transform = properties.transform ? (CATransform3D)*properties.transform.get() : CATransform3DIdentity;

    if (properties.changedProperties & LayerChange::SublayerTransformChanged)
        layer.sublayerTransform = properties.sublayerTransform ? (CATransform3D)*properties.sublayerTransform.get() : CATransform3DIdentity;

    if (properties.changedProperties & LayerChange::HiddenChanged)
        layer.hidden = properties.hidden;

    if (properties.changedProperties & LayerChange::GeometryFlippedChanged)
        layer.geometryFlipped = properties.geometryFlipped;

    if (properties.changedProperties & LayerChange::ContentsScaleChanged) {
        layer.contentsScale = properties.contentsScale;
        layer.rasterizationScale = properties.contentsScale;
    }
}

void RemoteLayerTreePropertyApplier::applyPropertiesToLayer(CALayer *layer, RemoteLayerTreeHost* layerTreeHost, const RemoteLayerTreeTransaction::LayerProperties& properties, RemoteLayerBackingStore::LayerContentsType layerContentsType)
{
    applyGeometryPropertiesToLayer(layer, properties);

    if (properties.changedProperties & LayerChange::NameChanged)
        layer.name = properties.name;

    if (properties.changedProperties & LayerChange::BackgroundColorChanged)
        layer.backgroundColor = cgColorFromColor(properties.backgroundColor).get();

    if (properties.changedProperties & LayerChange::BorderColorChanged)
        layer.borderColor = cgColorFromColor(properties.borderColor).get();

    if (properties.changedProperties & LayerChange::BorderWidthChanged)
        layer.borderWidth = properties.borderWidth;

    if (properties.changedProperties & LayerChange::OpacityChanged)
        layer.opacity = properties.opacity;

    if (properties.changedProperties & LayerChange::DoubleSidedChanged)
        layer.doubleSided = properties.doubleSided;

    if (properties.changedProperties & LayerChange::MasksToBoundsChanged)
        layer.masksToBounds = properties.masksToBounds;

    if (properties.changedProperties & LayerChange::OpaqueChanged)
        layer.opaque = properties.opaque;

    if (properties.changedProperties & LayerChange::ContentsRectChanged)
        layer.contentsRect = properties.contentsRect;

    if (properties.changedProperties & LayerChange::CornerRadiusChanged)
        layer.cornerRadius = properties.cornerRadius;

    if (properties.changedProperties & LayerChange::ShapeRoundedRectChanged) {
        Path path;
        if (properties.shapeRoundedRect)
            path.addRoundedRect(*properties.shapeRoundedRect);
        dynamic_objc_cast<CAShapeLayer>(layer).path = path.platformPath();
    }

    if (properties.changedProperties & LayerChange::ShapePathChanged)
        dynamic_objc_cast<CAShapeLayer>(layer).path = properties.shapePath.platformPath();

    if (properties.changedProperties & LayerChange::MinificationFilterChanged)
        layer.minificationFilter = toCAFilterType(properties.minificationFilter);

    if (properties.changedProperties & LayerChange::MagnificationFilterChanged)
        layer.magnificationFilter = toCAFilterType(properties.magnificationFilter);

    if (properties.changedProperties & LayerChange::BlendModeChanged)
        PlatformCAFilters::setBlendingFiltersOnLayer(layer, properties.blendMode);

    if (properties.changedProperties & LayerChange::WindRuleChanged) {
        if (auto *shapeLayer = dynamic_objc_cast<CAShapeLayer>(layer)) {
            switch (properties.windRule) {
            case WindRule::NonZero:
                shapeLayer.fillRule = kCAFillRuleNonZero;
                break;
            case WindRule::EvenOdd:
                shapeLayer.fillRule = kCAFillRuleEvenOdd;
                break;
            }
        }
    }

    if (properties.changedProperties & LayerChange::SpeedChanged)
        layer.speed = properties.speed;

    if (properties.changedProperties & LayerChange::TimeOffsetChanged)
        layer.timeOffset = properties.timeOffset;

    if (properties.changedProperties & LayerChange::BackingStoreChanged
        || properties.changedProperties & LayerChange::BackingStoreAttachmentChanged)
    {
        auto* backingStore = properties.backingStore.get();
        if (backingStore && properties.backingStoreAttached)
            backingStore->applyBackingStoreToLayer(layer, layerContentsType, layerTreeHost->replayCGDisplayListsIntoBackingStore());
        else {
            layer.contents = nil;
            layer.contentsOpaque = NO;
        }
    }

    if (properties.changedProperties & LayerChange::FiltersChanged)
        PlatformCAFilters::setFiltersOnLayer(layer, properties.filters ? *properties.filters : FilterOperations());

    if (properties.changedProperties & LayerChange::AnimationsChanged)
        PlatformCAAnimationRemote::updateLayerAnimations(layer, layerTreeHost, properties.addedAnimations, properties.keysOfAnimationsToRemove);

    if (properties.changedProperties & LayerChange::AntialiasesEdgesChanged)
        layer.edgeAntialiasingMask = properties.antialiasesEdges ? (kCALayerLeftEdge | kCALayerRightEdge | kCALayerBottomEdge | kCALayerTopEdge) : 0;

    if (properties.changedProperties & LayerChange::CustomAppearanceChanged)
        updateCustomAppearance(layer, properties.customAppearance);

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (properties.changedProperties & LayerChange::SeparatedChanged) {
        layer.separated = properties.isSeparated;
        if (properties.isSeparated)
            configureSeparatedLayer(layer);
    }

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    if (properties.changedProperties & LayerChange::SeparatedPortalChanged) {
        // FIXME: Implement SeparatedPortalChanged.
    }

    if (properties.changedProperties & LayerChange::DescendentOfSeparatedPortalChanged) {
        // FIXME: Implement DescendentOfSeparatedPortalChanged.
    }
#endif
#endif
}

void RemoteLayerTreePropertyApplier::applyProperties(RemoteLayerTreeNode& node, RemoteLayerTreeHost* layerTreeHost, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers, RemoteLayerBackingStore::LayerContentsType layerContentsType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    applyPropertiesToLayer(node.layer(), layerTreeHost, properties, layerContentsType);
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    applyGeometryPropertiesToLayer(node.interactionRegionsLayer(), properties);
    if (properties.changedProperties & LayerChange::EventRegionChanged)
        updateLayersForInteractionRegions(node.interactionRegionsLayer(), *layerTreeHost, properties);
#endif
    updateMask(node, properties, relatedLayers);

    if (properties.changedProperties & LayerChange::EventRegionChanged)
        node.setEventRegion(properties.eventRegion);

#if ENABLE(SCROLLING_THREAD)
    if (properties.changedProperties & LayerChange::ScrollingNodeIDChanged)
        node.setScrollingNodeID(properties.scrollingNodeID);
#endif

#if PLATFORM(IOS_FAMILY)
    applyPropertiesToUIView(node.uiView(), properties, relatedLayers);
#endif

    END_BLOCK_OBJC_EXCEPTIONS
}

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
static void applyInteractionRegionsHierarchyUpdate(RemoteLayerTreeNode& node, const RemoteLayerTreeTransaction::LayerProperties& properties, const RemoteLayerTreePropertyApplier::RelatedLayerMap& relatedLayers)
{
    auto sublayers = createNSArray(properties.children, [&] (auto& child) -> CALayer * {
        auto* childNode = relatedLayers.get(child);
        ASSERT(childNode);
        if (!childNode)
            return nil;
        return childNode->interactionRegionsLayer();
    });

    insertInteractionRegionLayersForLayer(sublayers.get(), node.interactionRegionsLayer());
    node.interactionRegionsLayer().sublayers = sublayers.get();
}
#endif

void RemoteLayerTreePropertyApplier::applyHierarchyUpdates(RemoteLayerTreeNode& node, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (!properties.changedProperties.contains(LayerChange::ChildrenChanged))
        return;

#if PLATFORM(IOS_FAMILY)
    auto hasViewChildren = [&] {
        if (node.uiView() && [[node.uiView() subviews] count])
            return true;
        if (properties.children.isEmpty())
            return false;
        auto* childNode = relatedLayers.get(properties.children.first());
        ASSERT(childNode);
        return childNode && childNode->uiView();
    };

    if (hasViewChildren()) {
        ASSERT(node.uiView());
        [node.uiView() _web_setSubviews:createNSArray(properties.children, [&] (auto& child) -> UIView * {
            auto* childNode = relatedLayers.get(child);
            ASSERT(childNode);
            if (!childNode)
                return nil;
            ASSERT(childNode->uiView());
            return childNode->uiView();
        }).get()];
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        applyInteractionRegionsHierarchyUpdate(node, properties, relatedLayers);
#endif
        return;
    }
#endif

    auto sublayers = createNSArray(properties.children, [&] (auto& child) -> CALayer * {
        auto* childNode = relatedLayers.get(child);
        ASSERT(childNode);
        if (!childNode)
            return nil;
#if PLATFORM(IOS_FAMILY)
        ASSERT(!childNode->uiView());
#endif
        return childNode->layer();
    });

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    applyInteractionRegionsHierarchyUpdate(node, properties, relatedLayers);
#endif

    node.layer().sublayers = sublayers.get();

    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreePropertyApplier::updateMask(RemoteLayerTreeNode& node, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (!properties.changedProperties.contains(LayerChange::MaskLayerChanged))
        return;

    auto maskOwnerLayer = node.layer();

    if (!properties.maskLayerID) {
        maskOwnerLayer.mask = nullptr;
        return;
    }

    auto* maskNode = properties.maskLayerID ? relatedLayers.get(*properties.maskLayerID) : nullptr;
    ASSERT(maskNode);
    if (!maskNode)
        return;
    CALayer *maskLayer = maskNode->layer();
    ASSERT(!maskLayer.superlayer);
    if (maskLayer.superlayer)
        return;
    maskOwnerLayer.mask = maskLayer;
}

#if PLATFORM(IOS_FAMILY)
void RemoteLayerTreePropertyApplier::applyPropertiesToUIView(UIView *view, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (properties.changedProperties.containsAny({ LayerChange::ContentsHiddenChanged, LayerChange::UserInteractionEnabledChanged }))
        view.userInteractionEnabled = !properties.contentsHidden && properties.userInteractionEnabled;
}
#endif

} // namespace WebKit
