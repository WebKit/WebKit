/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TiledLayerChromium.h"

#include "GraphicsContext3D.h"
#include "Region.h"
#include "TextStream.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCTextureUpdateQueue.h"
#include "cc/CCTiledLayerImpl.h"

#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>

using namespace std;
using WebKit::WebTransformationMatrix;

namespace WebCore {

class UpdatableTile : public CCLayerTilingData::Tile {
    WTF_MAKE_NONCOPYABLE(UpdatableTile);
public:
    static PassOwnPtr<UpdatableTile> create(PassOwnPtr<LayerTextureUpdater::Texture> texture)
    {
        return adoptPtr(new UpdatableTile(texture));
    }

    LayerTextureUpdater::Texture* texture() { return m_texture.get(); }
    CCPrioritizedTexture* managedTexture() { return m_texture->texture(); }

    bool isDirty() const { return !dirtyRect.isEmpty(); }

    // Reset update state for the current frame. This should occur before painting
    // for all layers. Since painting one layer can invalidate another layer
    // after it has already painted, mark all non-dirty tiles as valid before painting
    // such that invalidations during painting won't prevent them from being pushed.
    void resetUpdateState()
    {
        updateRect = IntRect();
        occluded = false;
        partialUpdate = false;
        validForFrame = !isDirty();
    }

    // This promises to update the tile and therefore also guarantees the tile
    // will be valid for this frame. dirtyRect is copied into updateRect so
    // we can continue to track re-entrant invalidations that occur during painting.
    void markForUpdate()
    {
        validForFrame = true;
        updateRect = dirtyRect;
        dirtyRect = IntRect();
    }

    IntRect dirtyRect;
    IntRect updateRect;
    bool partialUpdate;
    bool validForFrame;
    bool occluded;
    bool isInUseOnImpl;
private:
    explicit UpdatableTile(PassOwnPtr<LayerTextureUpdater::Texture> texture)
        : partialUpdate(false)
        , validForFrame(false)
        , occluded(false)
        , isInUseOnImpl(false)
        , m_texture(texture)
    {
    }

