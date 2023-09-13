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

#if PLATFORM(VISION)

#import <RealitySystemSupport/RealitySystemSupport.h>
#import <wtf/SoftLinking.h>
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(RealitySystemSupport)
SOFT_LINK_CLASS_OPTIONAL(RealitySystemSupport, RCPGlowEffectLayer)
SOFT_LINK_CONSTANT_MAY_FAIL(RealitySystemSupport, RCPAllowedInputTypesUserInfoKey, const NSString *)

#endif

#import <WebCore/WebActionDisablingCALayerDelegate.h>

namespace WebKit {
using namespace WebCore;

NSString *interactionRegionTypeKey = @"WKInteractionRegionType";
NSString *interactionRegionGroupNameKey = @"WKInteractionRegionGroupName";

#if PLATFORM(VISION)
RCPRemoteEffectInputTypes interactionRegionInputTypes = RCPRemoteEffectInputTypesAll ^ RCPRemoteEffectInputTypePointer;

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
        if (canLoadRCPAllowedInputTypesUserInfoKey())
            interactionRegionEffectUserInfo.get() = @{ getRCPAllowedInputTypesUserInfoKey(): @(interactionRegionInputTypes) };
    });
    return interactionRegionEffectUserInfo.get().get();
}

static float brightnessMultiplier()
{
    static float multiplier = 1.5;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (auto brightnessUserDefault = [[NSUserDefaults standardUserDefaults] floatForKey:@"WKInteractionRegionBrightnessMultiplier"])
            multiplier = brightnessUserDefault;
    });
    return multiplier;
}

static void configureLayerForInteractionRegion(CALayer *layer, NSString *groupName)
{
    if (![layer isKindOfClass:getRCPGlowEffectLayerClass()])
        return;

    [(RCPGlowEffectLayer *)layer setBrightnessMultiplier:brightnessMultiplier() forInputTypes:interactionRegionInputTypes];
    [(RCPGlowEffectLayer *)layer setEffectGroupConfigurator:^void(CARemoteEffectGroup *group)
    {
        group.groupName = groupName;
        group.matched = YES;
        group.userInfo = interactionRegionEffectUserInfo();
    }];
}

static void configureLayerAsGuard(CALayer *layer, NSString *groupName)
{
    CARemoteEffectGroup *group = [CARemoteEffectGroup groupWithEffects:@[]];
    group.groupName = groupName;
    group.matched = YES;
    group.userInfo = interactionRegionEffectUserInfo();
    layer.remoteEffects = @[ group ];
}
#else
static Class interactionRegionLayerClass() { return [CALayer class]; }
static void configureLayerForInteractionRegion(CALayer *, NSString *) { }
static void configureLayerAsGuard(CALayer *, NSString *) { }
#endif // !PLATFORM(VISION)

static NSString *interactionRegionGroupNameForRegion(const WebCore::PlatformLayerIdentifier& layerID, const WebCore::InteractionRegion& interactionRegion)
{
    return makeString("WKInteractionRegion-"_s, layerID.toString(), interactionRegion.elementIdentifier.toUInt64());
}

static void configureRemoteEffect(CALayer *layer, WebCore::InteractionRegion::Type type, NSString *groupName)
{
    switch (type) {
    case InteractionRegion::Type::Interaction:
        configureLayerForInteractionRegion(layer, groupName);
        break;
    case InteractionRegion::Type::Guard:
        configureLayerAsGuard(layer, groupName);
        break;
    case InteractionRegion::Type::Occlusion:
        break;
    }
}

static void applyBackgroundColorForDebuggingToLayer(CALayer *layer, WebCore::InteractionRegion::Type type)
{
    switch (type) {
    case InteractionRegion::Type::Interaction:
        [layer setBackgroundColor:cachedCGColor({ WebCore::SRGBA<float>(0, 1, 0, .2) }).get()];
        [layer setName:@"Interaction"];
        break;
    case InteractionRegion::Type::Guard:
        [layer setBorderColor:cachedCGColor({ WebCore::SRGBA<float>(0, 0, 1, .2) }).get()];
        [layer setBorderWidth:6];
        [layer setName:@"Guard"];
        break;
    case InteractionRegion::Type::Occlusion:
        [layer setBorderColor:cachedCGColor({ WebCore::SRGBA<float>(1, 0, 0, .2) }).get()];
        [layer setBorderWidth:6];
        [layer setName:@"Occlusion"];
        break;
    }
}

