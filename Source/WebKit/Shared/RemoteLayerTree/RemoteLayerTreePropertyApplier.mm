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

#import "Logging.h"
#import "PlatformCAAnimationRemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeInteractionRegionLayers.h"
#import "WKVideoView.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ContentsFormatCocoa.h>
#import <WebCore/GraphicsLayerEnums.h>
#import <WebCore/MediaPlayerEnumsCocoa.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/ScrollbarThemeMac.h>
#import <WebCore/WebAVPlayerLayer.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
#import "WKSeparatedImageView.h"

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#else
static void configureSeparatedLayer(CALayer *) { }
#endif

#endif // HAVE(CORE_ANIMATION_SEPARATED_LAYERS)

#if PLATFORM(IOS_FAMILY)
#import "RemoteLayerTreeViews.h"
#import <UIKit/UIView.h>
#import <UIKitSPI.h>
#endif

#if HAVE(MATERIAL_HOSTING)
#import "WKMaterialHostingSupport.h"
#endif

#if HAVE(CORE_MATERIAL)
#import <pal/cocoa/CoreMaterialSoftLink.h>
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

    // This method does not handle interleaved UIView and CALayer children of
    // a UIView, so we must not have any non-UIView-backed CALayer children.
    ASSERT(numOldSubviews == self.layer.sublayers.count);

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

static RetainPtr<NSString> toCAFilterType(PlatformCALayer::FilterType type)
{
    switch (type) {
    case PlatformCALayer::FilterType::Linear:
        return kCAFilterLinear;
    case PlatformCALayer::FilterType::Nearest:
        return kCAFilterNearest;
    case PlatformCALayer::FilterType::Trilinear:
        return kCAFilterTrilinear;
    };

    ASSERT_NOT_REACHED();
    return 0;
}

#if HAVE(CORE_MATERIAL)

static MTCoreMaterialRecipe materialRecipeForAppleVisualEffect(AppleVisualEffect effect, AppleVisualEffectData::ColorScheme colorScheme)
{
    bool isDark = colorScheme == AppleVisualEffectData::ColorScheme::Dark;

    switch (effect) {
    case AppleVisualEffect::BlurUltraThinMaterial:
        return isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentUltraThinDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentUltraThinLightSingleton();
    case AppleVisualEffect::BlurThinMaterial:
        return isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThinDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThinLightSingleton();
    case AppleVisualEffect::BlurMaterial:
        return isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentLightSingleton();
    case AppleVisualEffect::BlurThickMaterial:
        return isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThickDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThickLightSingleton();
    case AppleVisualEffect::BlurChromeMaterial:
        return isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformChromeDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformChromeLightSingleton();
    case AppleVisualEffect::None:
        return PAL::get_CoreMaterial_MTCoreMaterialRecipeNoneSingleton();
#if HAVE(MATERIAL_HOSTING)
    case AppleVisualEffect::GlassMaterial:
    case AppleVisualEffect::GlassClearMaterial:
    case AppleVisualEffect::GlassSubduedMaterial:
    case AppleVisualEffect::GlassMediaControlsMaterial:
    case AppleVisualEffect::GlassSubduedMediaControlsMaterial:
#endif
    case AppleVisualEffect::VibrancyLabel:
    case AppleVisualEffect::VibrancySecondaryLabel:
    case AppleVisualEffect::VibrancyTertiaryLabel:
    case AppleVisualEffect::VibrancyQuaternaryLabel:
    case AppleVisualEffect::VibrancyFill:
    case AppleVisualEffect::VibrancySecondaryFill:
    case AppleVisualEffect::VibrancyTertiaryFill:
    case AppleVisualEffect::VibrancySeparator:
        ASSERT_NOT_REACHED();
        return nil;
    }
}

