/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#import "TileController.h"

#import "GraphicsContext.h"
#import "IntRect.h"
#import "PlatformCALayer.h"
#import "Region.h"
#import "TileGrid.h"
#if !PLATFORM(IOS)
#import "LayerPool.h"
#endif
#import "WebLayer.h"
#import <wtf/MainThread.h>
#import <utility>

#if PLATFORM(IOS)
#import "TileControllerMemoryHandlerIOS.h"
#endif

namespace WebCore {

enum TileValidationPolicyFlag {
    PruneSecondaryTiles = 1 << 0,
    UnparentAllTiles = 1 << 1
};

PassOwnPtr<TileController> TileController::create(PlatformCALayer* rootPlatformLayer)
{
    return adoptPtr(new TileController(rootPlatformLayer));
}

TileController::TileController(PlatformCALayer* rootPlatformLayer)
    : m_tileCacheLayer(rootPlatformLayer)
    , m_tileGrid(std::make_unique<TileGrid>(*this))
    , m_tileSize(defaultTileWidth, defaultTileHeight)
    , m_exposedRect(FloatRect::infiniteRect())
    , m_tileRevalidationTimer(this, &TileController::tileRevalidationTimerFired)
    , m_contentsScale(1)
    , m_deviceScaleFactor(1)
    , m_tileCoverage(CoverageForVisibleArea)
    , m_marginTop(0)
    , m_marginBottom(0)
    , m_marginLeft(0)
    , m_marginRight(0)
    , m_isInWindow(false)
    , m_scrollingPerformanceLoggingEnabled(false)
    , m_unparentsOffscreenTiles(false)
    , m_acceleratesDrawing(false)
    , m_tilesAreOpaque(false)
    , m_hasTilesWithTemporaryScaleFactor(false)
    , m_tileDebugBorderWidth(0)
    , m_indicatorMode(AsyncScrollingIndication)
{
}

TileController::~TileController()
{
    ASSERT(isMainThread());

#if PLATFORM(IOS)
    tileControllerMemoryHandler().removeTileController(this);
#endif

    if (m_tiledScrollingIndicatorLayer)
        m_tiledScrollingIndicatorLayer->setOwner(nullptr);
}

void TileController::tileCacheLayerBoundsChanged()
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    setNeedsRevalidateTiles();
}

void TileController::setNeedsDisplay()
{
    tileGrid().setNeedsDisplay();
}

void TileController::setNeedsDisplayInRect(const IntRect& rect)
{
    tileGrid().setNeedsDisplayInRect(rect);
}

void TileController::platformCALayerPaintContents(PlatformCALayer* platformCALayer, GraphicsContext& context, const FloatRect&)
{
#if PLATFORM(IOS)
    if (pthread_main_np())
        WebThreadLock();
#endif

    if (platformCALayer == m_tiledScrollingIndicatorLayer.get()) {
        tileGrid().drawTileMapContents(context.platformContext(), m_tiledScrollingIndicatorLayer->bounds());
        return;
    }

    {
        GraphicsContextStateSaver stateSaver(context);

        FloatPoint3D layerOrigin = platformCALayer->position();
        context.translate(-layerOrigin.x(), -layerOrigin.y());
        context.scale(FloatSize(tileGrid().scale(), tileGrid().scale()));

        RepaintRectList dirtyRects = collectRectsToPaint(context.platformContext(), platformCALayer);
        drawLayerContents(context.platformContext(), m_tileCacheLayer, dirtyRects);
    }

    int repaintCount = platformCALayerIncrementRepaintCount(platformCALayer);
    if (owningGraphicsLayer()->platformCALayerShowRepaintCounter(0))
        drawRepaintIndicator(context.platformContext(), platformCALayer, repaintCount, cachedCGColor(m_tileDebugBorderColor, ColorSpaceDeviceRGB));

    if (scrollingPerformanceLoggingEnabled()) {
        FloatRect visiblePart(platformCALayer->position().x(), platformCALayer->position().y(), platformCALayer->bounds().size().width(), platformCALayer->bounds().size().height());
        visiblePart.intersect(visibleRect());

        if (repaintCount == 1 && !visiblePart.isEmpty())
            WTFLogAlways("SCROLLING: Filled visible fresh tile. Time: %f Unfilled Pixels: %u\n", WTF::monotonicallyIncreasingTime(), blankPixelCount());
    }
}

