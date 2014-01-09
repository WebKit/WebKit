/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TileCache.h"

#if PLATFORM(IOS)

#include "Logging.h"
#include "MemoryPressureHandler.h"
#include "SystemMemory.h"
#include "TileGrid.h"
#include "TileGridTile.h"
#include "TileLayer.h"
#include "TileLayerPool.h"
#include "WAKWindow.h"
#include "WKGraphics.h"
#include "WebCoreSystemInterface.h"
#include "WebCoreThreadRun.h"
#include <QuartzCore/QuartzCore.h>
#include <QuartzCore/QuartzCorePrivate.h>
#include <wtf/CurrentTime.h>

@interface WAKView (WebViewExtras)
- (void)_dispatchTileDidDraw:(CALayer*)tile;
- (void)_willStartScrollingOrZooming;
- (void)_didFinishScrollingOrZooming;
- (void)_dispatchTileDidDraw;
- (void)_scheduleLayerFlushForPendingTileCacheRepaint;
@end

@interface TileCacheTombstone : NSObject {
    BOOL dead;
}
@property(getter=isDead) BOOL dead;

@end

@implementation TileCacheTombstone

@synthesize dead;

@end

namespace WebCore {

TileCache::TileCache(WAKWindow* window)
    : m_window(window)
    , m_keepsZoomedOutTiles(false)
    , m_hasPendingLayoutTiles(false)
    , m_hasPendingUpdateTilingMode(false)
    , m_tombstone(adoptNS([[TileCacheTombstone alloc] init]))
    , m_tilingMode(Normal)
    , m_tilingDirection(TilingDirectionDown)
    , m_tileSize(512, 512)
    , m_tilesOpaque(true)
    , m_tileBordersVisible(false)
    , m_tilePaintCountersVisible(false)
    , m_acceleratedDrawingEnabled(false)
    , m_isSpeculativeTileCreationEnabled(true)
    , m_didCallWillStartScrollingOrZooming(false)
    , m_zoomedOutTileGrid(PassOwnPtr<TileGrid>())
    , m_zoomedInTileGrid(PassOwnPtr<TileGrid>())
    , m_tileCreationTimer(this, &TileCache::tileCreationTimerFired)
    , m_currentScale(1.f)
    , m_pendingScale(0)
    , m_pendingZoomedOutScale(0)
{
    m_zoomedOutTileGrid = TileGrid::create(this, m_tileSize);
    [hostLayer() insertSublayer:m_zoomedOutTileGrid->tileHostLayer() atIndex:0];
    hostLayerSizeChanged();
}

TileCache::~TileCache()
{
    [m_tombstone.get() setDead:true];
}

CGFloat TileCache::screenScale() const
{
    return [m_window screenScale];
}

CALayer* TileCache::hostLayer() const
{
    return [m_window hostLayer];
}

FloatRect TileCache::visibleRectInLayer(CALayer *layer) const
{
    return [layer convertRect:[m_window extendedVisibleRect] fromLayer:hostLayer()];
}

bool TileCache::tilesOpaque() const
{
    return m_tilesOpaque;
}
    
TileGrid* TileCache::activeTileGrid() const
{
    if (!m_keepsZoomedOutTiles)
        return m_zoomedOutTileGrid.get();
    if (m_tilingMode == Zooming)
        return m_zoomedOutTileGrid.get();
    if (m_zoomedInTileGrid && m_currentScale == m_zoomedInTileGrid->scale())
        return m_zoomedInTileGrid.get();
    return m_zoomedOutTileGrid.get();
}

TileGrid* TileCache::inactiveTileGrid() const
{
    return activeTileGrid() == m_zoomedOutTileGrid ? m_zoomedInTileGrid.get() : m_zoomedOutTileGrid.get();
}

void TileCache::setTilesOpaque(bool opaque)
{
    if (m_tilesOpaque == opaque)
        return;

    MutexLocker locker(m_tileMutex);

    m_tilesOpaque = opaque;
    m_zoomedOutTileGrid->updateTileOpacity();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateTileOpacity();
}

void TileCache::doLayoutTiles()
{
    if (isTileCreationSuspended())
        return;

    MutexLocker locker(m_tileMutex);
    TileGrid* activeGrid = activeTileGrid();
    // Even though we aren't actually creating tiles in the inactive grid, we
    // still need to drop invalid tiles in response to a layout.
    // See <rdar://problem/9839867>.
    if (TileGrid* inactiveGrid = inactiveTileGrid())
        inactiveGrid->dropInvalidTiles();
    if (activeGrid->checkDoSingleTileLayout())
        return;
    createTilesInActiveGrid(CoverVisibleOnly);
}

void TileCache::hostLayerSizeChanged()
{
    m_zoomedOutTileGrid->updateHostLayerSize();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateHostLayerSize();
}

void TileCache::setKeepsZoomedOutTiles(bool keep)
{
    m_keepsZoomedOutTiles = keep;
}

void TileCache::setCurrentScale(float scale)
{
    ASSERT(scale > 0);

    if (currentScale() == scale) {
        m_pendingScale = 0;
        return;
    }
    m_pendingScale = scale;
    if (m_tilingMode == Disabled)
        return;
    commitScaleChange();

    if (!keepsZoomedOutTiles() && !isTileInvalidationSuspended()) {
        // Tile invalidation is normally suspended during zooming by UIKit but some applications
        // using custom scrollviews may zoom without triggering the callbacks. Invalidate the tiles explicitly.
        MutexLocker locker(m_tileMutex);
        activeTileGrid()->dropAllTiles();
        activeTileGrid()->createTiles(CoverVisibleOnly);
    }
}

void TileCache::setZoomedOutScale(float scale)
{
    ASSERT(scale > 0);

    if (zoomedOutScale() == scale) {
        m_pendingZoomedOutScale = 0;
        return;
    }
    m_pendingZoomedOutScale = scale;
    if (m_tilingMode == Disabled)
        return;
    commitScaleChange();
}
    
void TileCache::commitScaleChange()
{
    ASSERT(m_pendingZoomedOutScale || m_pendingScale);
    ASSERT(m_tilingMode != Disabled);
    
    MutexLocker locker(m_tileMutex);

    if (m_pendingZoomedOutScale) {
        m_zoomedOutTileGrid->setScale(m_pendingZoomedOutScale);
        m_pendingZoomedOutScale = 0;
    }
    
    if (!m_keepsZoomedOutTiles) {
        ASSERT(activeTileGrid() == m_zoomedOutTileGrid);
        if (m_pendingScale) {
            m_currentScale = m_pendingScale;
            m_zoomedOutTileGrid->setScale(m_currentScale);
        }
        m_pendingScale = 0;
        return;
    }

    if (m_pendingScale) {
        m_currentScale = m_pendingScale;
        m_pendingScale = 0;
    }

    if (m_currentScale != m_zoomedOutTileGrid->scale()) {
        if (!m_zoomedInTileGrid) {
            m_zoomedInTileGrid = TileGrid::create(this, m_tileSize);
            [hostLayer() addSublayer:m_zoomedInTileGrid->tileHostLayer()];
            hostLayerSizeChanged();
        }
        m_zoomedInTileGrid->setScale(m_currentScale);
    }

    // Keep the current ordering during zooming.
    if (m_tilingMode != Zooming)
        bringActiveTileGridToFront();

    adjustTileGridTransforms();
    layoutTiles();
}

void TileCache::bringActiveTileGridToFront()
{
    TileGrid* activeGrid = activeTileGrid();
    TileGrid* otherGrid = inactiveTileGrid();
    if (!otherGrid)
        return;
    CALayer* frontLayer = activeGrid->tileHostLayer();
    CALayer* otherLayer = otherGrid->tileHostLayer();
    [hostLayer() insertSublayer:frontLayer above:otherLayer];
}
    
void TileCache::adjustTileGridTransforms()
{
    CALayer* zoomedOutHostLayer = m_zoomedOutTileGrid->tileHostLayer();
    float transformScale = currentScale() / zoomedOutScale();
    [zoomedOutHostLayer setTransform:CATransform3DMakeScale(transformScale, transformScale, 1.0f)];
    m_zoomedOutTileGrid->updateHostLayerSize();
}

void TileCache::layoutTiles()
{
    if (m_hasPendingLayoutTiles)
        return;
    m_hasPendingLayoutTiles = true;

    TileCacheTombstone *tombstone = m_tombstone.get();
    WebThreadRun(^{
        if ([tombstone isDead])
            return;
        m_hasPendingLayoutTiles = false;
        doLayoutTiles();
    });
}

void TileCache::layoutTilesNow()
{
    ASSERT(WebThreadIsLockedOrDisabled());

    // layoutTilesNow() is called after a zoom, while the tile mode is still set to Zooming.
    // If we checked for isTileCreationSuspended here, that would cause <rdar://problem/8434112> (Page flashes after zooming in/out).
    if (m_tilingMode == Disabled)
        return;
    
    // FIXME: layoutTilesNow should be called after state has been set to non-zooming and the active grid is the final one. 
    // Fix this in UIKit side (perhaps also getting rid of this call) and remove this code afterwards.
    // <rdar://problem/9672993>
    TilingMode savedTilingMode = m_tilingMode;
    if (m_tilingMode == Zooming)
        m_tilingMode = Minimal;

    MutexLocker locker(m_tileMutex);
    TileGrid* activeGrid = activeTileGrid();
    if (activeGrid->checkDoSingleTileLayout()) {
        m_tilingMode = savedTilingMode;
        return;
    }
    createTilesInActiveGrid(CoverVisibleOnly);
    m_tilingMode = savedTilingMode;
}

void TileCache::layoutTilesNowForRect(const IntRect& rect)
{
    ASSERT(WebThreadIsLockedOrDisabled());
    MutexLocker locker(m_tileMutex);

    activeTileGrid()->addTilesCoveringRect(rect);
}

void TileCache::removeAllNonVisibleTiles()
{
    MutexLocker locker(m_tileMutex);
    removeAllNonVisibleTilesInternal();
}

void TileCache::removeAllNonVisibleTilesInternal()
{
    TileGrid* activeGrid = activeTileGrid();
    if (keepsZoomedOutTiles() && activeGrid == m_zoomedInTileGrid && activeGrid->hasTiles())
        m_zoomedOutTileGrid->dropAllTiles();

    IntRect activeTileBounds = activeGrid->bounds();
    if (activeTileBounds.width() <= m_tileSize.width() && activeTileBounds.height() <= m_tileSize.height()) {
        // If the view is smaller than a tile, keep the tile even if it is not visible.
        activeGrid->dropTilesOutsideRect(activeTileBounds);
        return;
    }

    activeGrid->dropTilesOutsideRect(activeGrid->visibleRect());
}

void TileCache::removeAllTiles()
{
    MutexLocker locker(m_tileMutex);
    m_zoomedOutTileGrid->dropAllTiles();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->dropAllTiles();
}

void TileCache::removeForegroundTiles()
{
    MutexLocker locker(m_tileMutex);
    if (!keepsZoomedOutTiles())
        m_zoomedOutTileGrid->dropAllTiles();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->dropAllTiles();
}

void TileCache::setContentReplacementImage(RetainPtr<CGImageRef> contentReplacementImage)
{
    MutexLocker locker(m_contentReplacementImageMutex);
    m_contentReplacementImage = contentReplacementImage;
}

RetainPtr<CGImageRef> TileCache::contentReplacementImage() const
{
    MutexLocker locker(m_contentReplacementImageMutex);
    return m_contentReplacementImage;
}

void TileCache::setTileBordersVisible(bool flag)
{
    if (flag == m_tileBordersVisible)
        return;

    m_tileBordersVisible = flag;
    m_zoomedOutTileGrid->updateTileBorderVisibility();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateTileBorderVisibility();
}

void TileCache::setTilePaintCountersVisible(bool flag)
{
    m_tilePaintCountersVisible = flag;
    // The numbers will show up the next time the tiles paint.
}

void TileCache::finishedCreatingTiles(bool didCreateTiles, bool createMore)
{
    // We need to ensure that all tiles are showing the same version of the content.
    if (didCreateTiles && !m_savedDisplayRects.isEmpty())
        flushSavedDisplayRects();

    if (keepsZoomedOutTiles()) {
        if (m_zoomedInTileGrid && activeTileGrid() == m_zoomedOutTileGrid && m_tilingMode != Zooming && m_zoomedInTileGrid->hasTiles()) {
            // This CA transaction will cover the screen with top level tiles.
            // We can remove zoomed-in tiles without flashing.
            m_zoomedInTileGrid->dropAllTiles();
        } else if (activeTileGrid() == m_zoomedInTileGrid) {
            // Pass the minimum possible distance to consider all tiles, even visible ones.
            m_zoomedOutTileGrid->dropDistantTiles(0, std::numeric_limits<double>::min());
        }
    }

    // Keep creating tiles until the whole coverRect is covered.
    if (createMore)
        m_tileCreationTimer.startOneShot(0);
}

void TileCache::tileCreationTimerFired(Timer<TileCache>*)
{
    if (isTileCreationSuspended())
        return;
    MutexLocker locker(m_tileMutex);
    createTilesInActiveGrid(CoverSpeculative);
}

void TileCache::createTilesInActiveGrid(SynchronousTileCreationMode mode)
{
    if (memoryPressureHandler().hasReceivedMemoryPressure()) {
        LOG(MemoryPressure, "Under memory pressure at: %s", __PRETTY_FUNCTION__);
        removeAllNonVisibleTilesInternal();
    }
    activeTileGrid()->createTiles(mode);
}

unsigned TileCache::tileCapacityForGrid(TileGrid* grid)
{
    static unsigned capacity;
    if (!capacity) {
        size_t totalMemory = systemTotalMemory();
        totalMemory /= 1024 * 1024;
        if (totalMemory >= 1024)
            capacity = 128 * 1024 * 1024;
        else if (totalMemory >= 512)
            capacity = 64 * 1024 * 1024;
        else
            capacity = 32 * 1024 * 1024;
    }

    int gridCapacity;

    int memoryLevel = systemMemoryLevel();
    if (memoryLevel < 15)
        gridCapacity = capacity / 4;
    else if (memoryLevel < 20)
        gridCapacity = capacity / 2;
    else if (memoryLevel < 30) 
        gridCapacity = capacity * 3 / 4;
    else
        gridCapacity = capacity;

    if (keepsZoomedOutTiles() && grid == m_zoomedOutTileGrid) {
        if (activeTileGrid() == m_zoomedOutTileGrid)
            return gridCapacity;
        return gridCapacity / 4;
    }
    return gridCapacity * 3 / 4;
}

Color TileCache::colorForGridTileBorder(TileGrid* grid) const
{
    if (grid == m_zoomedOutTileGrid)
        return Color(.3f, .0f, 0.4f, 0.5f);

    return Color(.0f, .0f, 0.4f, 0.5f);
}

static bool shouldRepaintInPieces(const CGRect& dirtyRect, CGSRegionObj dirtyRegion, CGFloat contentsScale)
{
    // Estimate whether or not we should use the unioned rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the union. If that wasted space
    // is too large, then we will do individual rect painting instead.
    float singlePixels = 0;
    unsigned rectCount = 0;

    CGSRegionEnumeratorObj enumerator = CGSRegionEnumerator(dirtyRegion);
    CGRect *subRect;
    while ((subRect = CGSNextRect(enumerator))) {
        ++rectCount;
        singlePixels += subRect->size.width * subRect->size.height;
    }
    singlePixels /= (contentsScale * contentsScale);
    CGSReleaseRegionEnumerator(enumerator);

    const unsigned cRectThreshold = 10;
    if (rectCount < 2 || rectCount > cRectThreshold)
        return false;

    const float cWastedSpaceThreshold = 0.50f;
    float unionPixels = dirtyRect.size.width * dirtyRect.size.height;
    float wastedSpace = 1.f - (singlePixels / unionPixels);
    return wastedSpace > cWastedSpaceThreshold;
}

void TileCache::drawReplacementImage(TileLayer* layer, CGContextRef context, CGImageRef image)
{
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, CGContextGetClipBoundingBox(context));

