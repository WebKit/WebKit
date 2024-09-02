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
#import "WebPageProxy.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/PerformanceLoggingClient.h>
#import <WebCore/TileController.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeScrollingPerformanceData);

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
    appendBlankPixelCount(ScrollingLogEvent::Filled, blankPixelCount(visibleRect));
    logData();
}

void RemoteLayerTreeScrollingPerformanceData::didScroll(const FloatRect& visibleRect)
{
    auto pixelCount = blankPixelCount(visibleRect);
#if PLATFORM(MAC)
    if (pixelCount || m_lastUnfilledArea)
        m_lastUnfilledArea = pixelCount;
    else
        return;
#endif
    appendBlankPixelCount(ScrollingLogEvent::Exposed, pixelCount);
    logData();
}

bool RemoteLayerTreeScrollingPerformanceData::ScrollingLogEvent::canCoalesce(ScrollingLogEvent::EventType type, uint64_t pixelCount) const
{
    return eventType == type && value == pixelCount;
}

void RemoteLayerTreeScrollingPerformanceData::didChangeSynchronousScrollingReasons(WTF::MonotonicTime timestamp, uint64_t data)
{
    appendSynchronousScrollingChange(timestamp, data);
    logData();
}

void RemoteLayerTreeScrollingPerformanceData::appendBlankPixelCount(ScrollingLogEvent::EventType eventType, uint64_t blankPixelCount)
{
    auto now = MonotonicTime::now();
#if !PLATFORM(MAC)
    if (!m_events.isEmpty() && m_events.last().canCoalesce(eventType, blankPixelCount)) {
        m_events.last().endTime = now;
        return;
    }
#endif
    m_events.append(ScrollingLogEvent(now, now, eventType, blankPixelCount));
}

void RemoteLayerTreeScrollingPerformanceData::appendSynchronousScrollingChange(WTF::MonotonicTime timestamp, uint64_t scrollingChangeData)
{
    m_events.append(ScrollingLogEvent(timestamp, timestamp, ScrollingLogEvent::SwitchedScrollingMode, scrollingChangeData));
}

NSArray *RemoteLayerTreeScrollingPerformanceData::data()
{
    return createNSArray(m_events, [] (auto& pixelData) {
        return @[
            @(pixelData.startTime.toMachAbsoluteTime()),
            (pixelData.eventType == ScrollingLogEvent::Filled) ? @"filled" : @"exposed",
            @(pixelData.value)
        ];
    }).autorelease();
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
    CALayer *rootLayer = m_drawingArea->remoteLayerTreeHost().rootLayer();

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

void RemoteLayerTreeScrollingPerformanceData::logData()
{
#if PLATFORM(MAC)
    for (auto event : m_events) {
        switch (event.eventType) {
        case ScrollingLogEvent::SwitchedScrollingMode: {
            m_drawingArea->page().logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::SwitchedScrollingMode), event.startTime, event.value);
            break;
        }
        case ScrollingLogEvent::Exposed: {
            m_drawingArea->page().logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::ExposedTilelessArea), event.startTime, event.value);
            break;
        }
        case ScrollingLogEvent::Filled: {
            m_drawingArea->page().logScrollingEvent(static_cast<uint32_t>(PerformanceLoggingClient::ScrollingEvent::FilledTile), event.startTime, event.value);
            break;
        }
        default:
            break;
        }
    }
    m_events.clear();
#endif
}

}