float TileController::platformCALayerDeviceScaleFactor() const
{
    return owningGraphicsLayer()->platformCALayerDeviceScaleFactor();
}

bool TileController::platformCALayerShowDebugBorders() const
{
    return owningGraphicsLayer()->platformCALayerShowDebugBorders();
}

bool TileController::platformCALayerShowRepaintCounter(PlatformCALayer*) const
{
    return owningGraphicsLayer()->platformCALayerShowRepaintCounter(0);
}

void TileController::setContentsScale(float scale)
{
    m_contentsScale = scale;
    
    ASSERT(owningGraphicsLayer()->isCommittingChanges());

    float deviceScaleFactor = platformCALayerDeviceScaleFactor();

    // The scale we get is the product of the page scale factor and device scale factor.
    // Divide by the device scale factor so we'll get the page scale factor.
    scale /= deviceScaleFactor;

    if (tileGrid().scale() == scale && m_deviceScaleFactor == deviceScaleFactor && !m_hasTilesWithTemporaryScaleFactor)
        return;

    m_hasTilesWithTemporaryScaleFactor = false;
    m_deviceScaleFactor = deviceScaleFactor;

    tileGrid().setScale(scale);
}

void TileController::setAcceleratesDrawing(bool acceleratesDrawing)
{
    if (m_acceleratesDrawing == acceleratesDrawing)
        return;
    m_acceleratesDrawing = acceleratesDrawing;

    tileGrid().updateTilerLayerProperties();
}

void TileController::setTilesOpaque(bool opaque)
{
    if (opaque == m_tilesAreOpaque)
        return;
    m_tilesAreOpaque = opaque;

    tileGrid().updateTilerLayerProperties();
}

void TileController::setVisibleRect(const FloatRect& visibleRect)
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    if (m_visibleRect == visibleRect)
        return;

    m_visibleRect = visibleRect;
    setNeedsRevalidateTiles();
}

bool TileController::tilesWouldChangeForVisibleRect(const FloatRect& newVisibleRect) const
{
    if (bounds().isEmpty())
        return false;
    return tileGrid().tilesWouldChangeForVisibleRect(newVisibleRect, m_visibleRect);
}

void TileController::setExposedRect(const FloatRect& exposedRect)
{
    if (m_exposedRect == exposedRect)
        return;

    m_exposedRect = exposedRect;
    setNeedsRevalidateTiles();
}

void TileController::prepopulateRect(const FloatRect& rect)
{
    if (tileGrid().prepopulateRect(rect))
        setNeedsRevalidateTiles();
}

void TileController::setIsInWindow(bool isInWindow)
{
    if (m_isInWindow == isInWindow)
        return;

    m_isInWindow = isInWindow;

    if (m_isInWindow)
        setNeedsRevalidateTiles();
    else {
        const double tileRevalidationTimeout = 4;
        scheduleTileRevalidation(tileRevalidationTimeout);
    }
}

void TileController::setTileCoverage(TileCoverage coverage)
{
    if (coverage == m_tileCoverage)
        return;

    m_tileCoverage = coverage;
    setNeedsRevalidateTiles();
}

void TileController::revalidateTiles()
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    tileGrid().revalidateTiles(0);
    m_visibleRectAtLastRevalidate = m_visibleRect;
}

void TileController::forceRepaint()
{
    setNeedsDisplay();
}

void TileController::setTileDebugBorderWidth(float borderWidth)
{
    if (m_tileDebugBorderWidth == borderWidth)
        return;
    m_tileDebugBorderWidth = borderWidth;

    tileGrid().updateTilerLayerProperties();
}

void TileController::setTileDebugBorderColor(Color borderColor)
{
    if (m_tileDebugBorderColor == borderColor)
        return;
    m_tileDebugBorderColor = borderColor;

    tileGrid().updateTilerLayerProperties();
}

