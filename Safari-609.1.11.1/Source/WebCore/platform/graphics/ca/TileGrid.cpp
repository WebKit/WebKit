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

#if USE(CG)

#include "TileGrid.h"

#include "GraphicsContext.h"
#include "LayerPool.h"
#include "Logging.h"
#include "PlatformCALayer.h"
#include "TileController.h"
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "TileControllerMemoryHandlerIOS.h"
#endif

namespace WebCore {

#if !LOG_DISABLED
static String validationPolicyAsString(TileGrid::TileValidationPolicy validationPolicy)
{
    StringBuilder builder;
    builder.appendLiteral("[");
    if (validationPolicy & TileGrid::PruneSecondaryTiles)
        builder.appendLiteral("prune secondary");

    if (validationPolicy & TileGrid::UnparentAllTiles) {
        if (builder.isEmpty())
            builder.appendLiteral(", ");
        builder.appendLiteral("unparent all");
    }
    builder.appendLiteral("]");

    return builder.toString();
}
#endif

TileGrid::TileGrid(TileController& controller)
    : m_controller(controller)
    , m_containerLayer(controller.rootLayer().createCompatibleLayer(PlatformCALayer::LayerTypeLayer, nullptr))
    , m_cohortRemovalTimer(*this, &TileGrid::cohortRemovalTimerFired)
    , m_tileSize(kDefaultTileSize, kDefaultTileSize)
{
    m_containerLayer.get().setName(TileController::tileGridContainerLayerName());
    m_containerLayer.get().setContentsScale(m_controller.deviceScaleFactor());
}

TileGrid::~TileGrid()
{
    ASSERT(isMainThread());

    for (auto& tile : m_tiles.values())
        tile.layer->setOwner(nullptr);
}

void TileGrid::setIsZoomedOutTileGrid(bool isZoomedOutGrid)
{
    if (isZoomedOutGrid)
        m_containerLayer.get().setName(TileController::zoomedOutTileGridContainerLayerName());
    else
        m_containerLayer.get().setName(TileController::tileGridContainerLayerName());
}

void TileGrid::setScale(float scale)
{
    m_scale = scale;

    TransformationMatrix transform;
    transform.scale(1 / m_scale);
    m_containerLayer->setTransform(transform);

    m_controller.setNeedsRevalidateTiles();

    m_containerLayer.get().setContentsScale(m_controller.deviceScaleFactor());

    for (auto& tile : m_tiles.values())
        tile.layer->setContentsScale(m_controller.deviceScaleFactor());
}

void TileGrid::setNeedsDisplay()
{
    for (auto& entry : m_tiles) {
        TileInfo& tileInfo = entry.value;
        IntRect tileRect = rectForTileIndex(entry.key);

        if (tileRect.intersects(m_primaryTileCoverageRect) && tileInfo.layer->superlayer())
            tileInfo.layer->setNeedsDisplay();
        else
            tileInfo.hasStaleContent = true;
    }
}

void TileGrid::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_tiles.isEmpty())
        return;

    FloatRect scaledRect(rect);
    scaledRect.scale(m_scale);
    IntRect repaintRectInTileCoords(enclosingIntRect(scaledRect));

    IntSize tileSize = m_tileSize;

    // For small invalidations, lookup the covered tiles.
    if (repaintRectInTileCoords.height() < 2 * tileSize.height() && repaintRectInTileCoords.width() < 2 * tileSize.width()) {
        TileIndex topLeft;
        TileIndex bottomRight;
        if (getTileIndexRangeForRect(repaintRectInTileCoords, topLeft, bottomRight)) {
            for (int y = topLeft.y(); y <= bottomRight.y(); ++y) {
                for (int x = topLeft.x(); x <= bottomRight.x(); ++x) {
                    TileIndex tileIndex(x, y);
                    
                    TileMap::iterator it = m_tiles.find(tileIndex);
                    if (it != m_tiles.end())
                        setTileNeedsDisplayInRect(tileIndex, it->value, repaintRectInTileCoords, m_primaryTileCoverageRect);
                }
            }
        }
        return;
    }

    for (TileMap::iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it)
        setTileNeedsDisplayInRect(it->key, it->value, repaintRectInTileCoords, m_primaryTileCoverageRect);
}

