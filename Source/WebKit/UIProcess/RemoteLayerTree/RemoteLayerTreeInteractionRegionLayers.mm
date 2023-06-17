/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeInteractionRegionLayers.h"

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeHost.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(VISION)
#import <RealitySystemSupport/RealitySystemSupport.h>
#endif

//// FIXME: rdar://105775731
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(RealitySystemSupport)
SOFT_LINK_CLASS_OPTIONAL(RealitySystemSupport, RCPGlowEffectLayer)
#if PLATFORM(VISION)
SOFT_LINK_CONSTANT_MAY_FAIL(RealitySystemSupport, RCPAllowedInputTypesUserInfoKey, const NSString *)
#endif

@interface CALayer ()
@property (nonatomic) CGFloat sizeMultiplier;
@property (nonatomic, copy) void (^effectGroupConfigurator)(CARemoteEffectGroup *group);
@end

namespace WebKit {
using namespace WebCore;

NSString *interactionRegionTypeKey = @"WKInteractionRegionType";
NSString *interactionRegionGroupNameKey = @"WKInteractionRegionGroupName";

#if PLATFORM(VISION)
static Class interactionRegionLayerClass()
{
    if (getRCPGlowEffectLayerClass())
        return getRCPGlowEffectLayerClass();
    return [CALayer class];
}

static NSDictionary *interactionRegionEffectUserInfo()
{
    static NeverDestroyed<RetainPtr<NSDictionary>> interactionRegionEffectUserInfo;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (canLoadRCPAllowedInputTypesUserInfoKey()) {
            RCPRemoteEffectInputTypes allowedInputs = RCPRemoteEffectInputTypesAll ^ RCPRemoteEffectInputTypePointer;
            interactionRegionEffectUserInfo.get() = @{ getRCPAllowedInputTypesUserInfoKey(): @(allowedInputs) };
        }
    });
    return interactionRegionEffectUserInfo.get().get();
}

static void configureLayerForInteractionRegion(CALayer *layer, NSString *groupName)
{
    if (auto multiplier = [[NSUserDefaults standardUserDefaults] floatForKey:@"WKInteractionRegionLayerSizeMultiplier"]) {
        if ([layer respondsToSelector:@selector(setSizeMultiplier:)])
            [layer setSizeMultiplier:multiplier];
    }

    if ([layer respondsToSelector:@selector(setEffectGroupConfigurator:)]) {
        [layer setEffectGroupConfigurator:^void(CARemoteEffectGroup *group)
        {
            group.groupName = groupName;
            group.matched = YES;
            group.userInfo = interactionRegionEffectUserInfo();
        }];
    }
}
#else
static Class interactionRegionLayerClass() { return [CALayer class]; }
static void configureLayerForInteractionRegion(CALayer *, NSString *) { }
static NSDictionary *interactionRegionEffectUserInfo() { return @{ }; }
#endif // !PLATFORM(VISION)

static void configureLayerAsGuard(CALayer *layer, NSString *groupName)
{
    CARemoteEffectGroup *group = [CARemoteEffectGroup groupWithEffects:@[]];
    group.groupName = groupName;
    group.matched = YES;
    group.userInfo = interactionRegionEffectUserInfo();
    layer.remoteEffects = @[ group ];
}

static std::optional<WebCore::InteractionRegion::Type> interactionRegionTypeForLayer(CALayer *layer)
{
    id value = [layer valueForKey:interactionRegionTypeKey];
    if (value)
        return static_cast<InteractionRegion::Type>([value boolValue]);
    return std::nullopt;
}

static NSString * interactionRegionGroupNameForLayer(CALayer *layer)
{
    return [layer valueForKey:interactionRegionGroupNameKey];
}

static NSString* interactionRegionGroupNameForRegion(const WebCore::InteractionRegion& interactionRegion)
{
    return makeString("WKInteractionRegion-"_s, interactionRegion.elementIdentifier.toUInt64());
}

static bool isAnyInteractionRegionLayer(CALayer *layer)
{
    return !!interactionRegionTypeForLayer(layer);
}

static void setInteractionRegion(CALayer *layer, NSString *groupName)
{
    [layer setValue:@(static_cast<uint8_t>(InteractionRegion::Type::Interaction)) forKey:interactionRegionTypeKey];
    [layer setValue:groupName forKey:interactionRegionGroupNameKey];
}

static void setInteractionRegionOcclusion(CALayer *layer)
{
    [layer setValue:@(static_cast<uint8_t>(InteractionRegion::Type::Occlusion)) forKey:interactionRegionTypeKey];
}

static void setInteractionRegionGuard(CALayer *layer, NSString *groupName)
{
    [layer setValue:@(static_cast<uint8_t>(InteractionRegion::Type::Guard)) forKey:interactionRegionTypeKey];
    [layer setValue:groupName forKey:interactionRegionGroupNameKey];
}

