/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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
#include "TileController.h"

#include "IntRect.h"
#include "PlatformCALayer.h"
#include "Region.h"
#include "TileCoverageMap.h"
#include "TileGrid.h"
#include <utility>
#include <wtf/MainThread.h>

#if PLATFORM(IOS)
#include "MemoryPressureHandler.h"
#include "TileControllerMemoryHandlerIOS.h"
#endif

namespace WebCore {

String TileController::tileGridContainerLayerName()
{
    return ASCIILiteral("TileGrid Container Layer");
}

String TileController::zoomedOutTileGridContainerLayerName()
{
    return ASCIILiteral("Zoomed Out TileGrid Container Layer");
}

TileController::TileController(PlatformCALayer* rootPlatformLayer)
    : m_tileCacheLayer(rootPlatformLayer)
    , m_tileGrid(std::make_unique<TileGrid>(*this))
    , m_tileRevalidationTimer(*this, &TileController::tileRevalidationTimerFired)
    , m_zoomedOutContentsScale(0)
    , m_deviceScaleFactor(owningGraphicsLayer()->platformCALayerDeviceScaleFactor())
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
    , m_indicatorMode(SynchronousScrollingBecauseOfLackOfScrollingCoordinatorIndication)
    , m_topContentInset(0)
{
}

TileController::~TileController()
{
    ASSERT(isMainThread());

#if PLATFORM(IOS)
    tileControllerMemoryHandler().removeTileController(this);
#endif
}

void TileController::tileCacheLayerBoundsChanged()
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    setNeedsRevalidateTiles();
}

void TileController::setNeedsDisplay()
{
    tileGrid().setNeedsDisplay();
    clearZoomedOutTileGrid();
}

void TileController::setNeedsDisplayInRect(const IntRect& rect)
{
    tileGrid().setNeedsDisplayInRect(rect);
    if (m_zoomedOutTileGrid)
        m_zoomedOutTileGrid->dropTilesInRect(rect);
    updateTileCoverageMap();
}

void TileController::setContentsScale(float scale)
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());

    float deviceScaleFactor = owningGraphicsLayer()->platformCALayerDeviceScaleFactor();
    // The scale we get is the product of the page scale factor and device scale factor.
    // Divide by the device scale factor so we'll get the page scale factor.
    scale /= deviceScaleFactor;

    if (tileGrid().scale() == scale && m_deviceScaleFactor == deviceScaleFactor && !m_hasTilesWithTemporaryScaleFactor)
        return;

    m_hasTilesWithTemporaryScaleFactor = false;
    m_deviceScaleFactor = deviceScaleFactor;

    if (m_coverageMap)
        m_coverageMap->setDeviceScaleFactor(deviceScaleFactor);

    if (m_zoomedOutTileGrid && m_zoomedOutTileGrid->scale() == scale) {
        m_tileGrid = WTFMove(m_zoomedOutTileGrid);
        m_tileGrid->setIsZoomedOutTileGrid(false);
        m_tileGrid->revalidateTiles();
        tileGridsChanged();
        return;
    }

    if (m_zoomedOutContentsScale && m_zoomedOutContentsScale == tileGrid().scale() && tileGrid().scale() != scale && !m_hasTilesWithTemporaryScaleFactor) {
        m_zoomedOutTileGrid = WTFMove(m_tileGrid);
        m_zoomedOutTileGrid->setIsZoomedOutTileGrid(true);
        m_tileGrid = std::make_unique<TileGrid>(*this);
        tileGridsChanged();
    }

    tileGrid().setScale(scale);
    tileGrid().setNeedsDisplay();
}

float TileController::contentsScale() const
{
    return tileGrid().scale() * m_deviceScaleFactor;
}

float TileController::zoomedOutContentsScale() const
{
    return m_zoomedOutContentsScale * m_deviceScaleFactor;
}

void TileController::setZoomedOutContentsScale(float scale)
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());

    float deviceScaleFactor = owningGraphicsLayer()->platformCALayerDeviceScaleFactor();
    scale /= deviceScaleFactor;

    if (m_zoomedOutContentsScale == scale)
        return;
    m_zoomedOutContentsScale = scale;

    if (m_zoomedOutTileGrid && m_zoomedOutTileGrid->scale() != m_zoomedOutContentsScale)
        clearZoomedOutTileGrid();
}

void TileController::setAcceleratesDrawing(bool acceleratesDrawing)
{
    if (m_acceleratesDrawing == acceleratesDrawing)
        return;
    m_acceleratesDrawing = acceleratesDrawing;

    tileGrid().updateTileLayerProperties();
}

void TileController::setTilesOpaque(bool opaque)
{
    if (opaque == m_tilesAreOpaque)
        return;
    m_tilesAreOpaque = opaque;

    tileGrid().updateTileLayerProperties();
}

void TileController::setVisibleRect(const FloatRect& rect)
{
    m_visibleRect = rect;
}

void TileController::setCoverageRect(const FloatRect& rect)
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    if (m_coverageRect == rect)
        return;

    m_coverageRect = rect;
    setNeedsRevalidateTiles();
}

