/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "TileCache.h"

#import "IntRect.h"
#import "PlatformCALayer.h"
#import "WebLayer.h"
#import "WebTileCacheLayer.h"
#import "WebTileLayer.h"
#import <utility>

using namespace std;

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
@interface CALayer (WebCALayerDetails)
- (void)setAcceleratesDrawing:(BOOL)flag;
@end
#endif

namespace WebCore {

PassOwnPtr<TileCache> TileCache::create(WebTileCacheLayer* tileCacheLayer, const IntSize& tileSize)
{
    return adoptPtr(new TileCache(tileCacheLayer, tileSize));
}

TileCache::TileCache(WebTileCacheLayer* tileCacheLayer, const IntSize& tileSize)
    : m_tileCacheLayer(tileCacheLayer)
    , m_tileSize(tileSize)
    , m_tileContainerLayer(adoptCF([[CALayer alloc] init]))
    , m_tileDebugBorderWidth(0)
    , m_acceleratesDrawing(false)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [m_tileCacheLayer addSublayer:m_tileContainerLayer.get()];
    [CATransaction commit];
}

void TileCache::tileCacheLayerBoundsChanged()
{
    IntSize numTilesInGrid = numTilesForGridSize(bounds().size());
    if (numTilesInGrid == m_numTilesInGrid)
        return;

    resizeTileGrid(numTilesInGrid);
}

void TileCache::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}

void TileCache::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_numTilesInGrid.isZero())
        return;

    // Find the tiles that need to be invalidated.
    IntPoint topLeft;
    IntPoint bottomRight;
    getTileRangeForRect(rect, topLeft, bottomRight);

    for (int y = topLeft.y(); y <= bottomRight.y(); ++y) {
        for (int x = topLeft.x(); x <= bottomRight.x(); ++x) {
            WebTileLayer* tileLayer = tileLayerAtPosition(IntPoint(x, y));

            CGRect tileRect = [m_tileCacheLayer convertRect:rect toLayer:tileLayer];

            if (!CGRectIsEmpty(tileRect)) {
                [tileLayer setNeedsDisplayInRect:tileRect];

                if (shouldShowRepaintCounters()) {
                    CGRect bounds = [tileLayer bounds];
                    CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 52, 27);
                    [tileLayer setNeedsDisplayInRect:indicatorRect];
                }
            }
        }
    }
}