    OwnPtr<LayerTextureUpdater::Texture> m_texture;
};

TiledLayerChromium::TiledLayerChromium()
    : LayerChromium()
    , m_textureFormat(GraphicsContext3D::INVALID_ENUM)
    , m_skipsDraw(false)
    , m_failedUpdate(false)
    , m_sampledTexelFormat(LayerTextureUpdater::SampledTexelFormatInvalid)
    , m_tilingOption(AutoTile)
{
    m_tiler = CCLayerTilingData::create(IntSize(), CCLayerTilingData::HasBorderTexels);
}

TiledLayerChromium::~TiledLayerChromium()
{
}

PassOwnPtr<CCLayerImpl> TiledLayerChromium::createCCLayerImpl()
{
    return CCTiledLayerImpl::create(id());
}

void TiledLayerChromium::updateTileSizeAndTilingOption()
{
    ASSERT(layerTreeHost());

    const IntSize& defaultTileSize = layerTreeHost()->settings().defaultTileSize;
    const IntSize& maxUntiledLayerSize = layerTreeHost()->settings().maxUntiledLayerSize;
    int layerWidth = contentBounds().width();
    int layerHeight = contentBounds().height();

    const IntSize tileSize(min(defaultTileSize.width(), layerWidth), min(defaultTileSize.height(), layerHeight));

    // Tile if both dimensions large, or any one dimension large and the other
    // extends into a second tile but the total layer area isn't larger than that
    // of the largest possible untiled layer. This heuristic allows for long skinny layers
    // (e.g. scrollbars) that are Nx1 tiles to minimize wasted texture space but still avoids
    // creating very large tiles.
    const bool anyDimensionLarge = layerWidth > maxUntiledLayerSize.width() || layerHeight > maxUntiledLayerSize.height();
    const bool anyDimensionOneTile = (layerWidth <= defaultTileSize.width() || layerHeight <= defaultTileSize.height())
                                      && (layerWidth * layerHeight) <= (maxUntiledLayerSize.width() * maxUntiledLayerSize.height());
    const bool autoTiled = anyDimensionLarge && !anyDimensionOneTile;

    bool isTiled;
    if (m_tilingOption == AlwaysTile)
        isTiled = true;
    else if (m_tilingOption == NeverTile)
        isTiled = false;
    else
        isTiled = autoTiled;

    IntSize requestedSize = isTiled ? tileSize : contentBounds();
    const int maxSize = layerTreeHost()->layerRendererCapabilities().maxTextureSize;
    IntSize clampedSize = requestedSize.shrunkTo(IntSize(maxSize, maxSize));
    setTileSize(clampedSize);
}

void TiledLayerChromium::updateBounds()
{
    IntSize oldBounds = m_tiler->bounds();
    IntSize newBounds = contentBounds();
    if (oldBounds == newBounds)
        return;
    m_tiler->setBounds(newBounds);

    // Invalidate any areas that the new bounds exposes.
    Region oldRegion(IntRect(IntPoint(), oldBounds));
    Region newRegion(IntRect(IntPoint(), newBounds));
    newRegion.subtract(oldRegion);
    Vector<IntRect> rects = newRegion.rects();
    for (size_t i = 0; i < rects.size(); ++i)
        invalidateContentRect(rects[i]);
}

void TiledLayerChromium::setTileSize(const IntSize& size)
{
    m_tiler->setTileSize(size);
}

void TiledLayerChromium::setBorderTexelOption(CCLayerTilingData::BorderTexelOption borderTexelOption)
{
    m_tiler->setBorderTexelOption(borderTexelOption);
}

bool TiledLayerChromium::drawsContent() const
{
    if (!LayerChromium::drawsContent())
        return false;

    bool hasMoreThanOneTile = m_tiler->numTilesX() > 1 || m_tiler->numTilesY() > 1;
    if (m_tilingOption == NeverTile && hasMoreThanOneTile)
        return false;

    return true;
}

bool TiledLayerChromium::needsContentsScale() const
{
    return true;
}

IntSize TiledLayerChromium::contentBounds() const
{
    return IntSize(lroundf(bounds().width() * contentsScale()), lroundf(bounds().height() * contentsScale()));
}

void TiledLayerChromium::setTilingOption(TilingOption tilingOption)
{
    m_tilingOption = tilingOption;
}

void TiledLayerChromium::setIsMask(bool isMask)
{
    setTilingOption(isMask ? NeverTile : AutoTile);
}

void TiledLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCTiledLayerImpl* tiledLayer = static_cast<CCTiledLayerImpl*>(layer);

    tiledLayer->setSkipsDraw(m_skipsDraw);
    tiledLayer->setContentsSwizzled(m_sampledTexelFormat != LayerTextureUpdater::SampledTexelFormatRGBA);
    tiledLayer->setTilingData(*m_tiler);
    Vector<UpdatableTile*> invalidTiles;

    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        int i = iter->first.first;
        int j = iter->first.second;
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        tile->isInUseOnImpl = false;
        if (!tile->managedTexture()->haveBackingTexture()) {
            invalidTiles.append(tile);
            continue;
        }
        if (!tile->validForFrame)
            continue;

        tiledLayer->pushTileProperties(i, j, tile->managedTexture()->resourceId(), tile->opaqueRect());
        tile->isInUseOnImpl = true;
    }
    for (Vector<UpdatableTile*>::const_iterator iter = invalidTiles.begin(); iter != invalidTiles.end(); ++iter)
        m_tiler->takeTile((*iter)->i(), (*iter)->j());
}

CCPrioritizedTextureManager* TiledLayerChromium::textureManager() const
{
    if (!layerTreeHost())
        return 0;
    return layerTreeHost()->contentsTextureManager();
}

void TiledLayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (host && host != layerTreeHost()) {
        for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
            UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            tile->managedTexture()->setTextureManager(host->contentsTextureManager());
        }
    }
    LayerChromium::setLayerTreeHost(host);
}

UpdatableTile* TiledLayerChromium::tileAt(int i, int j) const
{
    return static_cast<UpdatableTile*>(m_tiler->tileAt(i, j));
}