    CGFloat contentsScale = [layer contentsScale];
    CGContextScaleCTM(context, 1 / contentsScale, -1 / contentsScale);
    CGRect imageRect = CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));
    CGContextTranslateCTM(context, 0, -imageRect.size.height);
    CGContextDrawImage(context, imageRect, image);
}

void TileCache::drawWindowContent(TileLayer* layer, CGContextRef context, CGRect dirtyRect)
{
    CGRect frame = [layer frame];
    WKFontAntialiasingStateSaver fontAntialiasingState(context, [m_window useOrientationDependentFontAntialiasing] && [layer isOpaque]);
    fontAntialiasingState.setup([WAKWindow hasLandscapeOrientation]);

    CGSRegionObj drawRegion = (CGSRegionObj)[layer regionBeingDrawn];
    CGFloat contentsScale = [layer contentsScale];
    if (drawRegion && shouldRepaintInPieces(dirtyRect, drawRegion, contentsScale)) {
        // Use fine grained repaint rectangles to minimize the amount of painted pixels.
        CGSRegionEnumeratorObj enumerator = CGSRegionEnumerator(drawRegion);
        CGRect *subRect;
        while ((subRect = CGSNextRect(enumerator))) {
            CGRect adjustedSubRect = *subRect;
            adjustedSubRect.origin.x /= contentsScale;
            adjustedSubRect.origin.y = frame.size.height - (adjustedSubRect.origin.y + adjustedSubRect.size.height) / contentsScale;
            adjustedSubRect.size.width /= contentsScale;
            adjustedSubRect.size.height /= contentsScale;

            CGRect subRectInSuper = [hostLayer() convertRect:adjustedSubRect fromLayer:layer];
            [m_window displayRect:subRectInSuper];
        }
        CGSReleaseRegionEnumerator(enumerator);
    } else {
        // Simple repaint
        CGRect dirtyRectInSuper = [hostLayer() convertRect:dirtyRect fromLayer:layer];
        [m_window displayRect:dirtyRectInSuper];
    }

    fontAntialiasingState.restore();
}

