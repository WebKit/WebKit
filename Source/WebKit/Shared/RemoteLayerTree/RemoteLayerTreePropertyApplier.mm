/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import <QuartzCore/QuartzCore.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>

#if PLATFORM(IOS_FAMILY)
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
    NSUInteger remainingSubviews = self.subviews.count;
    for (NSUInteger i = currIndex; i < remainingSubviews; ++i)
        [[self.subviews objectAtIndex:currIndex] removeFromSuperview];

    ASSERT([self.subviews isEqualToArray:newSubviews]);
}

@end
#endif

namespace WebKit {
using namespace WebCore;

static CGColorRef cgColorFromColor(const Color& color)
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
#if ENABLE(RUBBER_BANDING)
    switch (customAppearance) {
    case GraphicsLayer::CustomAppearance::None:
    case GraphicsLayer::CustomAppearance::DarkBackdrop:
    case GraphicsLayer::CustomAppearance::LightBackdrop:
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

void RemoteLayerTreePropertyApplier::applyPropertiesToLayer(CALayer *layer, RemoteLayerTreeHost* layerTreeHost, const RemoteLayerTreeTransaction::LayerProperties& properties, RemoteLayerBackingStore::LayerContentsType layerContentsType)
{
    if (properties.changedProperties & RemoteLayerTreeTransaction::NameChanged)
        layer.name = properties.name;

    if (properties.changedProperties & RemoteLayerTreeTransaction::PositionChanged) {
        layer.position = CGPointMake(properties.position.x(), properties.position.y());
        layer.zPosition = properties.position.z();
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged) {
        layer.anchorPoint = CGPointMake(properties.anchorPoint.x(), properties.anchorPoint.y());
        layer.anchorPointZ = properties.anchorPoint.z();
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::BoundsChanged)
        layer.bounds = properties.bounds;
    
    if (properties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged)
        layer.backgroundColor = cgColorFromColor(properties.backgroundColor);

    if (properties.changedProperties & RemoteLayerTreeTransaction::BorderColorChanged)
        layer.borderColor = cgColorFromColor(properties.borderColor);

    if (properties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
        layer.borderWidth = properties.borderWidth;

    if (properties.changedProperties & RemoteLayerTreeTransaction::OpacityChanged)
        layer.opacity = properties.opacity;

    if (properties.changedProperties & RemoteLayerTreeTransaction::TransformChanged)
        layer.transform = properties.transform ? (CATransform3D)*properties.transform.get() : CATransform3DIdentity;

    if (properties.changedProperties & RemoteLayerTreeTransaction::SublayerTransformChanged)
        layer.sublayerTransform = properties.sublayerTransform ? (CATransform3D)*properties.sublayerTransform.get() : CATransform3DIdentity;

    if (properties.changedProperties & RemoteLayerTreeTransaction::HiddenChanged)
        layer.hidden = properties.hidden;

    if (properties.changedProperties & RemoteLayerTreeTransaction::GeometryFlippedChanged)
        layer.geometryFlipped = properties.geometryFlipped;

    if (properties.changedProperties & RemoteLayerTreeTransaction::DoubleSidedChanged)
        layer.doubleSided = properties.doubleSided;

    if (properties.changedProperties & RemoteLayerTreeTransaction::MasksToBoundsChanged)
        layer.masksToBounds = properties.masksToBounds;

    if (properties.changedProperties & RemoteLayerTreeTransaction::OpaqueChanged)
        layer.opaque = properties.opaque;

    if (properties.changedProperties & RemoteLayerTreeTransaction::ContentsRectChanged)
        layer.contentsRect = properties.contentsRect;

    if (properties.changedProperties & RemoteLayerTreeTransaction::ContentsScaleChanged) {
        layer.contentsScale = properties.contentsScale;
        layer.rasterizationScale = properties.contentsScale;
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::CornerRadiusChanged)
        layer.cornerRadius = properties.cornerRadius;

    if (properties.changedProperties & RemoteLayerTreeTransaction::ShapeRoundedRectChanged) {
        Path path;
        if (properties.shapeRoundedRect)
            path.addRoundedRect(*properties.shapeRoundedRect);
        ASSERT([layer isKindOfClass:[CAShapeLayer class]]);
        [(CAShapeLayer *)layer setPath:path.platformPath()];
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::ShapePathChanged) {
        ASSERT([layer isKindOfClass:[CAShapeLayer class]]);
        [(CAShapeLayer *)layer setPath:properties.shapePath.platformPath()];
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::MinificationFilterChanged)
        layer.minificationFilter = toCAFilterType(properties.minificationFilter);

    if (properties.changedProperties & RemoteLayerTreeTransaction::MagnificationFilterChanged)
        layer.magnificationFilter = toCAFilterType(properties.magnificationFilter);

    if (properties.changedProperties & RemoteLayerTreeTransaction::BlendModeChanged)
        PlatformCAFilters::setBlendingFiltersOnLayer(layer, properties.blendMode);

    if (properties.changedProperties & RemoteLayerTreeTransaction::WindRuleChanged) {
        ASSERT([layer isKindOfClass:[CAShapeLayer class]]);
        CAShapeLayer *shapeLayer = (CAShapeLayer *)layer;
        switch (properties.windRule) {
        case WindRule::NonZero:
            shapeLayer.fillRule = @"non-zero";
            break;
        case WindRule::EvenOdd:
            shapeLayer.fillRule = @"even-odd";
            break;
        }
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::SpeedChanged)
        layer.speed = properties.speed;

    if (properties.changedProperties & RemoteLayerTreeTransaction::TimeOffsetChanged)
        layer.timeOffset = properties.timeOffset;

    if (properties.changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged
        || properties.changedProperties & RemoteLayerTreeTransaction::BackingStoreAttachmentChanged)
    {
        RemoteLayerBackingStore* backingStore = properties.backingStore.get();
        if (backingStore && properties.backingStoreAttached)
            backingStore->applyBackingStoreToLayer(layer, layerContentsType);
        else {
            layer.contents = nil;
            layer.contentsOpaque = NO;
        }
    }

    if (properties.changedProperties & RemoteLayerTreeTransaction::FiltersChanged)
        PlatformCAFilters::setFiltersOnLayer(layer, properties.filters ? *properties.filters : FilterOperations());

    if (properties.changedProperties & RemoteLayerTreeTransaction::AnimationsChanged)
        PlatformCAAnimationRemote::updateLayerAnimations(layer, layerTreeHost, properties.addedAnimations, properties.keyPathsOfAnimationsToRemove);

    if (properties.changedProperties & RemoteLayerTreeTransaction::EdgeAntialiasingMaskChanged)
        layer.edgeAntialiasingMask = properties.edgeAntialiasingMask;

    if (properties.changedProperties & RemoteLayerTreeTransaction::CustomAppearanceChanged)
        updateCustomAppearance(layer, properties.customAppearance);
}

void RemoteLayerTreePropertyApplier::applyProperties(RemoteLayerTreeNode& node, RemoteLayerTreeHost* layerTreeHost, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers, RemoteLayerBackingStore::LayerContentsType layerContentsType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    applyPropertiesToLayer(node.layer(), layerTreeHost, properties, layerContentsType);
    updateChildren(node, properties, relatedLayers);
    updateMask(node, properties, relatedLayers);

    if (properties.changedProperties & RemoteLayerTreeTransaction::EventRegionChanged)
        node.setEventRegion(properties.eventRegion ? std::make_unique<WebCore::Region>(*properties.eventRegion) : nullptr);

#if PLATFORM(IOS_FAMILY)
    applyPropertiesToUIView(node.uiView(), properties, relatedLayers);
#endif

    END_BLOCK_OBJC_EXCEPTIONS;
}

void RemoteLayerTreePropertyApplier::updateChildren(RemoteLayerTreeNode& node, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (!properties.changedProperties.contains(RemoteLayerTreeTransaction::ChildrenChanged))
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

    auto contentView = [&] {
        if (properties.customAppearance == GraphicsLayer::CustomAppearance::LightBackdrop || properties.customAppearance == GraphicsLayer::CustomAppearance::DarkBackdrop) {
            // This is a UIBackdropView, which should have children attached to
            // its content view, not directly on its layers.
            return [(_UIBackdropView *)node.uiView() contentView];
        }
        return node.uiView();
    };

    if (hasViewChildren()) {
        ASSERT(node.uiView());

        RetainPtr<NSMutableArray> subviews = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.children.size()]);
        for (auto& child : properties.children) {
            auto* childNode = relatedLayers.get(child);
            ASSERT(childNode);
            if (!childNode)
                continue;
            ASSERT(childNode->uiView());
            [subviews addObject:childNode->uiView()];
        }

        [contentView() _web_setSubviews:subviews.get()];
        return;
    }
#endif

    RetainPtr<NSMutableArray> sublayers = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.children.size()]);
    for (auto& child : properties.children) {
        auto* childNode = relatedLayers.get(child);
        ASSERT(childNode);
        if (!childNode)
            continue;
#if PLATFORM(IOS_FAMILY)
        ASSERT(!childNode->uiView());
#endif
        [sublayers addObject:childNode->layer()];
    }

    node.layer().sublayers = sublayers.get();
}