void TileCache::drawLayer(WebTileLayer* layer, CGContextRef context)
{
    PlatformCALayer* platformLayer = PlatformCALayer::platformCALayer(m_tileCacheLayer);
    if (!platformLayer)
        return;

    CGContextSaveGState(context);

    CGPoint layerOrigin = [layer frame].origin;
    CGContextTranslateCTM(context, -layerOrigin.x, -layerOrigin.y);
    drawLayerContents(context, layer, platformLayer);

    CGContextRestoreGState(context);

    unsigned repaintCount = [layer incrementRepaintCount];
    if (!shouldShowRepaintCounters())
        return;

    // FIXME: Some of this code could be shared with WebLayer.
    char text[16]; // that's a lot of repaints
    snprintf(text, sizeof(text), "%d", repaintCount);

    CGRect indicatorBox = [layer bounds];
    indicatorBox.size.width = 12 + 10 * strlen(text);
    indicatorBox.size.height = 27;
    CGContextSaveGState(context);

    CGContextSetAlpha(context, 0.5f);
    CGContextBeginTransparencyLayerWithRect(context, indicatorBox, 0);

    CGContextSetFillColorWithColor(context, m_tileDebugBorderColor.get());
    CGContextFillRect(context, indicatorBox);

    if (platformLayer->acceleratesDrawing())
        CGContextSetRGBFillColor(context, 1, 0, 0, 1);
    else
        CGContextSetRGBFillColor(context, 1, 1, 1, 1);

    CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
    CGContextSelectFont(context, "Helvetica", 22, kCGEncodingMacRoman);
    CGContextShowTextAtPoint(context, indicatorBox.origin.x + 5, indicatorBox.origin.y + 22, text, strlen(text));

    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void TileCache::setAcceleratesDrawing(bool acceleratesDrawing)
{
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    if (m_acceleratesDrawing == acceleratesDrawing)
        return;

    m_acceleratesDrawing = acceleratesDrawing;

    for (WebTileLayer* tileLayer in [m_tileContainerLayer.get() sublayers])
        [tileLayer setAcceleratesDrawing:m_acceleratesDrawing];
#else
    UNUSED_PARAM(acceleratesDrawing);
#endif
}

void TileCache::visibleRectChanged()
{
    // FIXME: Implement.
}

void TileCache::setTileDebugBorderWidth(float borderWidth)
{
    if (m_tileDebugBorderWidth == borderWidth)
        return;

    m_tileDebugBorderWidth = borderWidth;
    for (WebTileLayer* tileLayer in [m_tileContainerLayer.get() sublayers])
        [tileLayer setBorderWidth:m_tileDebugBorderWidth];
}

void TileCache::setTileDebugBorderColor(CGColorRef borderColor)
{
    if (m_tileDebugBorderColor == borderColor)
        return;

    m_tileDebugBorderColor = borderColor;
    for (WebTileLayer* tileLayer in [m_tileContainerLayer.get() sublayers])
        [tileLayer setBorderColor:m_tileDebugBorderColor.get()];
}

FloatRect TileCache::visibleRect() const
{
    CGRect rect = [m_tileCacheLayer bounds];

    CALayer *layer = m_tileCacheLayer;
    CALayer *superlayer = [layer superlayer];

    while (superlayer) {
        CGRect rectInSuperlayerCoordinates = [superlayer convertRect:rect fromLayer:layer];

        if ([superlayer masksToBounds])
            rect = CGRectIntersection([superlayer bounds], rectInSuperlayerCoordinates);
        else
            rect = rectInSuperlayerCoordinates;

        layer = superlayer;
        superlayer = [layer superlayer];
    }

    return [m_tileCacheLayer convertRect:rect fromLayer:layer];
}

IntRect TileCache::bounds() const
{
    return IntRect(IntPoint(), IntSize([m_tileCacheLayer bounds].size));
}

void TileCache::getTileRangeForRect(const IntRect& rect, IntPoint& topLeft, IntPoint& bottomRight)
{
    topLeft.setX(max(rect.x() / m_tileSize.width(), 0));
    topLeft.setY(max(rect.y() / m_tileSize.height(), 0));
    bottomRight.setX(min(rect.maxX() / m_tileSize.width(), m_numTilesInGrid.width() - 1));
    bottomRight.setY(min(rect.maxY() / m_tileSize.height(), m_numTilesInGrid.height() - 1));
}

IntSize TileCache::numTilesForGridSize(const IntSize& gridSize) const
{
    int numXTiles = ceil(static_cast<double>(gridSize.width()) / m_tileSize.width());
    int numYTiles = ceil(static_cast<double>(gridSize.height()) / m_tileSize.height());

    return IntSize(numXTiles, numYTiles);
}

void TileCache::resizeTileGrid(const IntSize& numTilesInGrid)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<NSMutableArray> newSublayers = adoptNS([[NSMutableArray alloc] initWithCapacity:numTilesInGrid.width() * numTilesInGrid.height()]);

    for (int y = 0; y < numTilesInGrid.height(); ++y) {
        for (int x = 0; x < numTilesInGrid.width(); ++x) {
            RetainPtr<WebTileLayer> tileLayer;

            if (x < m_numTilesInGrid.width() && y < m_numTilesInGrid.height()) {
                // We can reuse the tile layer at this position.
                tileLayer = tileLayerAtPosition(IntPoint(x, y));
            } else {
                tileLayer = createTileLayer();
            }

            [tileLayer.get() setPosition:CGPointMake(x * m_tileSize.width(), y * m_tileSize.height())];
            [newSublayers.get() addObject:tileLayer.get()];
        }
    }

    // FIXME: Make sure to call setTileCache:0 on the layers that get thrown away here.
    [m_tileContainerLayer.get() setSublayers:newSublayers.get()];
    m_numTilesInGrid = numTilesInGrid;

    [CATransaction commit];
}

WebTileLayer* TileCache::tileLayerAtPosition(const IntPoint& point) const
{
    ASSERT(point.x() >= 0);
    ASSERT(point.x() <= m_numTilesInGrid.width());
    ASSERT(point.y() >= 0);
    ASSERT(point.y() <= m_numTilesInGrid.height());

    return [[m_tileContainerLayer.get() sublayers] objectAtIndex:point.y() * m_numTilesInGrid.width() + point.x()];
}

RetainPtr<WebTileLayer> TileCache::createTileLayer()
{
    RetainPtr<WebTileLayer> layer = adoptNS([[WebTileLayer alloc] init]);
    [layer.get() setBounds:CGRectMake(0, 0, m_tileSize.width(), m_tileSize.height())];
    [layer.get() setAnchorPoint:CGPointZero];
    [layer.get() setTileCache:this];
    [layer.get() setBorderColor:m_tileDebugBorderColor.get()];
    [layer.get() setBorderWidth:m_tileDebugBorderWidth];

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    [layer.get() setAcceleratesDrawing:m_acceleratesDrawing];
#endif

    return layer;
}

bool TileCache::shouldShowRepaintCounters() const
{
    PlatformCALayer* platformLayer = PlatformCALayer::platformCALayer(m_tileCacheLayer);
    if (!platformLayer)
        return false;

    WebCore::PlatformCALayerClient* layerContents = platformLayer->owner();
    ASSERT(layerContents);
    if (!layerContents)
        return false;

    return layerContents->platformCALayerShowRepaintCounter();
}

} // namespace WebCore