IntRect TileController::bounds() const
{
    IntPoint boundsOriginIncludingMargin(-leftMarginWidth(), -topMarginHeight());
    IntSize boundsSizeIncludingMargin = expandedIntSize(m_tileCacheLayer->bounds().size());
    boundsSizeIncludingMargin.expand(leftMarginWidth() + rightMarginWidth(), topMarginHeight() + bottomMarginHeight());

    return IntRect(boundsOriginIncludingMargin, boundsSizeIncludingMargin);
}

IntRect TileController::boundsWithoutMargin() const
{
    return IntRect(IntPoint(), expandedIntSize(m_tileCacheLayer->bounds().size()));
}

IntRect TileController::boundsAtLastRevalidateWithoutMargin() const
{
    IntRect boundsWithoutMargin = IntRect(IntPoint(), m_boundsAtLastRevalidate.size());
    boundsWithoutMargin.contract(IntSize(leftMarginWidth() + rightMarginWidth(), topMarginHeight() + bottomMarginHeight()));
    return boundsWithoutMargin;
}

FloatRect TileController::computeTileCoverageRect(const FloatRect& previousVisibleRect, const FloatRect& visibleRect) const
{
    // If the page is not in a window (for example if it's in a background tab), we limit the tile coverage rect to the visible rect.
    if (!m_isInWindow)
        return visibleRect;

    // FIXME: look at how far the document can scroll in each dimension.
    float coverageHorizontalSize = visibleRect.width();
    float coverageVerticalSize = visibleRect.height();

#if PLATFORM(IOS)
    UNUSED_PARAM(previousVisibleRect);
    return visibleRect;
#else
    bool largeVisibleRectChange = !previousVisibleRect.isEmpty() && !visibleRect.intersects(previousVisibleRect);

    // Inflate the coverage rect so that it covers 2x of the visible width and 3x of the visible height.
    // These values were chosen because it's more common to have tall pages and to scroll vertically,
    // so we keep more tiles above and below the current area.

    if (m_tileCoverage & CoverageForHorizontalScrolling && !largeVisibleRectChange)
        coverageHorizontalSize *= 2;

    if (m_tileCoverage & CoverageForVerticalScrolling && !largeVisibleRectChange)
        coverageVerticalSize *= 3;
#endif
    coverageVerticalSize += topMarginHeight() + bottomMarginHeight();
    coverageHorizontalSize += leftMarginWidth() + rightMarginWidth();

    FloatRect coverageBounds = bounds();
    float coverageLeft = visibleRect.x() - (coverageHorizontalSize - visibleRect.width()) / 2;
    coverageLeft = std::min(coverageLeft, coverageBounds.maxX() - coverageHorizontalSize);
    coverageLeft = std::max(coverageLeft, coverageBounds.x());

    float coverageTop = visibleRect.y() - (coverageVerticalSize - visibleRect.height()) / 2;
    coverageTop = std::min(coverageTop, coverageBounds.maxY() - coverageVerticalSize);
    coverageTop = std::max(coverageTop, coverageBounds.y());

    return FloatRect(coverageLeft, coverageTop, coverageHorizontalSize, coverageVerticalSize);
}

void TileController::scheduleTileRevalidation(double interval)
{
    if (m_tileRevalidationTimer.isActive() && m_tileRevalidationTimer.nextFireInterval() < interval)
        return;

    m_tileRevalidationTimer.startOneShot(interval);
}

bool TileController::shouldAggressivelyRetainTiles() const
{
    return owningGraphicsLayer()->platformCALayerShouldAggressivelyRetainTiles(m_tileCacheLayer);
}

bool TileController::shouldTemporarilyRetainTileCohorts() const
{
    return owningGraphicsLayer()->platformCALayerShouldTemporarilyRetainTileCohorts(m_tileCacheLayer);
}

void TileController::tileRevalidationTimerFired(Timer<TileController>*)
{
    if (m_isInWindow) {
        setNeedsRevalidateTiles();
        return;
    }

    TileGrid::TileValidationPolicyFlags validationPolicy = (shouldAggressivelyRetainTiles() ? 0 : PruneSecondaryTiles) | UnparentAllTiles;

    tileGrid().revalidateTiles(validationPolicy);
}

void TileController::didRevalidateTiles()
{
    m_visibleRectAtLastRevalidate = visibleRect();
    m_boundsAtLastRevalidate = bounds();

    updateTileCoverageMap();
}