void RemoteLayerTreePropertyApplier::updateMask(RemoteLayerTreeNode& node, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (!properties.changedProperties.contains(RemoteLayerTreeTransaction::MaskLayerChanged))
        return;

    auto maskOwnerLayer = [&] {
        CALayer *layer = node.layer();
#if PLATFORM(IOS_FAMILY)
        if (properties.customAppearance == GraphicsLayer::CustomAppearance::LightBackdrop || properties.customAppearance == GraphicsLayer::CustomAppearance::DarkBackdrop) {
            // This is a UIBackdropView, which means any mask must be applied to the CABackdropLayer rather
            // that the view's layer. The backdrop is the first layer child.
            if (layer.sublayers.count && [layer.sublayers[0] isKindOfClass:[CABackdropLayer class]])
                layer = layer.sublayers[0];
        }
#endif
        return layer;
    };

    if (!properties.maskLayerID) {
        maskOwnerLayer().mask = nullptr;
        return;
    }

    auto* maskNode = relatedLayers.get(properties.maskLayerID);
    ASSERT(maskNode);
    if (!maskNode)
        return;
    CALayer *maskLayer = maskNode->layer();
    ASSERT(!maskLayer.superlayer);
    if (maskLayer.superlayer)
        return;
    maskOwnerLayer().mask = maskLayer;
}

#if PLATFORM(IOS_FAMILY)
void RemoteLayerTreePropertyApplier::applyPropertiesToUIView(UIView *view, const RemoteLayerTreeTransaction::LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (properties.changedProperties.containsAny({ RemoteLayerTreeTransaction::ContentsHiddenChanged, RemoteLayerTreeTransaction::UserInteractionEnabledChanged }))
        view.userInteractionEnabled = !properties.contentsHidden && properties.userInteractionEnabled;
}
#endif

} // namespace WebKit