void TileGrid::dropTilesInRect(const IntRect& rect)
{
    if (m_tiles.isEmpty())
        return;

    FloatRect scaledRect(rect);
    scaledRect.scale(m_scale);
    IntRect dropRectInTileCoords(enclosingIntRect(scaledRect));

    Vector<TileIndex> tilesToRemove;

    for (auto& index : m_tiles.keys()) {
        if (rectForTileIndex(index).intersects(dropRectInTileCoords))
            tilesToRemove.append(index);
    }

    removeTiles(tilesToRemove);
}

void TileGrid::setTileNeedsDisplayInRect(const TileIndex& tileIndex, TileInfo& tileInfo, const IntRect& repaintRectInTileCoords, const IntRect& coverageRectInTileCoords)
{
    PlatformCALayer* tileLayer = tileInfo.layer.get();

    IntRect tileRect = rectForTileIndex(tileIndex);
    FloatRect tileRepaintRect = tileRect;
    tileRepaintRect.intersect(repaintRectInTileCoords);
    if (tileRepaintRect.isEmpty())
        return;

    tileRepaintRect.moveBy(-tileRect.location());
    
    // We could test for intersection with the visible rect. This would reduce painting yet more,
    // but may make scrolling stale tiles into view more frequent.
    if (tileRect.intersects(coverageRectInTileCoords) && tileLayer->superlayer()) {
        tileLayer->setNeedsDisplayInRect(tileRepaintRect);

        if (m_controller.rootLayer().owner()->platformCALayerShowRepaintCounter(0)) {
            FloatRect indicatorRect(0, 0, 52, 27);
            tileLayer->setNeedsDisplayInRect(indicatorRect);
        }
    } else
        tileInfo.hasStaleContent = true;
}

void TileGrid::updateTileLayerProperties()
{
    bool acceleratesDrawing = m_controller.acceleratesDrawing();
    bool deepColor = m_controller.wantsDeepColorBackingStore();
    bool subpixelAntialiasedText = m_controller.supportsSubpixelAntialiasedText();
    bool opaque = m_controller.tilesAreOpaque();
    Color tileDebugBorderColor = m_controller.tileDebugBorderColor();
    float tileDebugBorderWidth = m_controller.tileDebugBorderWidth();

    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it) {
        const TileInfo& tileInfo = it->value;
        tileInfo.layer->setAcceleratesDrawing(acceleratesDrawing);
        tileInfo.layer->setWantsDeepColorBackingStore(deepColor);
        tileInfo.layer->setSupportsSubpixelAntialiasedText(subpixelAntialiasedText);
        tileInfo.layer->setOpaque(opaque);
        tileInfo.layer->setBorderColor(tileDebugBorderColor);
        tileInfo.layer->setBorderWidth(tileDebugBorderWidth);
    }
}

bool TileGrid::tilesWouldChangeForCoverageRect(const FloatRect& coverageRect) const
{
    if (coverageRect.isEmpty())
        return false;

    FloatRect scaledRect(coverageRect);
    scaledRect.scale(m_scale);
    IntRect currentCoverageRectInTileCoords(enclosingIntRect(scaledRect));

    IntRect tileCoverageRect;
    TileIndex topLeft;
    TileIndex bottomRight;
    if (getTileIndexRangeForRect(currentCoverageRectInTileCoords, topLeft, bottomRight)) {
        tileCoverageRect = rectForTileIndex(topLeft);
        tileCoverageRect.unite(rectForTileIndex(bottomRight));
    }

    return tileCoverageRect != m_primaryTileCoverageRect;
}

bool TileGrid::prepopulateRect(const FloatRect& rect)
{
    LOG_WITH_STREAM(Tiling, stream << "TileGrid " << this << " prepopulateRect: " << rect);

    IntRect enclosingCoverageRect = enclosingIntRect(rect);
    if (m_primaryTileCoverageRect.contains(enclosingCoverageRect))
        return false;

    m_secondaryTileCoverageRects.append(enclosingCoverageRect);
    return true;
}

IntRect TileGrid::rectForTileIndex(const TileIndex& tileIndex) const
{
    // FIXME: calculating the scaled size here should match with the rest of calculated sizes where we use the combination of
    // enclosingIntRect, expandedIntSize (floor vs ceil).
    // However enclosing this size could reveal gap on root layer's background. see RenderView::backgroundRect()
    IntSize tileSize = m_tileSize;
    IntRect rect(tileIndex.x() * tileSize.width(), tileIndex.y() * tileSize.height(), tileSize.width(), tileSize.height());
    IntRect scaledBounds(m_controller.bounds());
    scaledBounds.scale(m_scale);
    rect.intersect(scaledBounds);
    return rect;
}

