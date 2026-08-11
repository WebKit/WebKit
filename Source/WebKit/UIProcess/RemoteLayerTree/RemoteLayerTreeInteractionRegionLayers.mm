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

#if ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)

#import "PlatformCALayerRemote.h"
#import "RealitySystemSupportSPI.h"
#import "RemoteLayerTreeHost.h"
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/MakeString.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(RealitySystemSupport)
SOFT_LINK_CLASS_OPTIONAL(RealitySystemSupport, RCPGlowEffectLayer)
SOFT_LINK_CONSTANT_MAY_FAIL(RealitySystemSupport, RCPAllowedInputTypesUserInfoKey, const NSString *)

namespace WebKit {
using namespace WebCore;

NSString *interactionRegionTypeKey = @"WKInteractionRegionType";
NSString *interactionRegionElementIdentifierKey = @"WKInteractionRegionElementIdentifier";

RCPRemoteEffectInputTypes interactionRegionInputTypes = RCPRemoteEffectInputTypesAll ^ RCPRemoteEffectInputTypePointer;

static Class interactionRegionLayerClassSingleton()
{
    if (getRCPGlowEffectLayerClassSingleton())
        return getRCPGlowEffectLayerClassSingleton();
    return [CALayer class];
}

static NSDictionary *interactionRegionEffectUserInfo()
{
    static NeverDestroyed<RetainPtr<NSDictionary>> interactionRegionEffectUserInfo;
    static bool cached = false;
    if (!cached) {
        if (canLoadRCPAllowedInputTypesUserInfoKey())
            interactionRegionEffectUserInfo.get() = @{ getRCPAllowedInputTypesUserInfoKeySingleton(): @(interactionRegionInputTypes) };
        cached = true;
    }
    return interactionRegionEffectUserInfo.get().get();
}

static float brightnessMultiplier()
{
    static float multiplier = 1.5;
    static bool cached = false;
    if (!cached) {
        if (auto brightnessUserDefault = [[NSUserDefaults standardUserDefaults] floatForKey:@"WKInteractionRegionBrightnessMultiplier"])
            multiplier = brightnessUserDefault;
        cached = true;
    }
    return multiplier;
}

static bool applyBackgroundColorForDebugging()
{
    static bool applyBackground = false;
    static bool cached = false;
    if (!cached) {
        if (auto applyBackgroundUserDefault = [[NSUserDefaults standardUserDefaults] boolForKey:@"WKInteractionRegionDebugFill"])
            applyBackground = applyBackgroundUserDefault;
        cached = true;
    }
    return applyBackground;
}

static void configureLayerForInteractionRegion(CALayer *layer, NSString *groupName)
{
    if (![layer isKindOfClass:getRCPGlowEffectLayerClassSingleton()])
        return;

    [(RCPGlowEffectLayer *)layer setBrightnessMultiplier:brightnessMultiplier() forInputTypes:interactionRegionInputTypes];
    [(RCPGlowEffectLayer *)layer setEffectGroupConfigurator:^void(CARemoteEffectGroup *group)
    {
        group.groupName = groupName;
        group.matched = YES;
        group.userInfo = interactionRegionEffectUserInfo();
    }];
}

static void reconfigureLayerContentHint(CALayer *layer, WebCore::InteractionRegion::ContentHint contentHint)
{
    if (![layer isKindOfClass:getRCPGlowEffectLayerClassSingleton()])
        return;

    if (contentHint == WebCore::InteractionRegion::ContentHint::Photo)
        [(RCPGlowEffectLayer *)layer setContentRenderingHints:RCPGlowEffectContentRenderingHintPhoto];
    else
        [(RCPGlowEffectLayer *)layer setContentRenderingHints:0];
}

static void configureLayerAsGuard(CALayer *layer, NSString *groupName)
{
    CARemoteEffectGroup *group = [CARemoteEffectGroup groupWithEffects:@[]];
    group.groupName = groupName;
    group.matched = YES;
    group.userInfo = interactionRegionEffectUserInfo();
    layer.remoteEffects = @[ group ];
}

static RetainPtr<NSString> interactionRegionGroupNameForRegion(const WebCore::PlatformLayerIdentifier& layerID, const WebCore::InteractionRegion& interactionRegion)
{
    return makeString("WKInteractionRegion-"_s, interactionRegion.nodeIdentifier.toUInt64()).createNSString();
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

static void applyBackgroundColorForDebuggingToLayer(CALayer *layer, const WebCore::InteractionRegion& region)
{
    switch (region.type) {
    case InteractionRegion::Type::Interaction:
        if (region.contentHint == WebCore::InteractionRegion::ContentHint::Photo)
            [layer setBackgroundColor:cachedCGColor({ WebCore::SRGBA<float>(0.5, 0, 0.5, .2) }).get()];
        else
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

static CALayer *createInteractionRegionLayer(WebCore::InteractionRegion::Type type, WebCore::NodeIdentifier identifier, NSString *groupName)
{
    CALayer *layer = type == InteractionRegion::Type::Interaction
        ? [[interactionRegionLayerClassSingleton() alloc] init]
        : [[CALayer alloc] init];

    [layer setHitTestsAsOpaque:YES];
    [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    [layer setValue:@(static_cast<uint8_t>(type)) forKey:interactionRegionTypeKey];
    [layer setValue:@(identifier.toUInt64()) forKey:interactionRegionElementIdentifierKey];

    configureRemoteEffect(layer, type, groupName);

    return layer;
}

static std::optional<WebCore::InteractionRegion::Type> interactionRegionTypeForLayer(CALayer *layer)
{
    id value = [layer valueForKey:interactionRegionTypeKey];
    if (value)
        return static_cast<InteractionRegion::Type>([value intValue]);
    return std::nullopt;
}

static std::optional<UInt64> interactionRegionElementIdentifierForLayer(CALayer *layer)
{
    id value = [layer valueForKey:interactionRegionElementIdentifierKey];
    if (value)
        return [value unsignedLongLongValue];
    return std::nullopt;
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
    ASSERT(node.uiView());

    if (node.eventRegion().interactionRegions().isEmpty() || !node.uiView()) {
        node.removeInteractionRegionsContainer();
        return;
    }

    CALayer *container = node.ensureInteractionRegionsContainer();

    HashMap<std::pair<IntRect, InteractionRegion::Type>, RetainPtr<CALayer>> existingLayers;
    HashMap<std::pair<UInt64, InteractionRegion::Type>, RetainPtr<CALayer>> reusableLayers;
    for (CALayer *sublayer in container.sublayers) {
        if (auto identifier = interactionRegionElementIdentifierForLayer(sublayer)) {
            if (auto type = interactionRegionTypeForLayer(sublayer)) {
                auto result = existingLayers.add(std::make_pair(enclosingIntRect(sublayer.frame), *type), sublayer);
                ASSERT_UNUSED(result, result.isNewEntry);

                auto reuseKey = std::make_pair(*identifier, *type);
                auto reuseResult = reusableLayers.add(reuseKey, sublayer);
                if (!reuseResult.isNewEntry)
                    reusableLayers.remove(reuseResult.iterator);
            }
        }
    }

    bool applyBackground = applyBackgroundColorForDebugging();

    NSUInteger insertionPoint = 0;
    HashSet<std::pair<IntRect, InteractionRegion::Type>>dedupeSet;
    for (const WebCore::InteractionRegion& region : node.eventRegion().interactionRegions()) {
        auto rect = region.rectInLayerCoordinates;
        if (!node.visibleRect() || !node.visibleRect()->intersects(rect))
            continue;

        auto key = std::make_pair(enclosingIntRect(rect), region.type);
        if (dedupeSet.contains(key))
            continue;

        auto reuseKey = std::make_pair(region.nodeIdentifier.toUInt64(), region.type);
        RetainPtr interactionRegionGroupName = interactionRegionGroupNameForRegion(node.layerID(), region);

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
            regionLayer = adoptNS(createInteractionRegionLayer(region.type, region.nodeIdentifier, interactionRegionGroupName.get()));
        };
        findOrCreateLayer();

        if (didReuseLayer) {
            auto layerIdentifier = interactionRegionElementIdentifierForLayer(regionLayer.get());
            auto layerKey = std::make_pair(enclosingIntRect([regionLayer frame]), region.type);
            auto layerReuseKey = std::make_pair(*layerIdentifier, region.type);
            existingLayers.remove(layerKey);
            reusableLayers.remove(layerReuseKey);

            bool shouldReconfigureRemoteEffect = didReuseLayerBasedOnRect && layerIdentifier != region.nodeIdentifier.toUInt64();
            if (shouldReconfigureRemoteEffect)
                configureRemoteEffect(regionLayer.get(), region.type, interactionRegionGroupName.get());
        }

        if (!didReuseLayerBasedOnRect)
            [regionLayer setFrame:rect];

        if (region.type == InteractionRegion::Type::Interaction) {
            [regionLayer setCornerRadius:region.cornerRadius];
            if (region.cornerRadius) {
                if (region.useContinuousCorners)
                    [regionLayer setCornerCurve:kCACornerCurveContinuous];
                else
                    [regionLayer setCornerCurve:kCACornerCurveCircular];
            }
            reconfigureLayerContentHint(regionLayer.get(), region.contentHint);
            constexpr CACornerMask allCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
            if (region.maskedCorners.isEmpty())
                [regionLayer setMaskedCorners:allCorners];
            else
                [regionLayer setMaskedCorners:convertToCACornerMask(region.maskedCorners)];

            if (region.clipPath) {
                RetainPtr<CAShapeLayer> mask = [regionLayer mask];
                if (!mask) {
                    mask = adoptNS([[CAShapeLayer alloc] init]);
                    [regionLayer setMask:mask.get()];
                }

                [mask setFrame:[regionLayer bounds]];
                [mask setPath:region.clipPath->platformPath()];
            } else
                [regionLayer setMask:nil];
        }

        if (applyBackground)
            applyBackgroundColorForDebuggingToLayer(regionLayer.get(), region);

        // Since we insert new layers as we go, insertionPoint is always <= container.sublayers.count.
        ASSERT(insertionPoint <= container.sublayers.count);
        bool shouldAppendLayer = insertionPoint == container.sublayers.count;
        if (shouldAppendLayer || [container.sublayers objectAtIndex:insertionPoint] != regionLayer) {
            [regionLayer removeFromSuperlayer];
            [container insertSublayer:regionLayer.get() atIndex:insertionPoint];
        }

        insertionPoint++;
    }

    for (auto& sublayer : existingLayers.values())
        [sublayer removeFromSuperlayer];
}

} // namespace WebKit

#endif // ENABLE(GAZE_GLOW_FOR_INTERACTION_REGIONS)