UpdatableTile* TiledLayerChromium::createTile(int i, int j)
{
    createTextureUpdaterIfNeeded();

    OwnPtr<UpdatableTile> tile(UpdatableTile::create(textureUpdater()->createTexture(textureManager())));
    tile->managedTexture()->setDimensions(m_tiler->tileSize(), m_textureFormat);

    UpdatableTile* addedTile = tile.get();
    m_tiler->addTile(tile.release(), i, j);

    addedTile->dirtyRect = m_tiler->tileRect(addedTile);

    // Temporary diagnostic crash.
    if (!addedTile)
        CRASH();
    if (!tileAt(i, j))
        CRASH();

    return addedTile;
}

void TiledLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    float contentsWidthScale = static_cast<float>(contentBounds().width()) / bounds().width();
    float contentsHeightScale = static_cast<float>(contentBounds().height()) / bounds().height();
    FloatRect scaledDirtyRect(dirtyRect);
    scaledDirtyRect.scale(contentsWidthScale, contentsHeightScale);
    IntRect dirty = enclosingIntRect(scaledDirtyRect);
    invalidateContentRect(dirty);
    LayerChromium::setNeedsDisplayRect(dirtyRect);
}

void TiledLayerChromium::setUseLCDText(bool useLCDText)
{
    LayerChromium::setUseLCDText(useLCDText);

    CCLayerTilingData::BorderTexelOption borderTexelOption;
#if OS(ANDROID)
    // Always want border texels and GL_LINEAR due to pinch zoom.
    borderTexelOption = CCLayerTilingData::HasBorderTexels;
#else
    borderTexelOption = useLCDText ? CCLayerTilingData::NoBorderTexels : CCLayerTilingData::HasBorderTexels;
#endif
    setBorderTexelOption(borderTexelOption);
}

void TiledLayerChromium::invalidateContentRect(const IntRect& contentRect)
{
    updateBounds();
    if (m_tiler->isEmpty() || contentRect.isEmpty() || m_skipsDraw)
        return;

    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        ASSERT(tile);
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        IntRect bound = m_tiler->tileRect(tile);
        bound.intersect(contentRect);
        tile->dirtyRect.unite(bound);
    }
}

// Returns true if tile is dirty and only part of it needs to be updated.
bool TiledLayerChromium::tileOnlyNeedsPartialUpdate(UpdatableTile* tile)
{
    return !tile->dirtyRect.contains(m_tiler->tileRect(tile));
}

// Dirty tiles with valid textures needs buffered update to guarantee that
// we don't modify textures currently used for drawing by the impl thread.
bool TiledLayerChromium::tileNeedsBufferedUpdate(UpdatableTile* tile)
{
    if (!tile->managedTexture()->haveBackingTexture())
        return false;

    if (!tile->isDirty())
        return false;

    if (!tile->isInUseOnImpl)
        return false;

    return true;
}


bool TiledLayerChromium::updateTiles(int left, int top, int right, int bottom, CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats, bool& didPaint)
{
    didPaint = false;
    createTextureUpdaterIfNeeded();

    bool ignoreOcclusions = !occlusion;
    if (!haveTexturesForTiles(left, top, right, bottom, ignoreOcclusions)) {
        m_failedUpdate = true;
        return false;
    }

    IntRect paintRect = markTilesForUpdate(left, top, right, bottom, ignoreOcclusions);

    if (occlusion)
        occlusion->overdrawMetrics().didPaint(paintRect);

    if (paintRect.isEmpty())
        return true;

    didPaint = true;
    updateTileTextures(paintRect, left, top, right, bottom, queue, occlusion, stats);
    return true;
}