void TileCache::drawLayer(TileLayer* layer, CGContextRef context)
{
    // The web lock unlock observer runs after CA commit observer.
    if (!WebThreadIsLockedOrDisabled()) {
        LOG_ERROR("Drawing without holding the web thread lock");
        ASSERT_NOT_REACHED();
    }

    WKSetCurrentGraphicsContext(context);

    CGRect dirtyRect = CGContextGetClipBoundingBox(context);
    CGRect frame = [layer frame];
    CGContextTranslateCTM(context, -frame.origin.x, -frame.origin.y);
    CGRect scaledFrame = [hostLayer() convertRect:[layer bounds] fromLayer:layer];
    CGContextScaleCTM(context, frame.size.width / scaledFrame.size.width, frame.size.height / scaledFrame.size.height);

    if (RetainPtr<CGImage> contentReplacementImage = this->contentReplacementImage())
        drawReplacementImage(layer, context, contentReplacementImage.get());
    else
        drawWindowContent(layer, context, dirtyRect);

    ++layer.paintCount;
    if (m_tilePaintCountersVisible) {
        char text[16];
        snprintf(text, sizeof(text), "%d", layer.paintCount);

        CGContextSaveGState(context);

        CGContextTranslateCTM(context, frame.origin.x, frame.origin.y);
        CGContextSetFillColorWithColor(context, cachedCGColor(colorForGridTileBorder([layer tileGrid]), ColorSpaceDeviceRGB));
        
        CGRect labelBounds = [layer bounds];
        labelBounds.size.width = 10 + 12 * strlen(text);
        labelBounds.size.height = 25;
        CGContextFillRect(context, labelBounds);

        if (acceleratedDrawingEnabled())
            CGContextSetRGBFillColor(context, 1, 0, 0, 0.4f);
        else
            CGContextSetRGBFillColor(context, 1, 1, 1, 0.6f);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGContextSelectFont(context, "Helvetica", 25, kCGEncodingMacRoman);
        CGContextShowTextAtPoint(context, labelBounds.origin.x + 3, labelBounds.origin.y + 20, text, strlen(text));
#pragma clang diagnostic pop
    
        CGContextRestoreGState(context);        
    }

    WAKView* view = [m_window contentView];
    [view performSelector:@selector(_dispatchTileDidDraw:) withObject:layer afterDelay:0.0];
}

