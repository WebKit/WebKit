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

#if USE(CG)

#include "IntRect.h"
#include "Logging.h"
#include "PlatformCALayer.h"
#include "Region.h"
#include "TileCoverageMap.h"
#include "TileGrid.h"
#include <utility>
#include <wtf/MainThread.h>
#include <wtf/text/TextStream.h>

#if HAVE(IOSURFACE)
#include "IOSurface.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "TileControllerMemoryHandlerIOS.h"
#include <wtf/MemoryPressureHandler.h>
#endif

namespace WebCore {

static const Seconds tileSizeUpdateDelay { 500_ms };

String TileController::tileGridContainerLayerName()
{
    return "TileGrid container"_s;
}

String TileController::zoomedOutTileGridContainerLayerName()
{
    return "Zoomed-out TileGrid container"_s;
}

TileController::TileController(PlatformCALayer* rootPlatformLayer)
    : m_tileCacheLayer(rootPlatformLayer)
    , m_deviceScaleFactor(owningGraphicsLayer()->platformCALayerDeviceScaleFactor())
    , m_tileGrid(std::make_unique<TileGrid>(*this))
    , m_tileRevalidationTimer(*this, &TileController::tileRevalidationTimerFired)
    , m_tileSizeChangeTimer(*this, &TileController::tileSizeChangeTimerFired, tileSizeUpdateDelay)
    , m_marginEdges(false, false, false, false)
{
}

TileController::~TileController()
{
    ASSERT(isMainThread());

#if PLATFORM(IOS_FAMILY)
    tileControllerMemoryHandler().removeTileController(this);
#endif
}

void TileController::tileCacheLayerBoundsChanged()
{
    ASSERT(owningGraphicsLayer()->isCommittingChanges());
    setNeedsRevalidateTiles();
    notePendingTileSizeChange();
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

void TileController::setWantsDeepColorBackingStore(bool wantsDeepColorBackingStore)
{
    if (m_wantsDeepColorBackingStore == wantsDeepColorBackingStore)
        return;

    m_wantsDeepColorBackingStore = wantsDeepColorBackingStore;
    tileGrid().updateTileLayerProperties();
}

void TileController::setSupportsSubpixelAntialiasedText(bool supportsSubpixelAntialiasedText)
{
    if (m_supportsSubpixelAntialiasedText == supportsSubpixelAntialiasedText)
        return;

    m_supportsSubpixelAntialiasedText = supportsSubpixelAntialiasedText;
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
    if (rect == m_visibleRect)
        return;

    m_visibleRect = rect;
    updateTileCoverageMap();
}

void TileController::setLayoutViewportRect(std::optional<FloatRect> rect)
{
    if (rect == m_layoutViewportRect)
        return;

    m_layoutViewportRect = rect;
    updateTileCoverageMap();
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

void TileController::setScrollability(Scrollability scrollability)
{
    if (scrollability == m_scrollability)
        return;
    
    m_scrollability = scrollability;
    notePendingTileSizeChange();
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
    updateTileCoverageMap();
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
        const Seconds tileRevalidationTimeout = 4_s;
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

void TileController::setTileSizeUpdateDelayDisabledForTesting(bool value)
{
    m_isTileSizeUpdateDelayDisabledForTesting = value;
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

#if !PLATFORM(IOS_FAMILY)
// Return 'rect' padded evenly on all sides to achieve 'newSize', but make the padding uneven to contain within constrainingRect.
static FloatRect expandRectWithinRect(const FloatRect& rect, const FloatSize& newSize, const FloatRect& constrainingRect)
{
    ASSERT(newSize.width() >= rect.width() && newSize.height() >= rect.height());

    FloatSize extraSize = newSize - rect.size();
    
    FloatRect expandedRect = rect;
    expandedRect.inflateX(extraSize.width() / 2);
    expandedRect.inflateY(extraSize.height() / 2);

    if (expandedRect.x() < constrainingRect.x())
        expandedRect.setX(constrainingRect.x());
    else if (expandedRect.maxX() > constrainingRect.maxX())
        expandedRect.setX(constrainingRect.maxX() - expandedRect.width());
    
    if (expandedRect.y() < constrainingRect.y())
        expandedRect.setY(constrainingRect.y());
    else if (expandedRect.maxY() > constrainingRect.maxY())
        expandedRect.setY(constrainingRect.maxY() - expandedRect.height());
    
    return intersection(expandedRect, constrainingRect);
}
#endif

void TileController::adjustTileCoverageRect(FloatRect& coverageRect, const FloatSize& newSize, const FloatRect& previousVisibleRect, const FloatRect& visibleRect, float contentsScale) const
{
    // If the page is not in a window (for example if it's in a background tab), we limit the tile coverage rect to the visible rect.
    if (!m_isInWindow) {
        coverageRect = visibleRect;
        return;
    }

#if PLATFORM(IOS_FAMILY)
    // FIXME: unify the iOS and Mac code.
    UNUSED_PARAM(previousVisibleRect);
    
    if (m_tileCoverage == CoverageForVisibleArea || MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        coverageRect = visibleRect;
        return;
    }

    double horizontalMargin = kDefaultTileSize / contentsScale;
    double verticalMargin = kDefaultTileSize / contentsScale;

    MonotonicTime currentTime = MonotonicTime::now();
    Seconds timeDelta = currentTime - m_velocity.lastUpdateTime;

    FloatRect futureRect = visibleRect;
    futureRect.setLocation(FloatPoint(
        futureRect.location().x() + timeDelta.value() * m_velocity.horizontalVelocity,
        futureRect.location().y() + timeDelta.value() * m_velocity.verticalVelocity));

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
        if (m_velocity.scaleChangeRate > 0) {
            coverageRect = visibleRect;
            return;
        }
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

    coverageRect.unite(futureRect);
    return;
#else
    UNUSED_PARAM(contentsScale);

    // FIXME: look at how far the document can scroll in each dimension.
    FloatSize coverageSize = visibleRect.size();

    bool largeVisibleRectChange = !previousVisibleRect.isEmpty() && !visibleRect.intersects(previousVisibleRect);

    // Inflate the coverage rect so that it covers 2x of the visible width and 3x of the visible height.
    // These values were chosen because it's more common to have tall pages and to scroll vertically,
    // so we keep more tiles above and below the current area.
    float widthScale = 1;
    float heightScale = 1;

    if (m_tileCoverage & CoverageForHorizontalScrolling && !largeVisibleRectChange)
        widthScale = 2;

    if (m_tileCoverage & CoverageForVerticalScrolling && !largeVisibleRectChange)
        heightScale = 3;
    
    coverageSize.scale(widthScale, heightScale);

    FloatRect coverageBounds = boundsForSize(newSize);
    
    FloatRect coverage = expandRectWithinRect(visibleRect, coverageSize, coverageBounds);
    LOG_WITH_STREAM(Tiling, stream << "TileController::computeTileCoverageRect newSize=" << newSize << " mode " << m_tileCoverage << " expanded to " << coverageSize << " bounds with margin " << coverageBounds << " coverage " << coverage);
    coverageRect.unite(coverage);
#endif
}

void TileController::scheduleTileRevalidation(Seconds interval)
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

void TileController::willStartLiveResize()
{
    m_inLiveResize = true;
}

void TileController::didEndLiveResize()
{
    m_inLiveResize = false;
    m_tileSizeLocked = false; // Let the end of a live resize update the tiles.
}

void TileController::notePendingTileSizeChange()
{
    if (m_isTileSizeUpdateDelayDisabledForTesting)
        tileSizeChangeTimerFired();
    else
        m_tileSizeChangeTimer.restart();
}

void TileController::tileSizeChangeTimerFired()
{
    if (!owningGraphicsLayer())
        return;

    m_tileSizeLocked = false;
    setNeedsRevalidateTiles();
}

IntSize TileController::tileSize() const
{
    if (m_inLiveResize || m_tileSizeLocked)
        return tileGrid().tileSize();

    const int kLowestCommonDenominatorMaxTileSize = 4 * 1024;
    IntSize maxTileSize(kLowestCommonDenominatorMaxTileSize, kLowestCommonDenominatorMaxTileSize);

#if HAVE(IOSURFACE)
    IntSize surfaceSizeLimit = IOSurface::maximumSize();
    surfaceSizeLimit.scale(1 / m_deviceScaleFactor);
    maxTileSize = maxTileSize.shrunkTo(surfaceSizeLimit);
#endif
    
    if (owningGraphicsLayer()->platformCALayerUseGiantTiles())
        return maxTileSize;

    IntSize tileSize(kDefaultTileSize, kDefaultTileSize);

    if (m_scrollability == NotScrollable) {
        IntSize scaledSize = expandedIntSize(boundsWithoutMargin().size() * tileGrid().scale());
        tileSize = scaledSize.constrainedBetween(IntSize(kDefaultTileSize, kDefaultTileSize), maxTileSize);
    } else if (m_scrollability == VerticallyScrollable)
        tileSize.setWidth(std::min(std::max<int>(ceilf(boundsWithoutMargin().width() * tileGrid().scale()), kDefaultTileSize), maxTileSize.width()));

    LOG_WITH_STREAM(Scrolling, stream << "TileController::tileSize newSize=" << tileSize);

    m_tileSizeLocked = true;
    return tileSize;
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
        m_coverageMap->setNeedsUpdate();
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

void TileController::setHasMargins(bool marginTop, bool marginBottom, bool marginLeft, bool marginRight)
{
    RectEdges<bool> marginEdges(marginTop, marginRight, marginBottom, marginLeft);
    if (marginEdges == m_marginEdges)
        return;
    
    m_marginEdges = marginEdges;
    setNeedsRevalidateTiles();
}

void TileController::setMarginSize(int marginSize)
{
    if (marginSize == m_marginSize)
        return;
    
    m_marginSize = marginSize;
    setNeedsRevalidateTiles();
}

bool TileController::hasMargins() const
{
    return m_marginSize && (m_marginEdges.top() || m_marginEdges.bottom() || m_marginEdges.left() || m_marginEdges.right());
}

bool TileController::hasHorizontalMargins() const
{
    return m_marginSize && (m_marginEdges.left() || m_marginEdges.right());
}

bool TileController::hasVerticalMargins() const
{
    return m_marginSize && (m_marginEdges.top() || m_marginEdges.bottom());
}

int TileController::topMarginHeight() const
{
    return (m_marginSize * m_marginEdges.top()) / tileGrid().scale();
}

int TileController::bottomMarginHeight() const
{
    return (m_marginSize * m_marginEdges.bottom()) / tileGrid().scale();
}

int TileController::leftMarginWidth() const
{
    return (m_marginSize * m_marginEdges.left()) / tileGrid().scale();
}

int TileController::rightMarginWidth() const
{
    return (m_marginSize * m_marginEdges.right()) / tileGrid().scale();
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

    StringBuilder nameBuilder;
    nameBuilder.append("tile at ");
    nameBuilder.appendNumber(tileRect.location().x());
    nameBuilder.append(',');
    nameBuilder.appendNumber(tileRect.location().y());
    layer->setName(nameBuilder.toString());

    float temporaryScaleFactor = owningGraphicsLayer()->platformCALayerContentsScaleMultiplierForNewTiles(m_tileCacheLayer);
    m_hasTilesWithTemporaryScaleFactor |= temporaryScaleFactor != 1;

    layer->setContentsScale(m_deviceScaleFactor * temporaryScaleFactor);
    layer->setAcceleratesDrawing(m_acceleratesDrawing);
    layer->setWantsDeepColorBackingStore(m_wantsDeepColorBackingStore);
    layer->setSupportsSubpixelAntialiasedText(m_supportsSubpixelAntialiasedText);

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

#if PLATFORM(IOS_FAMILY)
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

void TileController::logFilledVisibleFreshTile(unsigned blankPixelCount)
{
    owningGraphicsLayer()->platformCALayerLogFilledVisibleFreshTile(blankPixelCount);
}

} // namespace WebCore

#endif