static CACornerMask convertToCACornerMask(OptionSet<InteractionRegion::CornerMask> mask)
{
    CACornerMask cornerMask = 0;

    if (mask.contains(InteractionRegion::CornerMask::MinXMinYCorner))
        cornerMask |= kCALayerMinXMinYCorner;
    if (mask.contains(InteractionRegion::CornerMask::MaxXMinYCorner))
        cornerMask |= kCALayerMaxXMinYCorner;
    if (mask.contains(InteractionRegion::CornerMask::MinXMaxYCorner))
        cornerMask |= kCALayerMinXMaxYCorner;
    if (mask.contains(InteractionRegion::CornerMask::MaxXMaxYCorner))
        cornerMask |= kCALayerMaxXMaxYCorner;

    return cornerMask;
}

void insertInteractionRegionLayersForLayer(NSMutableArray *sublayers, CALayer *layer)
{
    NSUInteger insertionPoint = 0;
    for (CALayer *sublayer in layer.sublayers) {
        if (!isAnyInteractionRegionLayer(sublayer))
            break;

        [sublayers insertObject:sublayer atIndex:insertionPoint];
        insertionPoint++;
    }
}

void updateLayersForInteractionRegions(CALayer *layer, RemoteLayerTreeHost& host, const RemoteLayerTreeTransaction::LayerProperties& properties)
{
    ASSERT(properties.changedProperties & LayerChange::EventRegionChanged);

    HashMap<std::pair<IntRect, InteractionRegion::Type>, CALayer *>existingLayers;
    for (CALayer *sublayer in layer.sublayers) {
        if (auto type = interactionRegionTypeForLayer(sublayer)) {
            auto result = existingLayers.add(std::make_pair(enclosingIntRect(sublayer.frame), *type), sublayer);
            ASSERT_UNUSED(result, result.isNewEntry);
        }
    }

    bool applyBackgroundColorForDebugging = [[NSUserDefaults standardUserDefaults] boolForKey:@"WKInteractionRegionDebugFill"];

    NSUInteger insertionPoint = 0;
    for (const WebCore::InteractionRegion& region : properties.eventRegion.interactionRegions()) {
        bool foundInPosition = false;
        RetainPtr<CALayer> regionLayer;
        IntRect rect = region.rectInLayerCoordinates;
        auto key = std::make_pair(rect, region.type);
        auto interactionRegionGroupName = interactionRegionGroupNameForRegion(region);

        auto layerIterator = existingLayers.find(key);
        if (layerIterator != existingLayers.end()) {
            regionLayer = layerIterator->value;
            existingLayers.remove(key);
            if ([layer.sublayers objectAtIndex:insertionPoint] == regionLayer)
                foundInPosition = true;
            else
                [regionLayer removeFromSuperlayer];
        } else {
            if (region.type == InteractionRegion::Type::Interaction)
                regionLayer = adoptNS([[interactionRegionLayerClass() alloc] init]);
            else
                regionLayer = adoptNS([[CALayer alloc] init]);

            [regionLayer setFrame:rect];
            [regionLayer setHitTestsAsOpaque:YES];

            switch (region.type) {
            case InteractionRegion::Type::Occlusion:
                setInteractionRegionOcclusion(regionLayer.get());
                if (applyBackgroundColorForDebugging) {
                    [regionLayer setBorderColor:cachedCGColor({ WebCore::SRGBA<float>(1, 0, 0, .2) }).get()];
                    [regionLayer setBorderWidth:6];
                    [regionLayer setName:@"Occlusion"];
                }
                break;
            case InteractionRegion::Type::Guard:
                setInteractionRegionGuard(regionLayer.get(), interactionRegionGroupName);
                configureLayerAsGuard(regionLayer.get(), interactionRegionGroupName);
                if (applyBackgroundColorForDebugging) {
                    [regionLayer setBorderColor:cachedCGColor({ WebCore::SRGBA<float>(0, 0, 1, .2) }).get()];
                    [regionLayer setBorderWidth:6];
                    [regionLayer setName:@"Guard"];
                }
                break;
            case InteractionRegion::Type::Interaction:
                setInteractionRegion(regionLayer.get(), interactionRegionGroupName);
                configureLayerForInteractionRegion(regionLayer.get(), interactionRegionGroupName);
                if (applyBackgroundColorForDebugging) {
                    [regionLayer setBackgroundColor:cachedCGColor({ WebCore::SRGBA<float>(0, 1, 0, .2) }).get()];
                    [regionLayer setName:@"Interaction"];
                }
                break;
            }
            [layer insertSublayer:regionLayer.get() atIndex:insertionPoint];
        }

        if (!foundInPosition)
            [layer insertSublayer:regionLayer.get() atIndex:insertionPoint];

        if (![interactionRegionGroupName isEqualToString:interactionRegionGroupNameForLayer(regionLayer.get())]) {
            if (region.type == InteractionRegion::Type::Guard)
                configureLayerAsGuard(regionLayer.get(), interactionRegionGroupName);
            if (region.type == InteractionRegion::Type::Interaction)
                configureLayerForInteractionRegion(regionLayer.get(), interactionRegionGroupName);
        }

        if (region.type == InteractionRegion::Type::Interaction) {
            [regionLayer setCornerRadius:region.borderRadius];
            if (!region.maskedCorners.isEmpty())
                [regionLayer setMaskedCorners:convertToCACornerMask(region.maskedCorners)];
        }

        insertionPoint++;
    }

    for (CALayer *sublayer : existingLayers.values())
        [sublayer removeFromSuperlayer];
}

} // namespace WebKit

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