void TiledLayerChromium::markOcclusionsAndRequestTextures(int left, int top, int right, int bottom, const CCOcclusionTracker* occlusion)
{
    // There is some difficult dependancies between occlusions, recording occlusion metrics
    // and requesting memory so those are encapsulated in this function:
    // - We only want to call requestLate on unoccluded textures (to preserve
    //   memory for other layers when near OOM).
    // - We only want to record occlusion metrics if all memory requests succeed.

    int occludedTileCount = 0;
    bool succeeded = true;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            ASSERT(tile); // Did setTexturePriorities get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            ASSERT(!tile->occluded); // Did resetUpdateState get skipped? Are we doing more than one occlusion pass?
            IntRect visibleTileRect = intersection(m_tiler->tileBounds(i, j), visibleContentRect());
            if (occlusion && occlusion->occluded(this, visibleTileRect)) {
                tile->occluded = true;
                occludedTileCount++;
            } else {
                succeeded &= tile->managedTexture()->requestLate();
            }
        }
    }

    if (!succeeded)
        return;

    // FIXME: Remove the loop and just pass the count!
    for (int i = 0; i < occludedTileCount; i++)
        occlusion->overdrawMetrics().didCullTileForUpload();
}

bool TiledLayerChromium::haveTexturesForTiles(int left, int top, int right, int bottom, bool ignoreOcclusions)
{
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            ASSERT(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;

            // Ensure the entire tile is dirty if we don't have the texture.
            if (!tile->managedTexture()->haveBackingTexture())
                tile->dirtyRect = m_tiler->tileRect(tile);

            // If using occlusion and the visible region of the tile is occluded,
            // don't reserve a texture or update the tile.
            if (tile->occluded && !ignoreOcclusions)
                continue;

            if (!tile->managedTexture()->canAcquireBackingTexture())
                return false;
        }
    }
    return true;
}

IntRect TiledLayerChromium::markTilesForUpdate(int left, int top, int right, int bottom, bool ignoreOcclusions)
{
    IntRect paintRect;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            ASSERT(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            if (tile->occluded && !ignoreOcclusions)
                continue;
            paintRect.unite(tile->dirtyRect);
            tile->markForUpdate();
        }
    }
    return paintRect;
}

void TiledLayerChromium::updateTileTextures(const IntRect& paintRect, int left, int top, int right, int bottom, CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats)
{
    // The updateRect should be in layer space. So we have to convert the paintRect from content space to layer space.
    m_updateRect = FloatRect(paintRect);
    float widthScale = bounds().width() / static_cast<float>(contentBounds().width());
    float heightScale = bounds().height() / static_cast<float>(contentBounds().height());
    m_updateRect.scale(widthScale, heightScale);

    // Calling prepareToUpdate() calls into WebKit to paint, which may have the side
    // effect of disabling compositing, which causes our reference to the texture updater to be deleted.
    // However, we can't free the memory backing the SkCanvas until the paint finishes,
    // so we grab a local reference here to hold the updater alive until the paint completes.
    RefPtr<LayerTextureUpdater> protector(textureUpdater());
    IntRect paintedOpaqueRect;
    textureUpdater()->prepareToUpdate(paintRect, m_tiler->tileSize(), 1 / widthScale, 1 / heightScale, paintedOpaqueRect, stats);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            ASSERT(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;

            IntRect tileRect = m_tiler->tileBounds(i, j);

            // Use updateRect as the above loop copied the dirty rect for this frame to updateRect.
            const IntRect& dirtyRect = tile->updateRect;
            if (dirtyRect.isEmpty())
                continue;

            // Save what was painted opaque in the tile. Keep the old area if the paint didn't touch it, and didn't paint some
            // other part of the tile opaque.
            IntRect tilePaintedRect = intersection(tileRect, paintRect);
            IntRect tilePaintedOpaqueRect = intersection(tileRect, paintedOpaqueRect);
            if (!tilePaintedRect.isEmpty()) {
                IntRect paintInsideTileOpaqueRect = intersection(tile->opaqueRect(), tilePaintedRect);
                bool paintInsideTileOpaqueRectIsNonOpaque = !tilePaintedOpaqueRect.contains(paintInsideTileOpaqueRect);
                bool opaquePaintNotInsideTileOpaqueRect = !tilePaintedOpaqueRect.isEmpty() && !tile->opaqueRect().contains(tilePaintedOpaqueRect);

                if (paintInsideTileOpaqueRectIsNonOpaque || opaquePaintNotInsideTileOpaqueRect)
                    tile->setOpaqueRect(tilePaintedOpaqueRect);
            }

            // sourceRect starts as a full-sized tile with border texels included.
            IntRect sourceRect = m_tiler->tileRect(tile);
            sourceRect.intersect(dirtyRect);
            // Paint rect not guaranteed to line up on tile boundaries, so
            // make sure that sourceRect doesn't extend outside of it.
            sourceRect.intersect(paintRect);

            tile->updateRect = sourceRect;

            if (sourceRect.isEmpty())
                continue;

            tile->texture()->prepareRect(sourceRect, stats);
            if (occlusion)
                occlusion->overdrawMetrics().didUpload(WebTransformationMatrix(), sourceRect, tile->opaqueRect());

            const IntPoint anchor = m_tiler->tileRect(tile).location();

            // Calculate tile-space rectangle to upload into.
            IntRect destRect(IntPoint(sourceRect.x() - anchor.x(), sourceRect.y() - anchor.y()), sourceRect.size());
            if (destRect.x() < 0)
                CRASH();
            if (destRect.y() < 0)
                CRASH();

            // Offset from paint rectangle to this tile's dirty rectangle.
            IntPoint paintOffset(sourceRect.x() - paintRect.x(), sourceRect.y() - paintRect.y());
            if (paintOffset.x() < 0)
                CRASH();
            if (paintOffset.y() < 0)
                CRASH();
            if (paintOffset.x() + destRect.width() > paintRect.width())
                CRASH();
            if (paintOffset.y() + destRect.height() > paintRect.height())
                CRASH();

            TextureUploader::Parameters upload = { tile->texture(), sourceRect, destRect };
            if (tile->partialUpdate)
                queue.appendPartialUpload(upload);
            else
                queue.appendFullUpload(upload);
        }
    }
}