bool TileGrid::getTileIndexRangeForRect(const IntRect& rect, TileIndex& topLeft, TileIndex& bottomRight) const
{
    IntRect clampedRect = m_controller.bounds();
    clampedRect.scale(m_scale);
    clampedRect.intersect(rect);
    
    if (clampedRect.isEmpty())
        return false;

    auto tileSize = m_tileSize;
    if (clampedRect.x() >= 0)
        topLeft.setX(clampedRect.x() / tileSize.width());
    else
        topLeft.setX(floorf((float)clampedRect.x() / tileSize.width()));

    if (clampedRect.y() >= 0)
        topLeft.setY(clampedRect.y() / tileSize.height());
    else
        topLeft.setY(floorf((float)clampedRect.y() / tileSize.height()));

    int bottomXRatio = ceil((float)clampedRect.maxX() / tileSize.width());
    bottomRight.setX(std::max(bottomXRatio - 1, 0));

    int bottomYRatio = ceil((float)clampedRect.maxY() / tileSize.height());
    bottomRight.setY(std::max(bottomYRatio - 1, 0));
    
    return true;
}

unsigned TileGrid::blankPixelCount() const
{
    PlatformLayerList tiles(m_tiles.size());

    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it) {
        if (PlatformLayer *layer = it->value.layer->platformLayer())
            tiles.append(layer);
    }

    return TileController::blankPixelCountForTiles(tiles, m_controller.visibleRect(), IntPoint(0, 0));
}

void TileGrid::removeTiles(Vector<TileGrid::TileIndex>& toRemove)
{
    for (size_t i = 0; i < toRemove.size(); ++i) {
        TileInfo tileInfo = m_tiles.take(toRemove[i]);
        tileInfo.layer->removeFromSuperlayer();
        m_tileRepaintCounts.remove(tileInfo.layer.get());
        tileInfo.layer->moveToLayerPool();
    }
}

void TileGrid::removeAllTiles()
{
    Vector<TileIndex> tilesToRemove;
    tilesToRemove.reserveInitialCapacity(m_tiles.size());

    for (auto& entry : m_tiles)
        tilesToRemove.uncheckedAppend(entry.key);

    removeTiles(tilesToRemove);
}

void TileGrid::removeAllSecondaryTiles()
{
    Vector<TileIndex> tilesToRemove;

    for (auto& entry : m_tiles) {
        const TileInfo& tileInfo = entry.value;
        if (tileInfo.cohort == VisibleTileCohort)
            continue;
        tilesToRemove.append(entry.key);
    }

    removeTiles(tilesToRemove);
}

void TileGrid::removeTilesInCohort(TileCohort cohort)
{
    ASSERT(cohort != VisibleTileCohort);
    Vector<TileIndex> tilesToRemove;

    for (auto& entry : m_tiles) {
        const TileInfo& tileInfo = entry.value;
        if (tileInfo.cohort != cohort)
            continue;
        tilesToRemove.append(entry.key);
    }

    removeTiles(tilesToRemove);
}

