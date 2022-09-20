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
#import <WebCore/IntRectHash.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/RemoteLayerTreePropertyApplierInteractionRegionAdditions.mm>
#else
static void configureLayerForInteractionRegion(CALayer *, NSString *) { }
#endif

@interface WKInteractionRegion : NSObject
@property (nonatomic, assign) WebCore::InteractionRegion interactionRegion;
@end

@implementation WKInteractionRegion
@end

namespace WebKit {
using namespace WebCore;

NSString *interactionRegionKey = @"WKInteractionRegion";

static std::optional<WebCore::InteractionRegion> interactionRegionForLayer(CALayer *layer)
{
    id value = [layer valueForKey:interactionRegionKey];
    if (![value isKindOfClass:[WKInteractionRegion class]])
        return std::nullopt;
    WKInteractionRegion *region = (WKInteractionRegion *)value;
    return region.interactionRegion;
}

static bool isInteractionRegionLayer(CALayer *layer)
{
    return !!interactionRegionForLayer(layer);
}

static void setInteractionRegion(CALayer *layer, const WebCore::InteractionRegion& interactionRegion)
{
    WKInteractionRegion *region = [[[WKInteractionRegion alloc] init] autorelease];
    region.interactionRegion = interactionRegion;
    [layer setValue:region forKey:interactionRegionKey];
}

void appendInteractionRegionLayersForLayer(NSMutableArray *sublayers, CALayer *layer)
{
    for (CALayer *sublayer in layer.sublayers) {
        if (isInteractionRegionLayer(sublayer))
            [sublayers addObject:sublayer];
    }
}

void updateLayersForInteractionRegions(CALayer *layer, RemoteLayerTreeHost& host, const RemoteLayerTreeTransaction::LayerProperties& properties)
{
    ASSERT(properties.changedProperties & RemoteLayerTreeTransaction::EventRegionChanged);

    HashMap<IntRect, CALayer *> interactionRegionLayers;
    for (CALayer *sublayer in layer.sublayers) {
        if (!isInteractionRegionLayer(sublayer))
            continue;
        auto enclosingFrame = enclosingIntRect(sublayer.frame);
        if (enclosingFrame.isEmpty())
            continue;
        auto result = interactionRegionLayers.add(enclosingFrame, sublayer);
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    bool applyBackgroundColorForDebugging = [[NSUserDefaults standardUserDefaults] boolForKey:@"WKInteractionRegionDebugFill"];
    float minimumBorderRadius = host.drawingArea().page().preferences().interactionRegionMinimumCornerRadius();

    HashSet<IntRect> liveLayerBounds;
    for (const WebCore::InteractionRegion& region : properties.eventRegion.interactionRegions()) {
        for (IntRect rect : region.regionInLayerCoordinates.rects()) {
            if (!liveLayerBounds.add(rect).isNewEntry)
                continue;

            auto layerIterator = interactionRegionLayers.find(rect);

            RetainPtr<CALayer> interactionRegionLayer;
            if (layerIterator != interactionRegionLayers.end()) {
                interactionRegionLayer = layerIterator->value;
                interactionRegionLayers.remove(layerIterator);
            } else {
                interactionRegionLayer = adoptNS([[CALayer alloc] init]);
                [interactionRegionLayer setFrame:rect];
                [interactionRegionLayer setHitTestsAsOpaque:YES];

                if (applyBackgroundColorForDebugging)
                    [interactionRegionLayer setBackgroundColor:cachedCGColor({ WebCore::SRGBA<float>(0, 1, 0, .3) }).get()];

                setInteractionRegion(interactionRegionLayer.get(), region);
                configureLayerForInteractionRegion(interactionRegionLayer.get(), makeString("WKInteractionRegion-"_s, String::number(region.elementIdentifier.toUInt64())));

                [layer addSublayer:interactionRegionLayer.get()];
            }

            [interactionRegionLayer setCornerRadius:std::max(region.borderRadius, minimumBorderRadius)];
        }
    }

    for (CALayer *sublayer : interactionRegionLayers.values())
        [sublayer removeFromSuperlayer];
}

} // namespace WebKit

#endif // ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