void TiledLayerChromium::setTexturePriorities(const CCPriorityCalculator& priorityCalc)
{
    setTexturePrioritiesInRect(priorityCalc, visibleContentRect());
}

namespace {
// This picks a small animated layer to be anything less than one viewport. This
// is specifically for page transitions which are viewport-sized layers. The extra
// 64 pixels is due to these layers being slightly larger than the viewport in some cases.
bool isSmallAnimatedLayer(TiledLayerChromium* layer)
{
    if (!layer->drawTransformIsAnimating() && !layer->screenSpaceTransformIsAnimating())
        return false;
    IntSize viewportSize = layer->layerTreeHost() ? layer->layerTreeHost()->deviceViewportSize() : IntSize();
    IntRect contentRect(IntPoint::zero(), layer->contentBounds());
    return contentRect.width() <= viewportSize.width() + 64
        && contentRect.height() <= viewportSize.height() + 64;
}

// FIXME: Remove this and make this based on distance once distance can be calculated
// for offscreen layers. For now, prioritize all small animated layers after 512
// pixels of pre-painting.
void setPriorityForTexture(const CCPriorityCalculator& priorityCalc,
                           const IntRect& visibleRect,
                           const IntRect& tileRect,
                           bool drawsToRoot,
                           bool isSmallAnimatedLayer,
                           CCPrioritizedTexture* texture)
{
    int priority = CCPriorityCalculator::lowestPriority();
    if (!visibleRect.isEmpty())
        priority = priorityCalc.priorityFromDistance(visibleRect, tileRect, drawsToRoot);
    if (isSmallAnimatedLayer)
        priority = CCPriorityCalculator::maxPriority(priority, priorityCalc.priorityFromDistance(512, drawsToRoot));
    if (priority != CCPriorityCalculator::lowestPriority())
        texture->setRequestPriority(priority);
}
}

