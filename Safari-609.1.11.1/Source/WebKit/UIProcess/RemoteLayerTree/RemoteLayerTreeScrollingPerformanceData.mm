/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeScrollingPerformanceData.h"

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeHost.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/TileController.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeScrollingPerformanceData::RemoteLayerTreeScrollingPerformanceData(RemoteLayerTreeDrawingAreaProxy& drawingArea)
    : m_drawingArea(drawingArea)
{
}

RemoteLayerTreeScrollingPerformanceData::~RemoteLayerTreeScrollingPerformanceData()
{
}

void RemoteLayerTreeScrollingPerformanceData::didCommitLayerTree(const FloatRect& visibleRect)
{
    // FIXME: maybe we only care about newly created tiles?
    appendBlankPixelCount(BlankPixelCount::Filled, blankPixelCount(visibleRect));
}

void RemoteLayerTreeScrollingPerformanceData::didScroll(const FloatRect& visibleRect)
{
    appendBlankPixelCount(BlankPixelCount::Exposed, blankPixelCount(visibleRect));
}

bool RemoteLayerTreeScrollingPerformanceData::BlankPixelCount::canCoalesce(BlankPixelCount::EventType type, unsigned pixelCount) const
{
    return eventType == type && blankPixelCount == pixelCount;
}

void RemoteLayerTreeScrollingPerformanceData::appendBlankPixelCount(BlankPixelCount::EventType eventType, unsigned blankPixelCount)
{
    double now = CFAbsoluteTimeGetCurrent();
    if (!m_blankPixelCounts.isEmpty() && m_blankPixelCounts.last().canCoalesce(eventType, blankPixelCount)) {
        m_blankPixelCounts.last().endTime = now;
        return;
    }

    m_blankPixelCounts.append(BlankPixelCount(now, now, eventType, blankPixelCount));
}

NSArray *RemoteLayerTreeScrollingPerformanceData::data()
{
    NSMutableArray* dataArray = [NSMutableArray arrayWithCapacity:m_blankPixelCounts.size()];
    
    for (auto pixelData : m_blankPixelCounts) {
        [dataArray addObject:@[
            @(pixelData.startTime),
            (pixelData.eventType == BlankPixelCount::Filled) ? @"filled" : @"exposed",
            @(pixelData.blankPixelCount)
        ]];
    }
    return dataArray;
}

static CALayer *findTileGridContainerLayer(CALayer *layer)
{
    for (CALayer *currLayer : [layer sublayers]) {
        String layerName = [currLayer name];
        if (layerName == TileController::tileGridContainerLayerName())
            return currLayer;

        if (CALayer *foundLayer = findTileGridContainerLayer(currLayer))
            return foundLayer;
    }

    return nil;
}

unsigned RemoteLayerTreeScrollingPerformanceData::blankPixelCount(const FloatRect& visibleRect) const
{
    CALayer *rootLayer = m_drawingArea.remoteLayerTreeHost().rootLayer();

    CALayer *tileGridContainer = findTileGridContainerLayer(rootLayer);
    if (!tileGridContainer) {
        NSLog(@"Failed to find TileGrid Container Layer");
        return UINT_MAX;
    }

    FloatRect visibleRectExcludingToolbar = visibleRect;
    if (visibleRectExcludingToolbar.y() < 0)
        visibleRectExcludingToolbar.setY(0);

    Region paintedVisibleTileRegion;

    for (CALayer *tileLayer : [tileGridContainer sublayers]) {
        FloatRect tileRect = [tileLayer convertRect:[tileLayer bounds] toLayer:tileGridContainer];
    
        tileRect.intersect(visibleRectExcludingToolbar);
        
        if (!tileRect.isEmpty())
            paintedVisibleTileRegion.unite(enclosingIntRect(tileRect));
    }

    Region uncoveredRegion(enclosingIntRect(visibleRectExcludingToolbar));
    uncoveredRegion.subtract(paintedVisibleTileRegion);

    return uncoveredRegion.totalArea();
}

}