unsigned TileController::blankPixelCount() const
{
    return tileGrid().blankPixelCount();
}

unsigned TileController::blankPixelCountForTiles(const PlatformLayerList& tiles, const FloatRect& visibleRect, const IntPoint& tileTranslation)
{
    Region paintedVisibleTiles;

    for (PlatformLayerList::const_iterator it = tiles.begin(), end = tiles.end(); it != end; ++it) {
        const PlatformLayer* tileLayer = it->get();

        FloatRect visiblePart(CGRectOffset([tileLayer frame], tileTranslation.x(), tileTranslation.y()));
        visiblePart.intersect(visibleRect);

        if (!visiblePart.isEmpty())
            paintedVisibleTiles.unite(enclosingIntRect(visiblePart));
    }

    Region uncoveredRegion(enclosingIntRect(visibleRect));
    uncoveredRegion.subtract(paintedVisibleTiles);

    return uncoveredRegion.totalArea();
}

void TileController::setNeedsRevalidateTiles()
{
    owningGraphicsLayer()->platformCALayerSetNeedsToRevalidateTiles();
}

void TileController::updateTileCoverageMap()
{
    if (!m_tiledScrollingIndicatorLayer)
        return;
    FloatRect containerBounds = bounds();
    FloatRect visibleRect = this->visibleRect();

    visibleRect.intersect(tileGrid().scaledExposedRect());
    visibleRect.contract(4, 4); // Layer is positioned 2px from top and left edges.

    float widthScale = 1;
    float scale = 1;
    if (!containerBounds.isEmpty()) {
        widthScale = std::min<float>(visibleRect.width() / containerBounds.width(), 0.1);
        scale = std::min(widthScale, visibleRect.height() / containerBounds.height());
    }
    
    float indicatorScale = scale * tileGrid().scale();
    FloatRect mapBounds = containerBounds;
    mapBounds.scale(indicatorScale, indicatorScale);

    if (!m_exposedRect.isInfinite())
        m_tiledScrollingIndicatorLayer->setPosition(m_exposedRect.location() + FloatPoint(2, 2));
    else
        m_tiledScrollingIndicatorLayer->setPosition(FloatPoint(2, 2));

    m_tiledScrollingIndicatorLayer->setBounds(mapBounds);
    m_tiledScrollingIndicatorLayer->setNeedsDisplay();

    visibleRect.scale(indicatorScale, indicatorScale);
    visibleRect.expand(2, 2);
    m_visibleRectIndicatorLayer->setPosition(visibleRect.location());
    m_visibleRectIndicatorLayer->setBounds(FloatRect(FloatPoint(), visibleRect.size()));

    Color visibleRectIndicatorColor;
    switch (m_indicatorMode) {
    case SynchronousScrollingBecauseOfStyleIndication:
        visibleRectIndicatorColor = Color(255, 0, 0);
        break;
    case SynchronousScrollingBecauseOfEventHandlersIndication:
        visibleRectIndicatorColor = Color(255, 255, 0);
        break;
    case AsyncScrollingIndication:
        visibleRectIndicatorColor = Color(0, 200, 0);
        break;
    }

    m_visibleRectIndicatorLayer->setBorderColor(visibleRectIndicatorColor);
}

IntRect TileController::tileGridExtent() const
{
    return tileGrid().extent();
}

double TileController::retainedTileBackingStoreMemory() const
{
    return tileGrid().retainedTileBackingStoreMemory();
}

// Return the rect in layer coords, not tile coords.
IntRect TileController::tileCoverageRect() const
{
    return tileGrid().tileCoverageRect();
}