static MTCoreMaterialVisualStyle materialVisualStyleForAppleVisualEffect(AppleVisualEffect effect)
{
    switch (effect) {
    case AppleVisualEffect::VibrancyLabel:
    case AppleVisualEffect::VibrancyFill:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStylePrimarySingleton();
    case AppleVisualEffect::VibrancySecondaryLabel:
    case AppleVisualEffect::VibrancySecondaryFill:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleSecondarySingleton();
    case AppleVisualEffect::VibrancyTertiaryLabel:
    case AppleVisualEffect::VibrancyTertiaryFill:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleTertiarySingleton();
    case AppleVisualEffect::VibrancyQuaternaryLabel:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleQuaternarySingleton();
    case AppleVisualEffect::VibrancySeparator:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleSeparatorSingleton();
    case AppleVisualEffect::None:
    case AppleVisualEffect::BlurUltraThinMaterial:
    case AppleVisualEffect::BlurThinMaterial:
    case AppleVisualEffect::BlurMaterial:
    case AppleVisualEffect::BlurThickMaterial:
    case AppleVisualEffect::BlurChromeMaterial:
#if HAVE(MATERIAL_HOSTING)
    case AppleVisualEffect::GlassMaterial:
    case AppleVisualEffect::GlassClearMaterial:
    case AppleVisualEffect::GlassSubduedMaterial:
    case AppleVisualEffect::GlassMediaControlsMaterial:
    case AppleVisualEffect::GlassSubduedMediaControlsMaterial:
#endif
        ASSERT_NOT_REACHED();
        return nil;
    }
}

static MTCoreMaterialVisualStyleCategory materialVisualStyleCategoryForAppleVisualEffect(AppleVisualEffect effect)
{
    switch (effect) {
    case AppleVisualEffect::VibrancyLabel:
    case AppleVisualEffect::VibrancySecondaryLabel:
    case AppleVisualEffect::VibrancyTertiaryLabel:
    case AppleVisualEffect::VibrancyQuaternaryLabel:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleCategoryStrokeSingleton();
    case AppleVisualEffect::VibrancyFill:
    case AppleVisualEffect::VibrancySecondaryFill:
    case AppleVisualEffect::VibrancyTertiaryFill:
    case AppleVisualEffect::VibrancySeparator:
        return PAL::get_CoreMaterial_MTCoreMaterialVisualStyleCategoryFillSingleton();
    case AppleVisualEffect::None:
    case AppleVisualEffect::BlurUltraThinMaterial:
    case AppleVisualEffect::BlurThinMaterial:
    case AppleVisualEffect::BlurMaterial:
    case AppleVisualEffect::BlurThickMaterial:
    case AppleVisualEffect::BlurChromeMaterial:
#if HAVE(MATERIAL_HOSTING)
    case AppleVisualEffect::GlassMaterial:
    case AppleVisualEffect::GlassClearMaterial:
    case AppleVisualEffect::GlassSubduedMaterial:
    case AppleVisualEffect::GlassMediaControlsMaterial:
    case AppleVisualEffect::GlassSubduedMediaControlsMaterial:
#endif
        ASSERT_NOT_REACHED();
        return nil;
    }
}

#if HAVE(MATERIAL_HOSTING)

static WKHostedMaterialEffectType hostedMaterialEffectTypeForAppleVisualEffect(AppleVisualEffect effect)
{
    switch (effect) {
    case AppleVisualEffect::GlassMaterial:
        return WKHostedMaterialEffectTypeGlass;
    case AppleVisualEffect::GlassClearMaterial:
        return WKHostedMaterialEffectTypeClearGlass;
    case AppleVisualEffect::GlassSubduedMaterial:
        return WKHostedMaterialEffectTypeSubduedGlass;
    case AppleVisualEffect::GlassMediaControlsMaterial:
        return WKHostedMaterialEffectTypeMediaControlsGlass;
    case AppleVisualEffect::GlassSubduedMediaControlsMaterial:
        return WKHostedMaterialEffectTypeSubduedMediaControlsGlass;
    case AppleVisualEffect::None:
    case AppleVisualEffect::BlurUltraThinMaterial:
    case AppleVisualEffect::BlurThinMaterial:
    case AppleVisualEffect::BlurMaterial:
    case AppleVisualEffect::BlurThickMaterial:
    case AppleVisualEffect::BlurChromeMaterial:
    case AppleVisualEffect::VibrancyLabel:
    case AppleVisualEffect::VibrancySecondaryLabel:
    case AppleVisualEffect::VibrancyTertiaryLabel:
    case AppleVisualEffect::VibrancyQuaternaryLabel:
    case AppleVisualEffect::VibrancyFill:
    case AppleVisualEffect::VibrancySecondaryFill:
    case AppleVisualEffect::VibrancyTertiaryFill:
    case AppleVisualEffect::VibrancySeparator:
        ASSERT_NOT_REACHED();
        return WKHostedMaterialEffectTypeNone;
    }
}