void TileCache::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}

void TileCache::scheduleLayerFlushForPendingRepaint()
{
    WAKView* view = [m_window contentView];
    [view _scheduleLayerFlushForPendingTileCacheRepaint];
}

void TileCache::setNeedsDisplayInRect(const IntRect& dirtyRect)
{
    MutexLocker locker(m_savedDisplayRectMutex);
    bool addedFirstRect = m_savedDisplayRects.isEmpty();
    m_savedDisplayRects.append(dirtyRect);
    if (!addedFirstRect)
        return;
    // Compositing layer flush will call back to doPendingRepaints(). The flush may be throttled and not happen immediately.
    scheduleLayerFlushForPendingRepaint();
}

void TileCache::invalidateTiles(const IntRect& dirtyRect)
{
    ASSERT(!m_tileMutex.tryLock());

    TileGrid* activeGrid = activeTileGrid();
    if (!keepsZoomedOutTiles()) {
        activeGrid->invalidateTiles(dirtyRect);
        return;
    }
    FloatRect scaledRect = dirtyRect;
    scaledRect.scale(zoomedOutScale() / currentScale());
    IntRect zoomedOutDirtyRect = enclosingIntRect(scaledRect);
    if (activeGrid == m_zoomedOutTileGrid) {
        bool dummy;
        IntRect coverRect = m_zoomedOutTileGrid->calculateCoverRect(m_zoomedOutTileGrid->visibleRect(), dummy);
        // Instead of repainting a tile outside the cover rect, just remove it.
        m_zoomedOutTileGrid->dropTilesBetweenRects(zoomedOutDirtyRect, coverRect);
        m_zoomedOutTileGrid->invalidateTiles(zoomedOutDirtyRect);
        // We need to invalidate zoomed in tiles as well while zooming, since
        // we could switch back to the zoomed in grid without dropping its
        // tiles.  See <rdar://problem/9946759>.
        if (m_tilingMode == Zooming && m_zoomedInTileGrid)
            m_zoomedInTileGrid->invalidateTiles(dirtyRect);
        return;
    }
    if (!m_zoomedInTileGrid->hasTiles()) {
        // If no tiles have been created yet for the zoomed in grid, we can't drop the zoomed out tiles.
        m_zoomedOutTileGrid->invalidateTiles(zoomedOutDirtyRect);
        return;
    }
    m_zoomedOutTileGrid->dropTilesIntersectingRect(zoomedOutDirtyRect);
    m_zoomedInTileGrid->invalidateTiles(dirtyRect);
}
    
