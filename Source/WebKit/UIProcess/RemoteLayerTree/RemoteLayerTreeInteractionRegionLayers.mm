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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/RemoteLayerTreePropertyApplierInteractionRegionAdditions.mm>
#else
static Class interactionRegionLayerClass() { return [CALayer class]; }
static void configureLayerForInteractionRegion(CALayer *, NSString *) { }
#endif

namespace WebKit {
using namespace WebCore;

NSString *interactionRegionTypeKey = @"WKInteractionRegionType";
NSString *interactionRegionGroupNameKey = @"WKInteractionRegionGroupName";

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

static bool isInteractionLayer(CALayer *layer)
{
    return interactionRegionTypeForLayer(layer) == InteractionRegion::Type::Interaction;
}

static bool isOcclusionLayer(CALayer *layer)
{
    return interactionRegionTypeForLayer(layer) == InteractionRegion::Type::Occlusion;
}

static bool isAnyInteractionRegionLayer(CALayer *layer)
{
    return !!interactionRegionTypeForLayer(layer);
}

static void setInteractionRegion(CALayer *layer, NSString *groupName)
{
    [layer setValue:@(static_cast<bool>(InteractionRegion::Type::Interaction)) forKey:interactionRegionTypeKey];
    [layer setValue:groupName forKey:interactionRegionGroupNameKey];
}

static void setInteractionRegionOcclusion(CALayer *layer)
{
    [layer setValue:@(static_cast<bool>(InteractionRegion::Type::Occlusion)) forKey:interactionRegionTypeKey];
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

    HashMap<IntRect, CALayer *> existingOcclusionLayers;
    HashMap<IntRect, CALayer *> existingInteractionLayers;
    for (CALayer *sublayer in layer.sublayers) {
        if (!isAnyInteractionRegionLayer(sublayer))
            break;

        auto enclosingFrame = enclosingIntRect(sublayer.frame);
        auto result = isInteractionLayer(sublayer) ? existingInteractionLayers.add(enclosingFrame, sublayer) : existingOcclusionLayers.add(enclosingFrame, sublayer);
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    bool applyBackgroundColorForDebugging = [[NSUserDefaults standardUserDefaults] boolForKey:@"WKInteractionRegionDebugFill"];

    NSUInteger insertionPoint = 0;
    for (const WebCore::InteractionRegion& region : properties.eventRegion.interactionRegions()) {
        for (IntRect rect : region.regionInLayerCoordinates.rects()) {
            bool foundInPosition = false;
            if (region.type == InteractionRegion::Type::Occlusion) {
                RetainPtr<CALayer> occlusionLayer;

                auto layerIterator = existingOcclusionLayers.find(rect);
                if (layerIterator != existingOcclusionLayers.end()) {
                    occlusionLayer = layerIterator->value;
                    existingOcclusionLayers.remove(layerIterator);
                    if ([layer.sublayers objectAtIndex:insertionPoint] == occlusionLayer)
                        foundInPosition = true;
                    else
                        [occlusionLayer removeFromSuperlayer];
                } else {
                    occlusionLayer = adoptNS([[CALayer alloc] init]);
                    [occlusionLayer setFrame:rect];
                    [occlusionLayer setHitTestsAsOpaque:YES];
                    setInteractionRegionOcclusion(occlusionLayer.get());

                    if (applyBackgroundColorForDebugging) {
                        [occlusionLayer setBorderColor:cachedCGColor({ WebCore::SRGBA<float>(1, 0, 0, .2) }).get()];
                        [occlusionLayer setBorderWidth:6];
                        [occlusionLayer setName:@"Occlusion"];
                    }
                }

                if (!foundInPosition)
                    [layer insertSublayer:occlusionLayer.get() atIndex:insertionPoint];

                insertionPoint++;
                continue;
            }

            RetainPtr<CALayer> interactionLayer;
            auto interactionRegionGroupName = interactionRegionGroupNameForRegion(region);

            auto layerIterator = existingInteractionLayers.find(rect);
            if (layerIterator != existingInteractionLayers.end()) {
                interactionLayer = layerIterator->value;
                existingInteractionLayers.remove(layerIterator);
                if ([layer.sublayers objectAtIndex:insertionPoint] == interactionLayer)
                    foundInPosition = true;
                else
                    [interactionLayer removeFromSuperlayer];
            } else {
                interactionLayer = adoptNS([[interactionRegionLayerClass() alloc] init]);
                [interactionLayer setFrame:rect];
                [interactionLayer setHitTestsAsOpaque:YES];
                setInteractionRegion(interactionLayer.get(), interactionRegionGroupName);
                configureLayerForInteractionRegion(interactionLayer.get(), interactionRegionGroupName);

                if (applyBackgroundColorForDebugging) {
                    [interactionLayer setBackgroundColor:cachedCGColor({ WebCore::SRGBA<float>(0, 1, 0, .2) }).get()];
                    [interactionLayer setName:@"Interaction"];
                }
            }

            if (![interactionRegionGroupName isEqualToString:interactionRegionGroupNameForLayer(interactionLayer.get())])
                configureLayerForInteractionRegion(interactionLayer.get(), interactionRegionGroupName);

            [interactionLayer setCornerRadius:region.borderRadius];

            if (!foundInPosition)
                [layer insertSublayer:interactionLayer.get() atIndex:insertionPoint];

            insertionPoint++;
        }
    }

    for (CALayer *sublayer : existingOcclusionLayers.values())
        [sublayer removeFromSuperlayer];

    for (CALayer *sublayer : existingInteractionLayers.values())
        [sublayer removeFromSuperlayer];
}

} // namespace WebKit

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