static WKHostedMaterialColorScheme hostedMaterialColorSchemeForAppleVisualEffectData(const AppleVisualEffectData& data)
{
    switch (data.colorScheme) {
    case AppleVisualEffectData::ColorScheme::Light:
        return WKHostedMaterialColorSchemeLight;
    case AppleVisualEffectData::ColorScheme::Dark:
        return WKHostedMaterialColorSchemeDark;
    }
}

#endif

static void applyVisualStylingToLayer(CALayer *layer, const AppleVisualEffectData& effectData)
{
    RetainPtr recipe = materialRecipeForAppleVisualEffect(effectData.contextEffect, effectData.colorScheme);
    if ([recipe isEqualToString:PAL::get_CoreMaterial_MTCoreMaterialRecipeNoneSingleton()]) {
        bool isDark = effectData.colorScheme == AppleVisualEffectData::ColorScheme::Dark;
#if PLATFORM(VISION)
        recipe = isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentUltraThinDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentUltraThinLightSingleton();
#else
        recipe = isDark ? PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThickDarkSingleton() : PAL::get_CoreMaterial_MTCoreMaterialRecipePlatformContentThickLightSingleton();
#endif
    }

    // Despite the name, MTVisualStylingCreateDictionaryRepresentation returns an autoreleased object.
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr visualStylingDescription = PAL::softLink_CoreMaterial_MTVisualStylingCreateDictionaryRepresentation(recipe.get(), materialVisualStyleCategoryForAppleVisualEffect(effectData.effect), materialVisualStyleForAppleVisualEffect(effectData.effect), nil);

    RetainPtr<NSArray<NSDictionary<NSString *, id> *>> filterDescriptionsArray = [visualStylingDescription objectForKey:@"filters"];
    RetainPtr filterDescription = [filterDescriptionsArray firstObject];

    NSArray *filters = nil;
    if (filterDescription) {
        RetainPtr<NSValue> colorMatrix = [filterDescription objectForKey:kCAFilterInputColorMatrix];
        RetainPtr filter = [CAFilter filterWithType:kCAFilterVibrantColorMatrix];
        [filter setValue:colorMatrix.get() forKey:kCAFilterInputColorMatrix];
        filters = @[ filter.get() ];
    }
    [layer setFilters:filters];
}

static void updateAppleVisualEffect(CALayer *layer, RemoteLayerTreeNode* layerTreeNode, AppleVisualEffectData effectData)
{
    if (RetainPtr materialLayer = dynamic_objc_cast<MTMaterialLayer>(layer)) {
        if (RetainPtr recipe = materialRecipeForAppleVisualEffect(effectData.effect, effectData.colorScheme)) {
            [materialLayer setRecipe:recipe.get()];
            return;
        }
    }

    if (appleVisualEffectAppliesFilter(effectData.effect)) {
        applyVisualStylingToLayer(layer, effectData);
        return;
    }

#if HAVE(MATERIAL_HOSTING)
    if (appleVisualEffectIsHostedMaterial(effectData.effect)) {
        CGFloat cornerRadius = 0;
        if (auto borderRect = effectData.borderRect) {
            // FIXME: Support more complex shapes and non-uniform corner radius.
            cornerRadius = std::max({
                borderRect->radii().topLeft().maxDimension(),
                borderRect->radii().topRight().maxDimension(),
                borderRect->radii().bottomLeft().maxDimension(),
                borderRect->radii().bottomRight().maxDimension()
            });
        }

#if PLATFORM(IOS_FAMILY)
        if (layerTreeNode) {
            if (RetainPtr materialHostingView = dynamic_objc_cast<WKMaterialHostingView>(layerTreeNode->uiView()))
                [materialHostingView updateMaterialEffectType:hostedMaterialEffectTypeForAppleVisualEffect(effectData.effect) colorScheme:hostedMaterialColorSchemeForAppleVisualEffectData(effectData) cornerRadius:cornerRadius];
        }
#endif

        [WKMaterialHostingSupport updateHostingLayer:layer materialEffectType:hostedMaterialEffectTypeForAppleVisualEffect(effectData.effect) colorScheme:hostedMaterialColorSchemeForAppleVisualEffectData(effectData) cornerRadius:cornerRadius];
    }
#endif // HAVE(MATERIAL_HOSTING)
}