bool TileCache::isTileCreationSuspended() const 
{
    return (!keepsZoomedOutTiles() && m_tilingMode == Zooming) || m_tilingMode == Disabled;
}

bool TileCache::isTileInvalidationSuspended() const 
{ 
    return m_tilingMode == Zooming || m_tilingMode == Panning || m_tilingMode == ScrollToTop || m_tilingMode == Disabled; 
}

void TileCache::updateTilingMode()
{
    ASSERT(WebThreadIsCurrent() || !WebThreadIsEnabled());

    WAKView* view = [m_window contentView];

    if (m_tilingMode == Zooming || m_tilingMode == Panning || m_tilingMode == ScrollToTop) {
        if (!m_didCallWillStartScrollingOrZooming) {
            [view _willStartScrollingOrZooming];
            m_didCallWillStartScrollingOrZooming = true;
        }
    } else {
        if (m_didCallWillStartScrollingOrZooming) {
            [view _didFinishScrollingOrZooming];
            m_didCallWillStartScrollingOrZooming = false;
        }
        if (m_tilingMode == Disabled)
            return;

        MutexLocker locker(m_tileMutex);
        createTilesInActiveGrid(CoverVisibleOnly);

        if (!m_savedDisplayRects.isEmpty())
            scheduleLayerFlushForPendingRepaint();
    }
}