void TiledLayerChromium::setTexturePrioritiesInRect(const CCPriorityCalculator& priorityCalc, const IntRect& visibleContentRect)
{
    updateBounds();
    resetUpdateState();

    if (m_tiler->hasEmptyBounds())
        return;

    bool drawsToRoot = !renderTarget()->parent();
    bool smallAnimatedLayer = isSmallAnimatedLayer(this);

    // Minimally create the tiles in the desired pre-paint rect.
    IntRect createTilesRect = idlePaintRect(visibleContentRect);
    if (!createTilesRect.isEmpty()) {
        int left, top, right, bottom;
        m_tiler->contentRectToTileIndices(createTilesRect, left, top, right, bottom);
        for (int j = top; j <= bottom; ++j) {
            for (int i = left; i <= right; ++i) {
                if (!tileAt(i, j))
                    createTile(i, j);
            }
        }
    }

    // Also, minimally create all tiles for small animated layers and also
    // double-buffer them since we have limited their size to be reasonable.
    IntRect doubleBufferedRect = visibleContentRect;
    if (smallAnimatedLayer)
        doubleBufferedRect = IntRect(IntPoint::zero(), contentBounds());

    // Create additional textures for double-buffered updates when needed.
    // These textures must stay alive while the updated textures are incrementally
    // uploaded, swapped atomically via pushProperties, and finally deleted
    // after the commit is complete, after which they can be recycled.
    if (!doubleBufferedRect.isEmpty()) {
        int left, top, right, bottom;
        m_tiler->contentRectToTileIndices(doubleBufferedRect, left, top, right, bottom);
        for (int j = top; j <= bottom; ++j) {
            for (int i = left; i <= right; ++i) {
                UpdatableTile* tile = tileAt(i, j);
                if (!tile)
                    tile = createTile(i, j);
                // We need an additional texture if the tile needs a buffered-update and it's not a partial update.
                // FIXME: Decide if partial update should be allowed based on cost
                // of update. https://bugs.webkit.org/show_bug.cgi?id=77376
                if (!layerTreeHost() || !layerTreeHost()->bufferedUpdates() || !tileNeedsBufferedUpdate(tile))
                    continue;
                if (tileOnlyNeedsPartialUpdate(tile) && layerTreeHost()->requestPartialTextureUpdate()) {
                    tile->partialUpdate = true;
                    continue;
                }

                IntRect tileRect = m_tiler->tileRect(tile);
                tile->dirtyRect = tileRect;
                LayerTextureUpdater::Texture* backBuffer = tile->texture();
                setPriorityForTexture(priorityCalc, visibleContentRect, tile->dirtyRect, drawsToRoot, smallAnimatedLayer, backBuffer->texture());
                OwnPtr<CCPrioritizedTexture> frontBuffer = CCPrioritizedTexture::create(backBuffer->texture()->textureManager(),
                                                                                        backBuffer->texture()->size(),
                                                                                        backBuffer->texture()->format());
                // Swap backBuffer into frontBuffer and add it to delete after commit queue.
                backBuffer->swapTextureWith(frontBuffer);
                layerTreeHost()->deleteTextureAfterCommit(frontBuffer.release());
            }
        }
    }

    // Now update priorities on all tiles we have in the layer, no matter where they are.
    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        IntRect tileRect = m_tiler->tileRect(tile);
        setPriorityForTexture(priorityCalc, visibleContentRect, tileRect, drawsToRoot, smallAnimatedLayer, tile->managedTexture());
    }
}

Region TiledLayerChromium::visibleContentOpaqueRegion() const
{
    if (m_skipsDraw)
        return Region();
    if (opaque())
        return visibleContentRect();
    return m_tiler->opaqueRegionInContentRect(visibleContentRect());
}

void TiledLayerChromium::resetUpdateState()
{
    CCLayerTilingData::TileMap::const_iterator end = m_tiler->tiles().end();
    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != end; ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        tile->resetUpdateState();
    }
}