void TileGrid::revalidateTiles(TileValidationPolicy validationPolicy)
{
    FloatRect coverageRect = m_controller.coverageRect();
    IntRect bounds = m_controller.bounds();

    LOG_WITH_STREAM(Tiling, stream << "TileGrid " << this << " (controller " << &m_controller << ") revalidateTiles: bounds " << bounds << " coverageRect" << coverageRect << " validation: " << validationPolicyAsString(validationPolicy));

    FloatRect scaledRect(coverageRect);
    scaledRect.scale(m_scale);
    IntRect coverageRectInTileCoords(enclosingIntRect(scaledRect));

    TileCohort currCohort = nextTileCohort();
    unsigned tilesInCohort = 0;

    Seconds minimumRevalidationTimerDuration = Seconds::infinity();
    bool needsTileRevalidation = false;
    
    auto tileSize = m_controller.computeTileSize();
    if (tileSize != m_tileSize) {
        removeAllTiles();
        m_tileSize = tileSize;
    }

    // Move tiles newly outside the coverage rect into the cohort map.
    for (auto& entry : m_tiles) {
        TileInfo& tileInfo = entry.value;
        TileIndex tileIndex = entry.key;

        PlatformCALayer* tileLayer = tileInfo.layer.get();
        IntRect tileRect = rectForTileIndex(tileIndex);

        if (tileRect.intersects(coverageRectInTileCoords)) {
            tileInfo.cohort = VisibleTileCohort;
            if (tileInfo.hasStaleContent) {
                // FIXME: store a dirty region per layer?
                tileLayer->setNeedsDisplay();
                tileInfo.hasStaleContent = false;
            }
        } else {
            // Add to the currentCohort if not already in one.
            if (tileInfo.cohort == VisibleTileCohort) {
                tileInfo.cohort = currCohort;
                ++tilesInCohort;
                tileLayer->removeFromSuperlayer();
            } else if (m_controller.shouldAggressivelyRetainTiles() && tileLayer->superlayer()) {
                // Aggressive tile retention means we'll never remove cohorts, but we need to make sure they're unparented.
                // We can't immediately unparent cohorts comprised of secondary tiles that never touch the primary coverage rect,
                // because that would defeat the usefulness of prepopulateRect(); instead, age prepopulated tiles out as if they were being removed.
                for (auto& cohort : m_cohortList) {
                    if (cohort.cohort != tileInfo.cohort)
                        continue;
                    Seconds timeUntilCohortExpires = cohort.timeUntilExpiration();
                    if (timeUntilCohortExpires > 0_s) {
                        minimumRevalidationTimerDuration = std::min(minimumRevalidationTimerDuration, timeUntilCohortExpires);
                        needsTileRevalidation = true;
                    } else
                        tileLayer->removeFromSuperlayer();
                    break;
                }
            }
        }
    }

    if (needsTileRevalidation)
        m_controller.scheduleTileRevalidation(minimumRevalidationTimerDuration);

    if (!m_controller.shouldAggressivelyRetainTiles()) {
        if (m_controller.shouldTemporarilyRetainTileCohorts())
            scheduleCohortRemoval();
        else if (tilesInCohort) {
            removeTilesInCohort(currCohort);
            tilesInCohort = 0;
        }
    }

    if (tilesInCohort)
        startedNewCohort(currCohort);

    if (validationPolicy & PruneSecondaryTiles) {
        removeAllSecondaryTiles();
        m_cohortList.clear();
    }

    if (validationPolicy & UnparentAllTiles) {
        for (auto& tile : m_tiles.values())
            tile.layer->removeFromSuperlayer();
    }

    auto boundsAtLastRevalidate = m_controller.boundsAtLastRevalidate();
    if (boundsAtLastRevalidate != bounds) {
        // If there are margin tiles and the bounds have grown taller or wider, then the tiles that used to
        // be bottom or right margin tiles need to be invalidated.
        if (m_controller.hasMargins()) {
            if (bounds.width() > boundsAtLastRevalidate.width() || bounds.height() > boundsAtLastRevalidate.height()) {
                IntRect boundsWithoutMargin = m_controller.boundsWithoutMargin();
                IntRect oldBoundsWithoutMargin = m_controller.boundsAtLastRevalidateWithoutMargin();

                if (bounds.height() > boundsAtLastRevalidate.height()) {
                    IntRect formerBottomMarginRect = IntRect(oldBoundsWithoutMargin.x(), oldBoundsWithoutMargin.height(),
                        oldBoundsWithoutMargin.width(), boundsWithoutMargin.height() - oldBoundsWithoutMargin.height());
                    setNeedsDisplayInRect(formerBottomMarginRect);
                }

                if (bounds.width() > boundsAtLastRevalidate.width()) {
                    IntRect formerRightMarginRect = IntRect(oldBoundsWithoutMargin.width(), oldBoundsWithoutMargin.y(),
                        boundsWithoutMargin.width() - oldBoundsWithoutMargin.width(), oldBoundsWithoutMargin.height());
                    setNeedsDisplayInRect(formerRightMarginRect);
                }
            }
        }

        FloatRect scaledBounds(bounds);
        scaledBounds.scale(m_scale);
        IntRect boundsInTileCoords(enclosingIntRect(scaledBounds));

        TileIndex topLeftForBounds;
        TileIndex bottomRightForBounds;
        if (getTileIndexRangeForRect(boundsInTileCoords, topLeftForBounds, bottomRightForBounds)) {
            Vector<TileIndex> tilesToRemove;
            for (auto& index : m_tiles.keys()) {
                if (index.y() < topLeftForBounds.y() || index.y() > bottomRightForBounds.y() || index.x() < topLeftForBounds.x() || index.x() > bottomRightForBounds.x())
                    tilesToRemove.append(index);
            }
            removeTiles(tilesToRemove);
        }
    }

    // Ensure primary tile coverage tiles.
    m_primaryTileCoverageRect = ensureTilesForRect(coverageRect, CoverageType::PrimaryTiles);

    // Ensure secondary tiles (requested via prepopulateRect).
    if (!(validationPolicy & PruneSecondaryTiles)) {
        for (auto& secondaryCoverageRect : m_secondaryTileCoverageRects) {
            FloatRect secondaryRectInLayerCoordinates(secondaryCoverageRect);
            secondaryRectInLayerCoordinates.scale(1 / m_scale);
            ensureTilesForRect(secondaryRectInLayerCoordinates, CoverageType::SecondaryTiles);
        }
        m_secondaryTileCoverageRects.clear();
    }

    m_controller.didRevalidateTiles();
}