#endif

static void updateCustomAppearance(CALayer *layer, GraphicsLayerCustomAppearance customAppearance)
{
#if HAVE(RUBBER_BANDING)
    switch (customAppearance) {
    case GraphicsLayerCustomAppearance::None:
        ScrollbarThemeMac::removeOverhangAreaShadow(layer);
        break;
    case GraphicsLayerCustomAppearance::ScrollingShadow:
        ScrollbarThemeMac::setUpOverhangAreaShadow(layer);
        break;
    }
#else
    UNUSED_PARAM(customAppearance);
#endif
}

void RemoteLayerTreePropertyApplier::applyPropertiesToLayer(CALayer *layer, RemoteLayerTreeNode* layerTreeNode, RemoteLayerTreeHost* layerTreeHost, const LayerProperties& properties)
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

    if (properties.changedProperties & LayerChange::OpacityChanged)
        layer.opacity = properties.opacity;

    if (properties.changedProperties & LayerChange::MasksToBoundsChanged)
        layer.masksToBounds = properties.masksToBounds;

    if (properties.changedProperties & LayerChange::NameChanged)
        layer.name = properties.name.createNSString().get();

    if (properties.changedProperties & LayerChange::BackgroundColorChanged)
        layer.backgroundColor = cgColorFromColor(properties.backgroundColor).get();

    if (properties.changedProperties & LayerChange::BorderColorChanged)
        layer.borderColor = cgColorFromColor(properties.borderColor).get();

    if (properties.changedProperties & LayerChange::BorderWidthChanged)
        layer.borderWidth = properties.borderWidth;

    if (properties.changedProperties & LayerChange::DoubleSidedChanged)
        layer.doubleSided = properties.doubleSided;

    if (properties.changedProperties & LayerChange::ContentsRectChanged)
        layer.contentsRect = properties.contentsRect;

    if (properties.changedProperties & LayerChange::CornerRadiusChanged) {
        layer.cornerRadius = properties.cornerRadius;
        if (properties.cornerRadius)
            layer.cornerCurve = kCACornerCurveCircular;
    }

    if (properties.changedProperties & LayerChange::ShapeRoundedRectChanged) {
        Path path;
        if (properties.shapeRoundedRect)
            path.addRoundedRect(*properties.shapeRoundedRect);
        dynamic_objc_cast<CAShapeLayer>(layer).path = path.protectedPlatformPath().get();
    }

    if (properties.changedProperties & LayerChange::ShapePathChanged)
        dynamic_objc_cast<CAShapeLayer>(layer).path = properties.shapePath.protectedPlatformPath().get();

    if (properties.changedProperties & LayerChange::MinificationFilterChanged)
        layer.minificationFilter = toCAFilterType(properties.minificationFilter).get();

    if (properties.changedProperties & LayerChange::MagnificationFilterChanged)
        layer.magnificationFilter = toCAFilterType(properties.magnificationFilter).get();

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
        auto* backingStore = properties.backingStoreOrProperties.properties.get();
        if (backingStore && properties.backingStoreAttached) {
            RELEASE_ASSERT(layerTreeNode);
            layerTreeNode->applyBackingStore(layerTreeHost, *backingStore);
        } else
            [layer _web_clearContents];
    }

    if (properties.changedProperties & LayerChange::BackdropRootIsOpaqueChanged && layerTreeNode)
        layerTreeNode->setBackdropRootIsOpaque(properties.backdropRootIsOpaque);

    if (properties.changedProperties & LayerChange::FiltersChanged)
        PlatformCAFilters::setFiltersOnLayer(layer, properties.filters ? *properties.filters : FilterOperations(), layerTreeNode && layerTreeNode->backdropRootIsOpaque());

    if (properties.changedProperties & LayerChange::AnimationsChanged) {
#if ENABLE(THREADED_ANIMATIONS)
        if (layerTreeHost->threadedAnimationsEnabled()) {
            LOG_WITH_STREAM(Animations, stream << "RemoteLayerTreePropertyApplier::applyProperties() for layer " << layerTreeNode->layerID() << " found " << properties.animationChanges.effects.size() << " effects.");
            layerTreeNode->setAcceleratedEffectsAndBaseValues(properties.animationChanges.effects, properties.animationChanges.baseValues, *layerTreeHost);
        }
#endif
        PlatformCAAnimationRemote::updateLayerAnimations(layer, layerTreeHost, properties.animationChanges.addedAnimations, properties.animationChanges.keysOfAnimationsToRemove);
    }

    if (properties.changedProperties & LayerChange::AntialiasesEdgesChanged)
        layer.edgeAntialiasingMask = properties.antialiasesEdges ? (kCALayerLeftEdge | kCALayerRightEdge | kCALayerBottomEdge | kCALayerTopEdge) : 0;

    if (properties.changedProperties & LayerChange::CustomAppearanceChanged)
        updateCustomAppearance(layer, properties.customAppearance);

    if (properties.changedProperties & LayerChange::BackdropRootChanged)
        layer.shouldRasterize = properties.backdropRoot;

    if (properties.changedProperties & LayerChange::TonemappingEnabledChanged) {
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
        layer.toneMapMode = properties.tonemappingEnabled ? CAToneMapModeIfSupported : CAToneMapModeNever;
#endif
    }

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    if (properties.changedProperties & LayerChange::SeparatedPortalChanged) {
        // FIXME: Implement SeparatedPortalChanged.
    }

    if (properties.changedProperties & LayerChange::DescendentOfSeparatedPortalChanged) {
        // FIXME: Implement DescendentOfSeparatedPortalChanged.
    }