void TileCache::setTilingMode(TilingMode tilingMode)
{
    if (tilingMode == m_tilingMode)
        return;
    bool wasZooming = (m_tilingMode == Zooming);
    m_tilingMode = tilingMode;

    if ((m_pendingZoomedOutScale || m_pendingScale) && m_tilingMode != Disabled)
        commitScaleChange();
    else if (wasZooming) {
        MutexLocker locker(m_tileMutex);
        bringActiveTileGridToFront();
    }

    if (m_hasPendingUpdateTilingMode)
        return;
    m_hasPendingUpdateTilingMode = true;

    TileCacheTombstone *tombstone = m_tombstone.get();
    WebThreadRun(^{
        if ([tombstone isDead])
            return;
        m_hasPendingUpdateTilingMode = false;
        updateTilingMode();
    });
}

void TileCache::setTilingDirection(TilingDirection tilingDirection)
{
    m_tilingDirection = tilingDirection;
}

TileCache::TilingDirection TileCache::tilingDirection() const
{
    return m_tilingDirection;
}
    
float TileCache::zoomedOutScale() const
{
    return m_zoomedOutTileGrid->scale();
}

float TileCache::currentScale() const
{
    return m_currentScale;
}

void TileCache::doPendingRepaints()
{
    if (m_savedDisplayRects.isEmpty())
        return;
    if (isTileInvalidationSuspended())
        return;
    MutexLocker locker(m_tileMutex);
    flushSavedDisplayRects();
}

