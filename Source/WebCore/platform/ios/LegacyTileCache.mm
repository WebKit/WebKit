/*
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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
#import "LegacyTileCache.h"

#if PLATFORM(IOS_FAMILY)

#import "FontAntialiasingStateSaver.h"
#import "LegacyTileGrid.h"
#import "LegacyTileGridTile.h"
#import "LegacyTileLayer.h"
#import "LegacyTileLayerPool.h"
#import "Logging.h"
#import "SystemMemory.h"
#import "WAKWindow.h"
#import "WKGraphics.h"
#import "WebCoreThreadRun.h"
#import <CoreText/CoreText.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/RAMSize.h>

@interface WAKView (WebViewExtras)
- (void)_dispatchTileDidDraw:(CALayer*)tile;
- (void)_willStartScrollingOrZooming;
- (void)_didFinishScrollingOrZooming;
- (void)_dispatchTileDidDraw;
- (void)_scheduleRenderingUpdateForPendingTileCacheRepaint;
@end

@interface LegacyTileCacheTombstone : NSObject {
    BOOL dead;
}
@property(getter=isDead) BOOL dead;

@end

@implementation LegacyTileCacheTombstone

@synthesize dead;

@end

namespace WebCore {

LegacyTileCache::LegacyTileCache(WAKWindow* window)
    : m_window(window)
    , m_tombstone(adoptNS([[LegacyTileCacheTombstone alloc] init]))
    , m_zoomedOutTileGrid(makeUnique<LegacyTileGrid>(*this, m_tileSize))
    , m_tileCreationTimer(*this, &LegacyTileCache::tileCreationTimerFired)
{
    [hostLayer() insertSublayer:m_zoomedOutTileGrid->tileHostLayer() atIndex:0];
    hostLayerSizeChanged();
}

LegacyTileCache::~LegacyTileCache()
{
    [m_tombstone.get() setDead:true];
}

CGFloat LegacyTileCache::screenScale() const
{
    return [m_window screenScale];
}

CALayer* LegacyTileCache::hostLayer() const
{
    return [m_window hostLayer];
}

FloatRect LegacyTileCache::visibleRectInLayer(CALayer *layer) const
{
    if (m_overrideVisibleRect)
        return [layer convertRect:m_overrideVisibleRect.value() fromLayer:hostLayer()];

    return [layer convertRect:[m_window extendedVisibleRect] fromLayer:hostLayer()];
}

bool LegacyTileCache::setOverrideVisibleRect(const FloatRect& rect)
{
    m_overrideVisibleRect = rect;
    auto coveredByExistingTiles = false;
    if (activeTileGrid())
        coveredByExistingTiles = activeTileGrid()->tilesCover(enclosingIntRect(m_overrideVisibleRect.value()));
    return coveredByExistingTiles;
}

bool LegacyTileCache::tilesOpaque() const
{
    return m_tilesOpaque;
}
    
LegacyTileGrid* LegacyTileCache::activeTileGrid() const
{
    if (!m_keepsZoomedOutTiles)
        return m_zoomedOutTileGrid.get();
    if (m_tilingMode == Zooming)
        return m_zoomedOutTileGrid.get();
    if (m_zoomedInTileGrid && m_currentScale == m_zoomedInTileGrid->scale())
        return m_zoomedInTileGrid.get();
    return m_zoomedOutTileGrid.get();
}

LegacyTileGrid* LegacyTileCache::inactiveTileGrid() const
{
    return activeTileGrid() == m_zoomedOutTileGrid.get() ? m_zoomedInTileGrid.get() : m_zoomedOutTileGrid.get();
}

void LegacyTileCache::setTilesOpaque(bool opaque)
{
    if (m_tilesOpaque == opaque)
        return;

    LockHolder locker(m_tileMutex);

    m_tilesOpaque = opaque;
    m_zoomedOutTileGrid->updateTileOpacity();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateTileOpacity();
}

void LegacyTileCache::doLayoutTiles()
{
    if (isTileCreationSuspended())
        return;

    LockHolder locker(m_tileMutex);
    LegacyTileGrid* activeGrid = activeTileGrid();
    // Even though we aren't actually creating tiles in the inactive grid, we
    // still need to drop invalid tiles in response to a layout.
    // See <rdar://problem/9839867>.
    if (LegacyTileGrid* inactiveGrid = inactiveTileGrid())
        inactiveGrid->dropInvalidTiles();
    if (activeGrid->checkDoSingleTileLayout())
        return;
    createTilesInActiveGrid(CoverVisibleOnly);
}

void LegacyTileCache::hostLayerSizeChanged()
{
    m_zoomedOutTileGrid->updateHostLayerSize();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateHostLayerSize();
}

void LegacyTileCache::setKeepsZoomedOutTiles(bool keep)
{
    m_keepsZoomedOutTiles = keep;
}

void LegacyTileCache::setCurrentScale(float scale)
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
        LockHolder locker(m_tileMutex);
        activeTileGrid()->dropAllTiles();
        activeTileGrid()->createTiles(CoverVisibleOnly);
    }
}

void LegacyTileCache::setZoomedOutScale(float scale)
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
    
void LegacyTileCache::commitScaleChange()
{
    ASSERT(m_pendingZoomedOutScale || m_pendingScale);
    ASSERT(m_tilingMode != Disabled);
    
    LockHolder locker(m_tileMutex);

    if (m_pendingZoomedOutScale) {
        m_zoomedOutTileGrid->setScale(m_pendingZoomedOutScale);
        m_pendingZoomedOutScale = 0;
    }
    
    if (!m_keepsZoomedOutTiles) {
        ASSERT(activeTileGrid() == m_zoomedOutTileGrid.get());
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
            m_zoomedInTileGrid = makeUnique<LegacyTileGrid>(*this, m_tileSize);
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

void LegacyTileCache::bringActiveTileGridToFront()
{
    LegacyTileGrid* activeGrid = activeTileGrid();
    LegacyTileGrid* otherGrid = inactiveTileGrid();
    if (!otherGrid)
        return;
    CALayer* frontLayer = activeGrid->tileHostLayer();
    CALayer* otherLayer = otherGrid->tileHostLayer();
    [hostLayer() insertSublayer:frontLayer above:otherLayer];
}
    
void LegacyTileCache::adjustTileGridTransforms()
{
    CALayer* zoomedOutHostLayer = m_zoomedOutTileGrid->tileHostLayer();
    float transformScale = currentScale() / zoomedOutScale();
    [zoomedOutHostLayer setTransform:CATransform3DMakeScale(transformScale, transformScale, 1.0f)];
    m_zoomedOutTileGrid->updateHostLayerSize();
}

void LegacyTileCache::layoutTiles()
{
    if (m_hasPendingLayoutTiles)
        return;
    m_hasPendingLayoutTiles = true;

    LegacyTileCacheTombstone *tombstone = m_tombstone.get();
    WebThreadRun(^{
        if ([tombstone isDead])
            return;
        m_hasPendingLayoutTiles = false;
        doLayoutTiles();
    });
}

void LegacyTileCache::layoutTilesNow()
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

    LockHolder locker(m_tileMutex);
    LegacyTileGrid* activeGrid = activeTileGrid();
    if (activeGrid->checkDoSingleTileLayout()) {
        m_tilingMode = savedTilingMode;
        return;
    }
    createTilesInActiveGrid(CoverVisibleOnly);
    m_tilingMode = savedTilingMode;
}

void LegacyTileCache::layoutTilesNowForRect(const IntRect& rect)
{
    ASSERT(WebThreadIsLockedOrDisabled());
    LockHolder locker(m_tileMutex);

    activeTileGrid()->addTilesCoveringRect(rect);
}

void LegacyTileCache::removeAllNonVisibleTiles()
{
    LockHolder locker(m_tileMutex);
    removeAllNonVisibleTilesInternal();
}

void LegacyTileCache::removeAllNonVisibleTilesInternal()
{
    LegacyTileGrid* activeGrid = activeTileGrid();
    if (keepsZoomedOutTiles() && activeGrid == m_zoomedInTileGrid.get() && activeGrid->hasTiles())
        m_zoomedOutTileGrid->dropAllTiles();

    IntRect activeTileBounds = activeGrid->bounds();
    if (activeTileBounds.width() <= m_tileSize.width() && activeTileBounds.height() <= m_tileSize.height()) {
        // If the view is smaller than a tile, keep the tile even if it is not visible.
        activeGrid->dropTilesOutsideRect(activeTileBounds);
        return;
    }

    activeGrid->dropTilesOutsideRect(activeGrid->visibleRect());
}

void LegacyTileCache::removeAllTiles()
{
    LockHolder locker(m_tileMutex);
    m_zoomedOutTileGrid->dropAllTiles();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->dropAllTiles();
}

void LegacyTileCache::removeForegroundTiles()
{
    LockHolder locker(m_tileMutex);
    if (!keepsZoomedOutTiles())
        m_zoomedOutTileGrid->dropAllTiles();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->dropAllTiles();
}

void LegacyTileCache::setContentReplacementImage(RetainPtr<CGImageRef> contentReplacementImage)
{
    LockHolder locker(m_contentReplacementImageMutex);
    m_contentReplacementImage = contentReplacementImage;
}

RetainPtr<CGImageRef> LegacyTileCache::contentReplacementImage() const
{
    LockHolder locker(m_contentReplacementImageMutex);
    return m_contentReplacementImage;
}

void LegacyTileCache::setTileBordersVisible(bool flag)
{
    if (flag == m_tileBordersVisible)
        return;

    m_tileBordersVisible = flag;
    m_zoomedOutTileGrid->updateTileBorderVisibility();
    if (m_zoomedInTileGrid)
        m_zoomedInTileGrid->updateTileBorderVisibility();
}

void LegacyTileCache::setTilePaintCountersVisible(bool flag)
{
    m_tilePaintCountersVisible = flag;
    // The numbers will show up the next time the tiles paint.
}

void LegacyTileCache::finishedCreatingTiles(bool didCreateTiles, bool createMore)
{
    // We need to ensure that all tiles are showing the same version of the content.
    if (didCreateTiles && !m_savedDisplayRects.isEmpty())
        flushSavedDisplayRects();

    if (keepsZoomedOutTiles()) {
        if (m_zoomedInTileGrid && activeTileGrid() == m_zoomedOutTileGrid.get() && m_tilingMode != Zooming && m_zoomedInTileGrid->hasTiles()) {
            // This CA transaction will cover the screen with top level tiles.
            // We can remove zoomed-in tiles without flashing.
            m_zoomedInTileGrid->dropAllTiles();
        } else if (activeTileGrid() == m_zoomedInTileGrid.get()) {
            // Pass the minimum possible distance to consider all tiles, even visible ones.
            m_zoomedOutTileGrid->dropDistantTiles(0, std::numeric_limits<double>::min());
        }
    }

    // Keep creating tiles until the whole coverRect is covered.
    if (createMore)
        m_tileCreationTimer.startOneShot(0_s);
}

void LegacyTileCache::tileCreationTimerFired()
{
    if (isTileCreationSuspended())
        return;
    LockHolder locker(m_tileMutex);
    createTilesInActiveGrid(CoverSpeculative);
}

void LegacyTileCache::createTilesInActiveGrid(SynchronousTileCreationMode mode)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        LOG(MemoryPressure, "Under memory pressure at: %s", __PRETTY_FUNCTION__);
        removeAllNonVisibleTilesInternal();
    }
    activeTileGrid()->createTiles(mode);
}

unsigned LegacyTileCache::tileCapacityForGrid(LegacyTileGrid* grid)
{
    static unsigned capacity;
    if (!capacity) {
        size_t totalMemory = ramSize() / 1024 / 1024;
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

    if (keepsZoomedOutTiles() && grid == m_zoomedOutTileGrid.get()) {
        if (activeTileGrid() == m_zoomedOutTileGrid.get())
            return gridCapacity;
        return gridCapacity / 4;
    }
    return gridCapacity * 3 / 4;
}

Color LegacyTileCache::colorForGridTileBorder(LegacyTileGrid* grid) const
{
    if (grid == m_zoomedOutTileGrid.get())
        return SRGBA<uint8_t> { 51, 255, 0, 128 };
    return SRGBA<uint8_t> { 51, 230, 0, 128 };
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

void LegacyTileCache::drawReplacementImage(LegacyTileLayer* layer, CGContextRef context, CGImageRef image)
{
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, CGContextGetClipBoundingBox(context));

    CGFloat contentsScale = [layer contentsScale];
    CGContextScaleCTM(context, 1 / contentsScale, -1 / contentsScale);
    CGRect imageRect = CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));
    CGContextTranslateCTM(context, 0, -imageRect.size.height);
    CGContextDrawImage(context, imageRect, image);
}

void LegacyTileCache::drawWindowContent(LegacyTileLayer* layer, CGContextRef context, CGRect dirtyRect, DrawingFlags drawingFlags)
{
    CGRect frame = [layer frame];
    FontAntialiasingStateSaver fontAntialiasingState(context, [m_window useOrientationDependentFontAntialiasing] && [layer isOpaque]);
    fontAntialiasingState.setup([WAKWindow hasLandscapeOrientation]);

    if (drawingFlags == DrawingFlags::Snapshotting)
        [m_window setIsInSnapshottingPaint:YES];
        
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
    
    if (drawingFlags == DrawingFlags::Snapshotting)
        [m_window setIsInSnapshottingPaint:NO];
}

void LegacyTileCache::drawLayer(LegacyTileLayer* layer, CGContextRef context, DrawingFlags drawingFlags)
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
        drawWindowContent(layer, context, dirtyRect, drawingFlags);

    ++layer.paintCount;
    if (m_tilePaintCountersVisible) {
        char text[16];
        snprintf(text, sizeof(text), "%d", layer.paintCount);

        CGContextSaveGState(context);

        CGContextTranslateCTM(context, frame.origin.x, frame.origin.y);
        CGContextSetFillColorWithColor(context, cachedCGColor(colorForGridTileBorder([layer tileGrid])));
        
        CGRect labelBounds = [layer bounds];
        labelBounds.size.width = 10 + 12 * strlen(text);
        labelBounds.size.height = 25;
        CGContextFillRect(context, labelBounds);

        if (acceleratedDrawingEnabled())
            CGContextSetRGBFillColor(context, 1, 0, 0, 0.4f);
        else
            CGContextSetRGBFillColor(context, 1, 1, 1, 0.6f);

        auto matrix = CGAffineTransformMakeScale(1, -1);
        auto font = adoptCF(CTFontCreateWithName(CFSTR("Helvetica"), 25, &matrix));
        CFTypeRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
        CFTypeRef values[] = { font.get(), kCFBooleanTrue };
        auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        auto string = adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(text), strlen(text), kCFStringEncodingUTF8, false, kCFAllocatorNull));
        auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, string.get(), attributes.get()));
        auto line = adoptCF(CTLineCreateWithAttributedString(attributedString.get()));
        CGContextSetTextPosition(context, labelBounds.origin.x + 3, labelBounds.origin.y + 20);
        CTLineDraw(line.get(), context);
    
        CGContextRestoreGState(context);        
    }

    WAKView* view = [m_window contentView];
    [view performSelector:@selector(_dispatchTileDidDraw:) withObject:layer afterDelay:0.0];
}

void LegacyTileCache::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}

void LegacyTileCache::scheduleRenderingUpdateForPendingRepaint()
{
    WAKView* view = [m_window contentView];
    [view _scheduleRenderingUpdateForPendingTileCacheRepaint];
}

void LegacyTileCache::setNeedsDisplayInRect(const IntRect& dirtyRect)
{
    LockHolder locker(m_savedDisplayRectMutex);
    bool addedFirstRect = m_savedDisplayRects.isEmpty();
    m_savedDisplayRects.append(dirtyRect);
    if (!addedFirstRect)
        return;
    // Compositing layer flush will call back to doPendingRepaints(). The flush may be throttled and not happen immediately.
    scheduleRenderingUpdateForPendingRepaint();
}

void LegacyTileCache::invalidateTiles(const IntRect& dirtyRect)
{
    ASSERT(!m_tileMutex.tryLock());

    LegacyTileGrid* activeGrid = activeTileGrid();
    if (!keepsZoomedOutTiles()) {
        activeGrid->invalidateTiles(dirtyRect);
        return;
    }
    FloatRect scaledRect = dirtyRect;
    scaledRect.scale(zoomedOutScale() / currentScale());
    IntRect zoomedOutDirtyRect = enclosingIntRect(scaledRect);
    if (activeGrid == m_zoomedOutTileGrid.get()) {
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
    
bool LegacyTileCache::isTileCreationSuspended() const 
{
    return (!keepsZoomedOutTiles() && m_tilingMode == Zooming) || m_tilingMode == Disabled;
}

bool LegacyTileCache::isTileInvalidationSuspended() const 
{ 
    return m_tilingMode == Zooming || m_tilingMode == Panning || m_tilingMode == ScrollToTop || m_tilingMode == Disabled; 
}

void LegacyTileCache::updateTilingMode()
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

        LockHolder locker(m_tileMutex);
        createTilesInActiveGrid(CoverVisibleOnly);

        if (!m_savedDisplayRects.isEmpty())
            scheduleRenderingUpdateForPendingRepaint();
    }
}

void LegacyTileCache::setTilingMode(TilingMode tilingMode)
{
    if (tilingMode == m_tilingMode)
        return;
    bool wasZooming = (m_tilingMode == Zooming);
    m_tilingMode = tilingMode;

    if ((m_pendingZoomedOutScale || m_pendingScale) && m_tilingMode != Disabled)
        commitScaleChange();
    else if (wasZooming) {
        LockHolder locker(m_tileMutex);
        bringActiveTileGridToFront();
    }

    if (m_hasPendingUpdateTilingMode)
        return;
    m_hasPendingUpdateTilingMode = true;

    LegacyTileCacheTombstone *tombstone = m_tombstone.get();
    WebThreadRun(^{
        if ([tombstone isDead])
            return;
        m_hasPendingUpdateTilingMode = false;
        updateTilingMode();
    });
}

void LegacyTileCache::setTilingDirection(TilingDirection tilingDirection)
{
    m_tilingDirection = tilingDirection;
}

LegacyTileCache::TilingDirection LegacyTileCache::tilingDirection() const
{
    return m_tilingDirection;
}
    
float LegacyTileCache::zoomedOutScale() const
{
    return m_zoomedOutTileGrid->scale();
}

float LegacyTileCache::currentScale() const
{
    return m_currentScale;
}

void LegacyTileCache::doPendingRepaints()
{
    if (m_savedDisplayRects.isEmpty())
        return;
    if (isTileInvalidationSuspended())
        return;
    LockHolder locker(m_tileMutex);
    flushSavedDisplayRects();
}

void LegacyTileCache::flushSavedDisplayRects()
{
    ASSERT(!m_tileMutex.tryLock());
    ASSERT(!m_savedDisplayRects.isEmpty());

    Vector<IntRect> rects;
    {
        LockHolder locker(m_savedDisplayRectMutex);
        m_savedDisplayRects.swap(rects);
    }
    size_t size = rects.size();
    for (size_t n = 0; n < size; ++n)
        invalidateTiles(rects[n]);
}

void LegacyTileCache::setSpeculativeTileCreationEnabled(bool enabled)
{
    if (m_isSpeculativeTileCreationEnabled == enabled)
        return;
    m_isSpeculativeTileCreationEnabled = enabled;
    if (m_isSpeculativeTileCreationEnabled)
        m_tileCreationTimer.startOneShot(0_s);
}

bool LegacyTileCache::hasPendingDraw() const
{
    return !m_savedDisplayRects.isEmpty();
}

void LegacyTileCache::prepareToDraw()
{
    // This will trigger document relayout if needed.
    [[m_window contentView] viewWillDraw];

    if (!m_savedDisplayRects.isEmpty()) {
        LockHolder locker(m_tileMutex);
        flushSavedDisplayRects();
    }
}

void LegacyTileCache::setLayerPoolCapacity(unsigned capacity)
{
    LegacyTileLayerPool::sharedPool()->setCapacity(capacity);
}

void LegacyTileCache::drainLayerPool()
{
    LegacyTileLayerPool::sharedPool()->drain();
}

void LegacyTileCache::dumpTiles()
{
    NSLog(@"=================");
    NSLog(@"ZOOMED OUT");
    if (m_zoomedOutTileGrid.get() == activeTileGrid())
        NSLog(@"<ACTIVE>");
    m_zoomedOutTileGrid->dumpTiles();
    NSLog(@"=================");
    if (m_zoomedInTileGrid) {
        NSLog(@"ZOOMED IN");
        if (m_zoomedInTileGrid.get() == activeTileGrid())
            NSLog(@"<ACTIVE>");
        m_zoomedInTileGrid->dumpTiles();
        NSLog(@"=================");
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