bool TileController::tilesWouldChangeForCoverageRect(const FloatRect& rect) const
{
    if (bounds().isEmpty())
        return false;

    return tileGrid().tilesWouldChangeForCoverageRect(rect);
}

void TileController::setVelocity(const VelocityData& velocity)
{
    bool changeAffectsTileCoverage = m_velocity.velocityOrScaleIsChanging() || velocity.velocityOrScaleIsChanging();
    m_velocity = velocity;
    
    if (changeAffectsTileCoverage)
        setNeedsRevalidateTiles();
}

void TileController::setTopContentInset(float topContentInset)
{
    m_topContentInset = topContentInset;
    setTiledScrollingIndicatorPosition(FloatPoint(0, m_topContentInset));
}

void TileController::setTiledScrollingIndicatorPosition(const FloatPoint& position)
{
    if (!m_coverageMap)
        return;
    m_coverageMap->setPosition(position);
    m_coverageMap->update();
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
    tileGrid().revalidateTiles();
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

    tileGrid().updateTileLayerProperties();
}

void TileController::setTileDebugBorderColor(Color borderColor)
{
    if (m_tileDebugBorderColor == borderColor)
        return;
    m_tileDebugBorderColor = borderColor;

    tileGrid().updateTileLayerProperties();
}

IntRect TileController::boundsForSize(const FloatSize& size) const
{
    IntPoint boundsOriginIncludingMargin(-leftMarginWidth(), -topMarginHeight());
    IntSize boundsSizeIncludingMargin = expandedIntSize(size);
    boundsSizeIncludingMargin.expand(leftMarginWidth() + rightMarginWidth(), topMarginHeight() + bottomMarginHeight());

    return IntRect(boundsOriginIncludingMargin, boundsSizeIncludingMargin);
}