void TileCache::flushSavedDisplayRects()
{
    ASSERT(!m_tileMutex.tryLock());
    ASSERT(!m_savedDisplayRects.isEmpty());

    Vector<IntRect> rects;
    {
        MutexLocker locker(m_savedDisplayRectMutex);
        m_savedDisplayRects.swap(rects);
    }
    size_t size = rects.size();
    for (size_t n = 0; n < size; ++n)
        invalidateTiles(rects[n]);
}

void TileCache::setSpeculativeTileCreationEnabled(bool enabled)
{
    if (m_isSpeculativeTileCreationEnabled == enabled)
        return;
    m_isSpeculativeTileCreationEnabled = enabled;
    if (m_isSpeculativeTileCreationEnabled)
        m_tileCreationTimer.startOneShot(0);
}

bool TileCache::hasPendingDraw() const
{
    return !m_savedDisplayRects.isEmpty();
}

void TileCache::prepareToDraw()
{
    // This will trigger document relayout if needed.
    [[m_window contentView] viewWillDraw];

    if (!m_savedDisplayRects.isEmpty()) {
        MutexLocker locker(m_tileMutex);
        flushSavedDisplayRects();
    }
}

void TileCache::setLayerPoolCapacity(unsigned capacity)
{
    TileLayerPool::sharedPool()->setCapacity(capacity);
}

void TileCache::drainLayerPool()
{
    TileLayerPool::sharedPool()->drain();
}

void TileCache::dumpTiles()
{
    NSLog(@"=================");
    NSLog(@"ZOOMED OUT");
    if (m_zoomedOutTileGrid == activeTileGrid())
        NSLog(@"<ACTIVE>");
    m_zoomedOutTileGrid->dumpTiles();
    NSLog(@"=================");
    if (m_zoomedInTileGrid) {
        NSLog(@"ZOOMED IN");
        if (m_zoomedInTileGrid == activeTileGrid())
            NSLog(@"<ACTIVE>");
        m_zoomedInTileGrid->dumpTiles();
        NSLog(@"=================");
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS)