static CALayer *createInteractionRegionLayer(WebCore::InteractionRegion::Type type, NSString *groupName, bool applyBackgroundColorForDebugging)
{
    CALayer *layer = type == InteractionRegion::Type::Interaction
        ? [[interactionRegionLayerClass() alloc] init]
        : [[CALayer alloc] init];

    [layer setHitTestsAsOpaque:YES];
    [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    [layer setValue:@(static_cast<uint8_t>(type)) forKey:interactionRegionTypeKey];
    [layer setValue:groupName forKey:interactionRegionGroupNameKey];

    configureRemoteEffect(layer, type, groupName);

    if (applyBackgroundColorForDebugging)
        applyBackgroundColorForDebuggingToLayer(layer, type);

    return layer;
}

static std::optional<WebCore::InteractionRegion::Type> interactionRegionTypeForLayer(CALayer *layer)
{
    id value = [layer valueForKey:interactionRegionTypeKey];
    if (value)
        return static_cast<InteractionRegion::Type>([value boolValue]);
    return std::nullopt;
}

static NSString *interactionRegionGroupNameForLayer(CALayer *layer)
{
    return [layer valueForKey:interactionRegionGroupNameKey];
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

void updateLayersForInteractionRegions(RemoteLayerTreeNode& node)
{
    if (node.eventRegion().interactionRegions().isEmpty()) {
        node.removeInteractionRegionsContainer();
        return;
    }

    CALayer *container = node.ensureInteractionRegionsContainer();

    HashMap<std::pair<IntRect, InteractionRegion::Type>, CALayer *>existingLayers;
    HashMap<std::pair<String, InteractionRegion::Type>, CALayer *>reusableLayers;
    for (CALayer *sublayer in container.sublayers) {
        if (auto type = interactionRegionTypeForLayer(sublayer)) {
            auto result = existingLayers.add(std::make_pair(enclosingIntRect(sublayer.frame), *type), sublayer);
            ASSERT_UNUSED(result, result.isNewEntry);

            auto reuseKey = std::make_pair(interactionRegionGroupNameForLayer(sublayer), *type);
            if (reusableLayers.contains(reuseKey))
                reusableLayers.remove(reuseKey);
            else {
                auto result = reusableLayers.add(reuseKey, sublayer);
                ASSERT_UNUSED(result, result.isNewEntry);
            }
        }
    }

    bool applyBackgroundColorForDebugging = [[NSUserDefaults standardUserDefaults] boolForKey:@"WKInteractionRegionDebugFill"];

    NSUInteger insertionPoint = 0;
    HashSet<std::pair<IntRect, InteractionRegion::Type>>dedupeSet;
    for (const WebCore::InteractionRegion& region : node.eventRegion().interactionRegions()) {
        IntRect rect = region.rectInLayerCoordinates;
        if (!node.visibleRect() || !node.visibleRect()->intersects(rect))
            continue;

        auto interactionRegionGroupName = interactionRegionGroupNameForRegion(node.layerID(), region);
        auto key = std::make_pair(rect, region.type);
        if (dedupeSet.contains(key))
            continue;
        auto reuseKey = std::make_pair(interactionRegionGroupName, region.type);

        RetainPtr<CALayer> regionLayer;
        bool didReuseLayer = true;
        bool didReuseLayerBasedOnRect = false;

        auto findOrCreateLayer = [&]() {
            auto layerIterator = existingLayers.find(key);
            if (layerIterator != existingLayers.end()) {
                didReuseLayerBasedOnRect = true;
                regionLayer = layerIterator->value;
                return;
            }

            auto layerReuseIterator = reusableLayers.find(reuseKey);
            if (layerReuseIterator != reusableLayers.end()) {
                regionLayer = layerReuseIterator->value;
                return;
            }

            didReuseLayer = false;
            regionLayer = adoptNS(createInteractionRegionLayer(region.type, interactionRegionGroupName, applyBackgroundColorForDebugging));
        };
        findOrCreateLayer();

        if (didReuseLayer) {
            auto layerKey = std::make_pair(enclosingIntRect([regionLayer frame]), region.type);
            existingLayers.remove(layerKey);
            reusableLayers.remove(reuseKey);

            bool shouldReconfigureRemoteEffect = didReuseLayerBasedOnRect && ![interactionRegionGroupName isEqualToString:interactionRegionGroupNameForLayer(regionLayer.get())];
            if (shouldReconfigureRemoteEffect)
                configureRemoteEffect(regionLayer.get(), region.type, interactionRegionGroupName);
        }

        if (!didReuseLayerBasedOnRect)
            [regionLayer setFrame:rect];

        if (region.type == InteractionRegion::Type::Interaction) {
            [regionLayer setCornerRadius:region.borderRadius];
            constexpr CACornerMask allCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
            if (region.maskedCorners.isEmpty())
                [regionLayer setMaskedCorners:allCorners];
            else
                [regionLayer setMaskedCorners:convertToCACornerMask(region.maskedCorners)];
        }

        if ([container.sublayers objectAtIndex:insertionPoint] != regionLayer) {
            [regionLayer removeFromSuperlayer];
            [container insertSublayer:regionLayer.get() atIndex:insertionPoint];
        }

        insertionPoint++;
    }

    for (CALayer *sublayer : existingLayers.values())
        [sublayer removeFromSuperlayer];
}

} // namespace WebKit

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