IntRect TileController::bounds() const
{
    return boundsForSize(m_tileCacheLayer->bounds().size());
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

FloatRect TileController::computeTileCoverageRect(const FloatSize& newSize, const FloatRect& previousVisibleRect, const FloatRect& visibleRect, float contentsScale) const
{
    // If the page is not in a window (for example if it's in a background tab), we limit the tile coverage rect to the visible rect.
    if (!m_isInWindow)
        return visibleRect;

#if PLATFORM(IOS)
    // FIXME: unify the iOS and Mac code.
    UNUSED_PARAM(previousVisibleRect);
    
    if (m_tileCoverage == CoverageForVisibleArea || MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return visibleRect;

    double horizontalMargin = tileSize().width() / contentsScale;
    double verticalMargin = tileSize().height() / contentsScale;

    double currentTime = monotonicallyIncreasingTime();
    double timeDelta = currentTime - m_velocity.lastUpdateTime;

    FloatRect futureRect = visibleRect;
    futureRect.setLocation(FloatPoint(
        futureRect.location().x() + timeDelta * m_velocity.horizontalVelocity,
        futureRect.location().y() + timeDelta * m_velocity.verticalVelocity));

    if (m_velocity.horizontalVelocity) {
        futureRect.setWidth(futureRect.width() + horizontalMargin);
        if (m_velocity.horizontalVelocity < 0)
            futureRect.setX(futureRect.x() - horizontalMargin);
    }

    if (m_velocity.verticalVelocity) {
        futureRect.setHeight(futureRect.height() + verticalMargin);
        if (m_velocity.verticalVelocity < 0)
            futureRect.setY(futureRect.y() - verticalMargin);
    }

    if (!m_velocity.horizontalVelocity && !m_velocity.verticalVelocity) {
        if (m_velocity.scaleChangeRate > 0)
            return visibleRect;
        futureRect.setWidth(futureRect.width() + horizontalMargin);
        futureRect.setHeight(futureRect.height() + verticalMargin);
        futureRect.setX(futureRect.x() - horizontalMargin / 2);
        futureRect.setY(futureRect.y() - verticalMargin / 2);
    }

    // Can't use m_tileCacheLayer->bounds() here, because the size of the underlying platform layer
    // hasn't been updated for the current commit.
    IntSize contentSize = expandedIntSize(newSize);
    if (futureRect.maxX() > contentSize.width())
        futureRect.setX(contentSize.width() - futureRect.width());
    if (futureRect.maxY() > contentSize.height())
        futureRect.setY(contentSize.height() - futureRect.height());
    if (futureRect.x() < 0)
        futureRect.setX(0);
    if (futureRect.y() < 0)
        futureRect.setY(0);

    return futureRect;
#else
    UNUSED_PARAM(contentsScale);

    // FIXME: look at how far the document can scroll in each dimension.
    float coverageHorizontalSize = visibleRect.width();
    float coverageVerticalSize = visibleRect.height();

    bool largeVisibleRectChange = !previousVisibleRect.isEmpty() && !visibleRect.intersects(previousVisibleRect);

    // Inflate the coverage rect so that it covers 2x of the visible width and 3x of the visible height.
    // These values were chosen because it's more common to have tall pages and to scroll vertically,
    // so we keep more tiles above and below the current area.

    if (m_tileCoverage & CoverageForHorizontalScrolling && !largeVisibleRectChange)
        coverageHorizontalSize *= 2;

    if (m_tileCoverage & CoverageForVerticalScrolling && !largeVisibleRectChange)
        coverageVerticalSize *= 3;

    coverageVerticalSize += topMarginHeight() + bottomMarginHeight();
    coverageHorizontalSize += leftMarginWidth() + rightMarginWidth();

    // Can't use m_tileCacheLayer->bounds() here, because the size of the underlying platform layer
    // hasn't been updated for the current commit.
    FloatRect coverageBounds = boundsForSize(newSize);
    float coverageLeft = visibleRect.x() - (coverageHorizontalSize - visibleRect.width()) / 2;
    coverageLeft = std::min(coverageLeft, coverageBounds.maxX() - coverageHorizontalSize);
    coverageLeft = std::max(coverageLeft, coverageBounds.x());

    float coverageTop = visibleRect.y() - (coverageVerticalSize - visibleRect.height()) / 2;
    coverageTop = std::min(coverageTop, coverageBounds.maxY() - coverageVerticalSize);
    coverageTop = std::max(coverageTop, coverageBounds.y());

    return FloatRect(coverageLeft, coverageTop, coverageHorizontalSize, coverageVerticalSize);
#endif
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

IntSize TileController::tileSize() const
{
    return owningGraphicsLayer()->platformCALayerTileSize();
}

void TileController::clearZoomedOutTileGrid()
{
    m_zoomedOutTileGrid = nullptr;
    tileGridsChanged();
}

void TileController::tileGridsChanged()
{
    return owningGraphicsLayer()->platformCALayerCustomSublayersChanged(m_tileCacheLayer);
}

void TileController::tileRevalidationTimerFired()
{
    if (!owningGraphicsLayer())
        return;

    if (m_isInWindow) {
        setNeedsRevalidateTiles();
        return;
    }
    // If we are not visible get rid of the zoomed-out tiles.
    clearZoomedOutTileGrid();

    TileGrid::TileValidationPolicy validationPolicy = (shouldAggressivelyRetainTiles() ? 0 : TileGrid::PruneSecondaryTiles) | TileGrid::UnparentAllTiles;

    tileGrid().revalidateTiles(validationPolicy);
}

void TileController::didRevalidateTiles()
{
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

        FloatRect visiblePart(CGRectOffset(PlatformCALayer::frameForLayer(tileLayer), tileTranslation.x(), tileTranslation.y()));
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
    if (m_coverageMap)
        m_coverageMap->update();
}

IntRect TileController::tileGridExtent() const
{
    return tileGrid().extent();
}

double TileController::retainedTileBackingStoreMemory() const
{
    double bytes = tileGrid().retainedTileBackingStoreMemory();
    if (m_zoomedOutTileGrid)
        bytes += m_zoomedOutTileGrid->retainedTileBackingStoreMemory();
    return bytes;
}

// Return the rect in layer coords, not tile coords.
IntRect TileController::tileCoverageRect() const
{
    return tileGrid().tileCoverageRect();
}

PlatformCALayer* TileController::tiledScrollingIndicatorLayer()
{
    if (!m_coverageMap)
        m_coverageMap = std::make_unique<TileCoverageMap>(*this);

    return &m_coverageMap->layer();
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

bool TileController::hasHorizontalMargins() const
{
    return m_marginLeft || m_marginRight;
}

bool TileController::hasVerticalMargins() const
{
    return m_marginTop || m_marginBottom;
}

int TileController::topMarginHeight() const
{
    return m_marginTop / tileGrid().scale();
}

int TileController::bottomMarginHeight() const
{
    return m_marginBottom / tileGrid().scale();
}

int TileController::leftMarginWidth() const
{
    return m_marginLeft / tileGrid().scale();
}

int TileController::rightMarginWidth() const
{
    return m_marginRight / tileGrid().scale();
}

RefPtr<PlatformCALayer> TileController::createTileLayer(const IntRect& tileRect, TileGrid& grid)
{
    RefPtr<PlatformCALayer> layer = m_tileCacheLayer->createCompatibleLayerOrTakeFromPool(PlatformCALayer::LayerTypeTiledBackingTileLayer, &grid, tileRect.size());

    layer->setAnchorPoint(FloatPoint3D());
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

Vector<RefPtr<PlatformCALayer>> TileController::containerLayers()
{
    Vector<RefPtr<PlatformCALayer>> layerList;
    if (m_zoomedOutTileGrid)
        layerList.append(&m_zoomedOutTileGrid->containerLayer());
    layerList.append(&tileGrid().containerLayer());
    return layerList;
}

#if PLATFORM(IOS)
unsigned TileController::numberOfUnparentedTiles() const
{
    unsigned count = tileGrid().numberOfUnparentedTiles();
    if (m_zoomedOutTileGrid)
        count += m_zoomedOutTileGrid->numberOfUnparentedTiles();
    return count;
}

void TileController::removeUnparentedTilesNow()
{
    tileGrid().removeUnparentedTilesNow();
    if (m_zoomedOutTileGrid)
        m_zoomedOutTileGrid->removeUnparentedTilesNow();

    updateTileCoverageMap();
}
#endif

} // namespace WebCore