#endif

#if HAVE(AVKIT)
    if (properties.changedProperties & LayerChange::VideoGravityChanged) {
        auto *playerLayer = layer;
#if PLATFORM(IOS_FAMILY)
        if (layerTreeNode && [layerTreeNode->uiView() isKindOfClass:WKVideoView.class])
            playerLayer = [(WKVideoView*)layerTreeNode->uiView() playerLayer];
#endif
        ASSERT([playerLayer respondsToSelector:@selector(setVideoGravity:)]);
        if (RetainPtr webAVPlayerLayer = dynamic_objc_cast<WebAVPlayerLayer>(playerLayer))
            [webAVPlayerLayer setVideoGravity:convertMediaPlayerToAVLayerVideoGravity(properties.videoGravity).get()];
    }
#endif

#if HAVE(CORE_MATERIAL)
    if (properties.changedProperties & LayerChange::AppleVisualEffectChanged)
        updateAppleVisualEffect(layer, layerTreeNode, properties.appleVisualEffectData);
#endif
}

void RemoteLayerTreePropertyApplier::applyProperties(RemoteLayerTreeNode& node, RemoteLayerTreeHost* layerTreeHost, const LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    applyPropertiesToLayer(node.protectedLayer().get(), &node, layerTreeHost, properties);
    if (properties.changedProperties & LayerChange::EventRegionChanged)
        node.setEventRegion(properties.eventRegion);
    updateMask(node, properties, relatedLayers);

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS) || HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (properties.changedProperties & LayerChange::VisibleRectChanged)
        node.setVisibleRect(properties.visibleRect);
