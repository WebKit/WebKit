/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "RemoteScrollingTreeCocoa.h"

#if PLATFORM(COCOA) && ENABLE(UI_SIDE_COMPOSITING)

#import "Logging.h"
#import "RemoteLayerTreeNode.h"
#import <WebCore/WebCoreCALayerExtras.h>


namespace WebKit {
using namespace WebCore;

OptionalEventRegionConstRef eventRegionForLayer(CALayer *layer)
{
    if (RefPtr layerTreeNode = RemoteLayerTreeNode::forCALayer(layer))
        return std::cref(layerTreeNode->eventRegion());
    return std::nullopt;
}

bool layerEventRegionContainsPoint(CALayer *layer, CGPoint localPoint)
{
    return eventRegionForLayer(layer).transform([layerBounds = layer.bounds, localPoint](const WebCore::EventRegion& eventRegion) {
        // Scrolling changes boundsOrigin on the scroll container layer, but we computed its event region ignoring scroll position, so factor out bounds origin.
        FloatPoint boundsOrigin = layerBounds.origin;
        FloatPoint originRelativePoint = localPoint - toFloatSize(boundsOrigin);
        return eventRegion.contains(roundedIntPoint(originRelativePoint));
    }).value_or(false);
}

OptionalEventRegionConstRef eventRegionForPoint(CALayer* rootLayer, FloatPoint& location)
{
    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, rootLayer, location, layerEventRegionContainsPoint);

    if (layersAtPoint.isEmpty())
        return std::nullopt;

    auto [hitLayer, localPoint] = layersAtPoint.last();
    if (!hitLayer)
        return std::nullopt;

    location = roundedIntPoint(localPoint);
    return eventRegionForLayer(hitLayer.get());
}

}

#endif // PLATFORM(COCOA) && ENABLE(UI_SIDE_COMPOSITING)