TileGrid::TileCohort TileGrid::nextTileCohort() const
{
    if (!m_cohortList.isEmpty())
        return m_cohortList.last().cohort + 1;

    return 1;
}

void TileGrid::startedNewCohort(TileCohort cohort)
{
    m_cohortList.append(TileCohortInfo(cohort, MonotonicTime::now()));
#if PLATFORM(IOS_FAMILY)
    if (!m_controller.isInWindow())
        tileControllerMemoryHandler().tileControllerGainedUnparentedTiles(&m_controller);
#endif
}

TileGrid::TileCohort TileGrid::newestTileCohort() const
{
    return m_cohortList.isEmpty() ? 0 : m_cohortList.last().cohort;
}

TileGrid::TileCohort TileGrid::oldestTileCohort() const
{
    return m_cohortList.isEmpty() ? 0 : m_cohortList.first().cohort;
}

void TileGrid::scheduleCohortRemoval()
{
    const Seconds cohortRemovalTimerSeconds { 1_s };

    // Start the timer, or reschedule the timer from now if it's already active.
    if (!m_cohortRemovalTimer.isActive())
        m_cohortRemovalTimer.startRepeating(cohortRemovalTimerSeconds);
}

Seconds TileGrid::TileCohortInfo::timeUntilExpiration()
{
    Seconds cohortLifeTimeSeconds = 2_s;
    MonotonicTime timeThreshold = MonotonicTime::now() - cohortLifeTimeSeconds;
    return creationTime - timeThreshold;
}

void TileGrid::cohortRemovalTimerFired()
{
    if (m_cohortList.isEmpty()) {
        m_cohortRemovalTimer.stop();
        return;
    }

    while (!m_cohortList.isEmpty() && m_cohortList.first().timeUntilExpiration() < 0_s) {
        TileCohortInfo firstCohort = m_cohortList.takeFirst();
        removeTilesInCohort(firstCohort.cohort);
    }

    m_controller.updateTileCoverageMap();
}

IntRect TileGrid::ensureTilesForRect(const FloatRect& rect, CoverageType newTileType)
{
    if (!m_controller.isInWindow())
        return IntRect();

    FloatRect scaledRect(rect);
    scaledRect.scale(m_scale);
    IntRect rectInTileCoords(enclosingIntRect(scaledRect));

    TileIndex topLeft;
    TileIndex bottomRight;
    if (!getTileIndexRangeForRect(rectInTileCoords, topLeft, bottomRight))
        return IntRect();

    TileCohort currCohort = nextTileCohort();
    unsigned tilesInCohort = 0;

    IntRect coverageRect;

    for (int y = topLeft.y(); y <= bottomRight.y(); ++y) {
        for (int x = topLeft.x(); x <= bottomRight.x(); ++x) {
            TileIndex tileIndex(x, y);

            IntRect tileRect = rectForTileIndex(tileIndex);
            TileInfo& tileInfo = m_tiles.add(tileIndex, TileInfo()).iterator->value;

            coverageRect.unite(tileRect);

            bool shouldChangeTileLayerFrame = false;

            if (!tileInfo.layer) {
                tileInfo.layer = m_controller.createTileLayer(tileRect, *this);
                ASSERT(!m_tileRepaintCounts.contains(tileInfo.layer.get()));
            } else {
                // We already have a layer for this tile. Ensure that its size is correct.
                FloatSize tileLayerSize(tileInfo.layer->bounds().size());
                shouldChangeTileLayerFrame = tileLayerSize != FloatSize(tileRect.size());

                if (shouldChangeTileLayerFrame) {
                    tileInfo.layer->setBounds(FloatRect(FloatPoint(), tileRect.size()));
                    tileInfo.layer->setPosition(tileRect.location());
                    tileInfo.layer->setNeedsDisplay();
                }
            }

            if (newTileType == CoverageType::SecondaryTiles && !tileRect.intersects(m_primaryTileCoverageRect)) {
                tileInfo.cohort = currCohort;
                ++tilesInCohort;
            }

            if (!tileInfo.layer->superlayer())
                m_containerLayer.get().appendSublayer(*tileInfo.layer);
        }
    }

    if (tilesInCohort)
        startedNewCohort(currCohort);

    LOG_WITH_STREAM(Tiling, stream << "TileGrid " << this << " (bounds " << m_controller.bounds() << ") ensureTilesForRect: " << rect << " covered " << coverageRect);

    return coverageRect;
}