#endif

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
    if (properties.changedProperties & LayerChange::EventRegionChanged || properties.changedProperties & LayerChange::VisibleRectChanged)
        updateLayersForInteractionRegions(node);
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (properties.changedProperties & LayerChange::SeparatedChanged)
        node.setShouldBeSeparated(properties.isSeparated);

    if (properties.changedProperties & LayerChange::SeparatedChanged || properties.changedProperties & LayerChange::VisibleRectChanged) {
        if (node.visibleRect() && node.shouldBeSeparated()) {
            node.layer().separated = true;
            configureSeparatedLayer(node.layer());
        } else if (node.layer().isSeparated)
            node.layer().separated = false;
    }
#endif

#if ENABLE(SCROLLING_THREAD)
    if (properties.changedProperties & LayerChange::ScrollingNodeIDChanged)
        node.setScrollingNodeID(properties.scrollingNodeID);
#endif

#if PLATFORM(IOS_FAMILY)
    applyPropertiesToUIView(node.uiView(), properties, relatedLayers);
#endif

    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreePropertyApplier::applyHierarchyUpdates(RemoteLayerTreeNode& node, const LayerProperties& properties, const RelatedLayerMap& relatedLayers)
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

        RetainPtr view = node.uiView();
#if HAVE(MATERIAL_HOSTING)
        if (RetainPtr materialHostingView = dynamic_objc_cast<WKMaterialHostingView>(view.get()))
            view = [materialHostingView contentView];
#endif

        [view _web_setSubviews:createNSArray(properties.children, [&] (auto& child) -> UIView * {
            auto* childNode = relatedLayers.get(child);
            ASSERT(childNode);
            if (!childNode)
                return nil;
            ASSERT(childNode->uiView());
            return childNode->uiView();
        }).get()];
#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
        node.updateInteractionRegionAfterHierarchyChange();
#endif
        return;
    }
#endif

    RetainPtr layer = node.layer();
#if HAVE(MATERIAL_HOSTING)
    if (RetainPtr contentLayer = [WKMaterialHostingSupport contentLayerForMaterialHostingLayer:layer.get()])
        layer = contentLayer;
#endif

    [layer setSublayers:createNSArray(properties.children, [&] (auto& child) -> CALayer * {
        auto* childNode = relatedLayers.get(child);
        ASSERT(childNode);
        if (!childNode)
            return nil;
#if PLATFORM(IOS_FAMILY)
        ASSERT(!childNode->uiView());
#endif
        return childNode->layer();
    }).get()];

    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreePropertyApplier::updateMask(RemoteLayerTreeNode& node, const LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (!properties.changedProperties.contains(LayerChange::MaskLayerChanged))
        return;

    RetainPtr maskOwnerLayer = node.layer();

    if (!properties.maskLayerID) {
        maskOwnerLayer.get().mask = nullptr;
        return;
    }

    RefPtr maskNode = properties.maskLayerID ? relatedLayers.get(*properties.maskLayerID) : nullptr;
    ASSERT(maskNode);
    if (!maskNode)
        return;

    RetainPtr maskLayer = maskNode->layer();
    [maskLayer removeFromSuperlayer];

    maskOwnerLayer.get().mask = maskLayer.get();
}

#if PLATFORM(IOS_FAMILY)
void RemoteLayerTreePropertyApplier::applyPropertiesToUIView(UIView *view, const LayerProperties& properties, const RelatedLayerMap& relatedLayers)
{
    if (properties.changedProperties.containsAny({ LayerChange::ContentsHiddenChanged, LayerChange::UserInteractionEnabledChanged }))
        view.userInteractionEnabled = !properties.contentsHidden && properties.userInteractionEnabled;

#if HAVE(MATERIAL_HOSTING)
    if (properties.changedProperties & LayerChange::BoundsChanged) {
        if (RetainPtr materialHostingView = dynamic_objc_cast<WKMaterialHostingView>(view))
            [materialHostingView updateHostingSize:properties.bounds.size()];
    }
#endif
}
#endif

} // namespace WebKit