PlatformCALayer* TileController::tiledScrollingIndicatorLayer()
{
    if (!m_tiledScrollingIndicatorLayer) {
        m_tiledScrollingIndicatorLayer = m_tileCacheLayer->createCompatibleLayer(PlatformCALayer::LayerTypeSimpleLayer, this);
        m_tiledScrollingIndicatorLayer->setOpacity(0.75);
        m_tiledScrollingIndicatorLayer->setAnchorPoint(FloatPoint3D());
        m_tiledScrollingIndicatorLayer->setBorderColor(Color::black);
        m_tiledScrollingIndicatorLayer->setBorderWidth(1);
        m_tiledScrollingIndicatorLayer->setPosition(FloatPoint(2, 2));

        m_visibleRectIndicatorLayer = m_tileCacheLayer->createCompatibleLayer(PlatformCALayer::LayerTypeLayer, nullptr);
        m_visibleRectIndicatorLayer->setBorderWidth(2);
        m_visibleRectIndicatorLayer->setAnchorPoint(FloatPoint3D());
        m_visibleRectIndicatorLayer->setBorderColor(Color(255, 0, 0));

        m_tiledScrollingIndicatorLayer->appendSublayer(m_visibleRectIndicatorLayer.get());

        updateTileCoverageMap();
    }

    return m_tiledScrollingIndicatorLayer.get();
}

void TileController::setScrollingModeIndication(ScrollingModeIndication scrollingMode)
{
    if (scrollingMode == m_indicatorMode)
        return;

    m_indicatorMode = scrollingMode;

    updateTileCoverageMap();
}

void TileController::setTileMargins(int marginTop, int marginBottom, int marginLeft, int marginRight)
{
    m_marginTop = marginTop;
    m_marginBottom = marginBottom;
    m_marginLeft = marginLeft;
    m_marginRight = marginRight;

    setNeedsRevalidateTiles();
}

bool TileController::hasMargins() const
{
    return m_marginTop || m_marginBottom || m_marginLeft || m_marginRight;
}

int TileController::topMarginHeight() const
{
    return m_marginTop;
}

int TileController::bottomMarginHeight() const
{
    return m_marginBottom;
}

int TileController::leftMarginWidth() const
{
    return m_marginLeft;
}

int TileController::rightMarginWidth() const
{
    return m_marginRight;
}

RefPtr<PlatformCALayer> TileController::createTileLayer(const IntRect& tileRect)
{
#if PLATFORM(IOS)
    RefPtr<PlatformCALayer> layer;
#else
    RefPtr<PlatformCALayer> layer = LayerPool::sharedPool()->takeLayerWithSize(tileRect.size());
#endif

    if (layer) {
        m_tileRepaintCounts.remove(layer.get());
        layer->setOwner(this);
    } else
        layer = m_tileCacheLayer->createCompatibleLayer(PlatformCALayer::LayerTypeTiledBackingTileLayer, this);

    layer->setAnchorPoint(FloatPoint3D());
    layer->setBounds(FloatRect(FloatPoint(), tileRect.size()));
    layer->setPosition(tileRect.location());
    layer->setBorderColor(m_tileDebugBorderColor);
    layer->setBorderWidth(m_tileDebugBorderWidth);
    layer->setEdgeAntialiasingMask(0);
    layer->setOpaque(m_tilesAreOpaque);
#ifndef NDEBUG
    layer->setName("Tile");
#endif

    float temporaryScaleFactor = owningGraphicsLayer()->platformCALayerContentsScaleMultiplierForNewTiles(m_tileCacheLayer);
    m_hasTilesWithTemporaryScaleFactor |= temporaryScaleFactor != 1;

    layer->setContentsScale(m_deviceScaleFactor * temporaryScaleFactor);
    layer->setAcceleratesDrawing(m_acceleratesDrawing);

    layer->setNeedsDisplay();

    return layer;
}

int TileController::platformCALayerIncrementRepaintCount(PlatformCALayer* platformCALayer)
{
    int repaintCount = 0;

    if (m_tileRepaintCounts.contains(platformCALayer))
        repaintCount = m_tileRepaintCounts.get(platformCALayer);

    m_tileRepaintCounts.set(platformCALayer, ++repaintCount);

    return repaintCount;
}

Vector<RefPtr<PlatformCALayer>> TileController::containerLayers()
{
    Vector<RefPtr<PlatformCALayer>> layerList(1);
    layerList[0] = &tileGrid().containerLayer();
    return layerList;
}
    
#if PLATFORM(IOS)
unsigned TileController::numberOfUnparentedTiles() const
{
    return tileGrid().numberOfUnparentedTiles();
}

void TileController::removeUnparentedTilesNow()
{
    tileGrid().removeUnparentedTilesNow();

    updateTileCoverageMap();
}
#endif

} // namespace WebCore