void TiledLayerChromium::updateContentRect(CCTextureUpdateQueue& queue, const IntRect& contentRect, const CCOcclusionTracker* occlusion, CCRenderingStats& stats)
{
    m_skipsDraw = false;
    m_failedUpdate = false;
    updateBounds();

    if (m_tiler->hasEmptyBounds())
        return;

    bool didPaint = false;

    // Animation pre-paint. If the layer is small, try to paint it all
    // immediately whether or not it is occluded, to avoid paint/upload
    // hiccups while it is animating.
    if (isSmallAnimatedLayer(this)) {
        int left, top, right, bottom;
        m_tiler->contentRectToTileIndices(IntRect(IntPoint::zero(), contentBounds()), left, top, right, bottom);
        updateTiles(left, top, right, bottom, queue, 0, stats, didPaint);
        if (didPaint)
            return;
        // This was an attempt to paint the entire layer so if we fail it's okay,
        // just fallback on painting visible etc. below.
        m_failedUpdate = false;
    }

    if (contentRect.isEmpty())
        return;

    // Visible painting. First occlude visible tiles and paint the non-occluded tiles.
    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(contentRect, left, top, right, bottom);
    markOcclusionsAndRequestTextures(left, top, right, bottom, occlusion);
    m_skipsDraw = !updateTiles(left, top, right, bottom, queue, occlusion, stats, didPaint);
    if (m_skipsDraw)
        m_tiler->reset();
    if (m_skipsDraw || didPaint)
        return;

    // If we have already painting everything visible. Do some pre-painting while idle.
    IntRect idlePaintContentRect = idlePaintRect(contentRect);
    if (idlePaintContentRect.isEmpty())
        return;

    // Prepaint anything that was occluded but inside the layer's visible region.
    if (!updateTiles(left, top, right, bottom, queue, 0, stats, didPaint) || didPaint)
        return;

    int prepaintLeft, prepaintTop, prepaintRight, prepaintBottom;
    m_tiler->contentRectToTileIndices(idlePaintContentRect, prepaintLeft, prepaintTop, prepaintRight, prepaintBottom);

    // Then expand outwards from the visible area until we find a dirty row or column to update.
    while (left > prepaintLeft || top > prepaintTop || right < prepaintRight || bottom < prepaintBottom) {
        if (bottom < prepaintBottom) {
            ++bottom;
            if (!updateTiles(left, bottom, right, bottom, queue, 0, stats, didPaint) || didPaint)
                return;
        }
        if (top > prepaintTop) {
            --top;
            if (!updateTiles(left, top, right, top, queue, 0, stats, didPaint) || didPaint)
                return;
        }
        if (left > prepaintLeft) {
            --left;
            if (!updateTiles(left, top, left, bottom, queue, 0, stats, didPaint) || didPaint)
                return;
        }
        if (right < prepaintRight) {
            ++right;
            if (!updateTiles(right, top, right, bottom, queue, 0, stats, didPaint) || didPaint)
                return;
        }
    }
}

bool TiledLayerChromium::needsIdlePaint(const IntRect& visibleContentRect)
{
    // Don't trigger more paints if we failed (as we'll just fail again).
    if (m_failedUpdate || visibleContentRect.isEmpty() || m_tiler->hasEmptyBounds())
        return false;

    IntRect idlePaintContentRect = idlePaintRect(visibleContentRect);
    if (idlePaintContentRect.isEmpty())
        return false;

    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(idlePaintContentRect, left, top, right, bottom);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            ASSERT(tile); // Did setTexturePriorities get skipped?
            if (!tile)
                continue;

            bool updated = !tile->updateRect.isEmpty();
            bool canAcquire = tile->managedTexture()->canAcquireBackingTexture();
            bool dirty = tile->isDirty() || !tile->managedTexture()->haveBackingTexture();
            if (!updated && canAcquire && dirty)
                return true;
        }
    }
    return false;
}

IntRect TiledLayerChromium::idlePaintRect(const IntRect& visibleContentRect)
{
    // Don't inflate an empty rect.
    if (visibleContentRect.isEmpty())
        return visibleContentRect;

    // FIXME: This can be made a lot larger now! We should increase
    //        this slowly while insuring it doesn't cause any perf issues.
    IntRect prepaintRect = visibleContentRect;
    prepaintRect.inflateX(m_tiler->tileSize().width());
    prepaintRect.inflateY(m_tiler->tileSize().height() * 2);
    IntRect contentRect(IntPoint::zero(), contentBounds());
    prepaintRect.intersect(contentRect);

    return prepaintRect;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