IntRect TileGrid::extent() const
{
    TileIndex topLeft;
    TileIndex bottomRight;
    if (getTileIndexRangeForRect(m_primaryTileCoverageRect, topLeft, bottomRight)) {
        // Return index of top, left tile and the number of tiles across and down.
        return IntRect(topLeft.x(), topLeft.y(), bottomRight.x() - topLeft.x() + 1, bottomRight.y() - topLeft.y() + 1);
    }

    return IntRect();
}

double TileGrid::retainedTileBackingStoreMemory() const
{
    double totalBytes = 0;
    
    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it) {
        const TileInfo& tileInfo = it->value;
        if (tileInfo.layer->superlayer()) {
            FloatRect bounds = tileInfo.layer->bounds();
            double contentsScale = tileInfo.layer->contentsScale();
            totalBytes += 4 * bounds.width() * contentsScale * bounds.height() * contentsScale;
        }
    }

    return totalBytes;
}

// Return the rect in layer coords, not tile coords.
IntRect TileGrid::tileCoverageRect() const
{
    IntRect coverageRectInLayerCoords(m_primaryTileCoverageRect);
    coverageRectInLayerCoords.scale(1 / m_scale);
    return coverageRectInLayerCoords;
}

void TileGrid::drawTileMapContents(CGContextRef context, CGRect layerBounds) const
{
    CGContextSetRGBFillColor(context, 0.3, 0.3, 0.3, 1);
    CGContextFillRect(context, layerBounds);

    CGFloat scaleFactor = layerBounds.size.width / m_controller.bounds().width();

    CGFloat contextScale = scaleFactor / m_scale;
    CGContextScaleCTM(context, contextScale, contextScale);
    
    for (TileMap::const_iterator it = m_tiles.begin(), end = m_tiles.end(); it != end; ++it) {
        const TileInfo& tileInfo = it->value;
        PlatformCALayer* tileLayer = tileInfo.layer.get();

        CGFloat red = 1;
        CGFloat green = 1;
        CGFloat blue = 1;
        CGFloat alpha = 1;
        if (tileInfo.hasStaleContent) {
            red = 0.25;
            green = 0.125;
            blue = 0;
        } else if (m_controller.shouldAggressivelyRetainTiles() && tileInfo.cohort != VisibleTileCohort) {
            red = 0.8;
            green = 0.8;
            blue = 0.8;
        }

        TileCohort newestCohort = newestTileCohort();
        TileCohort oldestCohort = oldestTileCohort();

        if (!m_controller.shouldAggressivelyRetainTiles() && tileInfo.cohort != VisibleTileCohort && newestCohort > oldestCohort)
            alpha = 1 - (static_cast<float>((newestCohort - tileInfo.cohort)) / (newestCohort - oldestCohort));

        CGContextSetRGBFillColor(context, red, green, blue, alpha);

        if (tileLayer->superlayer()) {
            CGContextSetLineWidth(context, 0.5 / contextScale);
            CGContextSetRGBStrokeColor(context, 0, 0, 0, 1);
        } else {
            CGContextSetLineWidth(context, 1 / contextScale);
            CGContextSetRGBStrokeColor(context, 0.2, 0.1, 0.9, 1);
        }

        CGRect frame = CGRectMake(tileLayer->position().x(), tileLayer->position().y(), tileLayer->bounds().size().width(), tileLayer->bounds().size().height());
        CGContextFillRect(context, frame);
        CGContextStrokeRect(context, frame);

        CGContextSetRGBFillColor(context, 0, 0, 0, 0.5);

        String repaintCount = String::number(m_tileRepaintCounts.get(tileLayer));

        CGContextSaveGState(context);

        tileLayer->drawTextAtPoint(context, frame.origin.x + 64, frame.origin.y + 192, CGSizeMake(3, -3), 58,
            repaintCount.ascii().data(), repaintCount.length());

        CGContextRestoreGState(context);
    }
}

