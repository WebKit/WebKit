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
    , m_tileContainerLayer(adoptCF([[CALayer alloc] init]))
    , m_tileSize(tileSize)
    , m_tileRevalidationTimer(this, &TileCache::tileRevalidationTimerFired)
    , m_acceleratesDrawing(false)
    , m_tileDebugBorderWidth(0)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [m_tileCacheLayer addSublayer:m_tileContainerLayer.get()];
    [CATransaction commit];
}

void TileCache::tileCacheLayerBoundsChanged()
{
    if (m_tiles.isEmpty()) {
        // We must revalidate immediately instead of using a timer when there are
        // no tiles to avoid a flash when transitioning from one page to another.
        revalidateTiles();
        return;
    }

    scheduleTileRevalidation();
}

void TileCache::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}

void TileCache::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_tiles.isEmpty())
        return;

    // Find the tiles that need to be invalidated.
    TileIndex topLeft;
    TileIndex bottomRight;
    getTileIndexRangeForRect(rect, topLeft, bottomRight);

    for (int y = topLeft.y(); y <= bottomRight.y(); ++y) {
        for (int x = topLeft.x(); x <= bottomRight.x(); ++x) {
            WebTileLayer* tileLayer = tileLayerAtIndex(TileIndex(x, y));
            if (!tileLayer)
                continue;

            CGRect tileRect = [m_tileCacheLayer convertRect:rect toLayer:tileLayer];
            if (CGRectIsEmpty(tileRect))
                continue;

            [tileLayer setNeedsDisplayInRect:tileRect];

            if (shouldShowRepaintCounters()) {
                CGRect bounds = [tileLayer bounds];
                CGRect indicatorRect = CGRectMake(bounds.origin.x, bounds.origin.y, 52, 27);
                [tileLayer setNeedsDisplayInRect:indicatorRect];
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

    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it)
        [it->second.get() setAcceleratesDrawing:m_acceleratesDrawing];
#else
    UNUSED_PARAM(acceleratesDrawing);
#endif
}

void TileCache::visibleRectChanged()
{
    scheduleTileRevalidation();
}

void TileCache::setTileDebugBorderWidth(float borderWidth)
{
    if (m_tileDebugBorderWidth == borderWidth)
        return;

    m_tileDebugBorderWidth = borderWidth;
    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it)
        [it->second.get() setBorderWidth:m_tileDebugBorderWidth];
}

void TileCache::setTileDebugBorderColor(CGColorRef borderColor)
{
    if (m_tileDebugBorderColor == borderColor)
        return;

    m_tileDebugBorderColor = borderColor;
    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it)
        [it->second.get() setBorderColor:m_tileDebugBorderColor.get()];
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

IntRect TileCache::rectForTileIndex(const TileIndex& tileIndex) const
{
    return IntRect(IntPoint(tileIndex.x() * m_tileSize.width(), tileIndex.y() * m_tileSize.height()), m_tileSize);
}

void TileCache::getTileIndexRangeForRect(const IntRect& rect, TileIndex& topLeft, TileIndex& bottomRight)
{
    IntRect clampedRect = intersection(rect, bounds());

    topLeft.setX(max(clampedRect.x() / m_tileSize.width(), 0));
    topLeft.setY(max(clampedRect.y() / m_tileSize.height(), 0));
    bottomRight.setX(max(clampedRect.maxX() / m_tileSize.width(), 0));
    bottomRight.setY(max(clampedRect.maxY() / m_tileSize.height(), 0));
}

void TileCache::scheduleTileRevalidation()
{
    if (m_tileRevalidationTimer.isActive())
        return;

    m_tileRevalidationTimer.startOneShot(0);
}

void TileCache::tileRevalidationTimerFired(Timer<TileCache>*)
{
    revalidateTiles();
}

void TileCache::revalidateTiles()
{
    IntRect tileCoverageRect = enclosingIntRect(visibleRect());
    if (tileCoverageRect.isEmpty())
        return;

    // Inflate the coverage rect so that it covers 2x of the visible width and 3x of the visible height.
    // These values were chosen because it's more common to have tall pages and to scroll vertically,
    // so we keep more tiles above and below the current area.
    tileCoverageRect.inflateX(tileCoverageRect.width() / 2);
    tileCoverageRect.inflateY(tileCoverageRect.height());

    Vector<TileIndex> tilesToRemove;

    for (TileMap::iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it) {
        const TileIndex& tileIndex = it->first;

        WebTileLayer* tileLayer = it->second.get();

        if (!rectForTileIndex(tileIndex).intersects(tileCoverageRect)) {
            // Remove this layer.
            [tileLayer removeFromSuperlayer];
            [tileLayer setTileCache:0];

            tilesToRemove.append(tileIndex);
        }
    }

    // FIXME: Be more clever about which tiles to remove. We might not want to remove all
    // the tiles that are outside the coverage rect. When we know that we're going to be scrolling up,
    // we might want to remove the ones below the coverage rect but keep the ones above.
    for (size_t i = 0; i < tilesToRemove.size(); ++i)
        m_tiles.remove(tilesToRemove[i]);

    TileIndex topLeft;
    TileIndex bottomRight;
    getTileIndexRangeForRect(tileCoverageRect, topLeft, bottomRight);

    bool didCreateNewTiles = false;

    for (int y = topLeft.y(); y <= bottomRight.y(); ++y) {
        for (int x = topLeft.x(); x <= bottomRight.x(); ++x) {
            TileIndex tileIndex(x, y);

            RetainPtr<WebTileLayer>& tileLayer = m_tiles.add(tileIndex, 0).first->second;
            if (tileLayer) {
                // We already have a layer for this tile.
                continue;
            }

            didCreateNewTiles = true;

            tileLayer = createTileLayer();

            [tileLayer.get() setNeedsDisplay];
            [tileLayer.get() setPosition:CGPointMake(x * m_tileSize.width(), y * m_tileSize.height())];
            [m_tileContainerLayer.get() addSublayer:tileLayer.get()];
        }
    }

    if (!didCreateNewTiles)
        return;

    PlatformCALayer* platformLayer = PlatformCALayer::platformCALayer(m_tileCacheLayer);
    platformLayer->owner()->platformCALayerDidCreateTiles();
}

WebTileLayer* TileCache::tileLayerAtIndex(const TileIndex& index) const
{
    return m_tiles.get(index).get();
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