void TileGrid::platformCALayerPaintContents(PlatformCALayer* platformCALayer, GraphicsContext& context, const FloatRect&, GraphicsLayerPaintBehavior layerPaintBehavior)
{
#if PLATFORM(IOS_FAMILY)
    if (pthread_main_np())
        WebThreadLock();
#endif

    if (!platformCALayerRepaintCount(platformCALayer))
        layerPaintBehavior |= GraphicsLayerPaintFirstTilePaint;

    {
        GraphicsContextStateSaver stateSaver(context);

        FloatPoint3D layerOrigin = platformCALayer->position();
        context.translate(-layerOrigin.x(), -layerOrigin.y());
        context.scale(m_scale);

        PlatformCALayer::RepaintRectList dirtyRects = PlatformCALayer::collectRectsToPaint(context.platformContext(), platformCALayer);
        PlatformCALayer::drawLayerContents(context.platformContext(), &m_controller.rootLayer(), dirtyRects, layerPaintBehavior);
    }

    int repaintCount = platformCALayerIncrementRepaintCount(platformCALayer);
    if (m_controller.rootLayer().owner()->platformCALayerShowRepaintCounter(0))
        PlatformCALayer::drawRepaintIndicator(context.platformContext(), platformCALayer, repaintCount, cachedCGColor(m_controller.tileDebugBorderColor()));

    if (m_controller.scrollingPerformanceLoggingEnabled()) {
        FloatRect visiblePart(platformCALayer->position().x(), platformCALayer->position().y(), platformCALayer->bounds().size().width(), platformCALayer->bounds().size().height());
        visiblePart.intersect(m_controller.visibleRect());

        if (repaintCount == 1 && !visiblePart.isEmpty())
            m_controller.logFilledVisibleFreshTile(blankPixelCount());
    }
}

float TileGrid::platformCALayerDeviceScaleFactor() const
{
    return m_controller.rootLayer().owner()->platformCALayerDeviceScaleFactor();
}

bool TileGrid::platformCALayerShowDebugBorders() const
{
    return m_controller.rootLayer().owner()->platformCALayerShowDebugBorders();
}

bool TileGrid::platformCALayerShowRepaintCounter(PlatformCALayer*) const
{
    return m_controller.rootLayer().owner()->platformCALayerShowRepaintCounter(nullptr);
}

bool TileGrid::isUsingDisplayListDrawing(PlatformCALayer*) const
{
    return m_controller.rootLayer().owner()->isUsingDisplayListDrawing(nullptr);
}

bool TileGrid::platformCALayerContentsOpaque() const
{
    return m_controller.tilesAreOpaque();
}

int TileGrid::platformCALayerRepaintCount(PlatformCALayer* platformCALayer) const
{
    const auto it = m_tileRepaintCounts.find(platformCALayer);
    return it != m_tileRepaintCounts.end() ? it->value : 0;
}

int TileGrid::platformCALayerIncrementRepaintCount(PlatformCALayer* platformCALayer)
{
    int repaintCount = 0;

    if (m_tileRepaintCounts.contains(platformCALayer))
        repaintCount = m_tileRepaintCounts.get(platformCALayer);

    m_tileRepaintCounts.set(platformCALayer, ++repaintCount);

    return repaintCount;
}

#if PLATFORM(IOS_FAMILY)
void TileGrid::removeUnparentedTilesNow()
{
    while (!m_cohortList.isEmpty()) {
        TileCohortInfo firstCohort = m_cohortList.takeFirst();
        removeTilesInCohort(firstCohort.cohort);
    }
}
#endif

} // namespace WebCore

#endif
