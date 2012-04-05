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
#include "LayerRendererChromium.h"
#include "ManagedTexture.h"
#include "Region.h"
#include "TextStream.h"
#include "TraceEvent.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCTiledLayerImpl.h"

#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>

// Start tiling when the width and height of a layer are larger than this size.
static int maxUntiledSize = 512;

// When tiling is enabled, use tiles of this dimension squared.
static int defaultTileSize = 256;

using namespace std;

namespace WebCore {

class UpdatableTile : public CCLayerTilingData::Tile {
    WTF_MAKE_NONCOPYABLE(UpdatableTile);
public:
    static PassOwnPtr<UpdatableTile> create(PassOwnPtr<LayerTextureUpdater::Texture> texture)
    {
        return adoptPtr(new UpdatableTile(texture));
    }

    LayerTextureUpdater::Texture* texture() { return m_texture.get(); }
    ManagedTexture* managedTexture() { return m_texture->texture(); }

    bool isDirty() const { return !m_dirtyRect.isEmpty(); }
    void copyAndClearDirty()
    {
        m_updateRect = m_dirtyRect;
        m_dirtyRect = IntRect();
    }
    // Returns whether the layer was dirty and not updated in the current frame. For tiles that were not culled, the
    // updateRect holds the area of the tile that was updated. Otherwise, the area that would have been updated.
    bool isDirtyForCurrentFrame() { return !m_dirtyRect.isEmpty() && (m_updateRect.isEmpty() || !m_updated); }

    IntRect m_dirtyRect;
    IntRect m_updateRect;
    bool m_partialUpdate;
    bool m_updated;
private:
    explicit UpdatableTile(PassOwnPtr<LayerTextureUpdater::Texture> texture)
        : m_partialUpdate(false)
        , m_updated(false)
        , m_texture(texture)
    {
    }

    OwnPtr<LayerTextureUpdater::Texture> m_texture;
};

TiledLayerChromium::TiledLayerChromium()
    : LayerChromium()
    , m_textureFormat(GraphicsContext3D::INVALID_ENUM)
    , m_skipsDraw(false)
    , m_skipsIdlePaint(false)
    , m_sampledTexelFormat(LayerTextureUpdater::SampledTexelFormatInvalid)
    , m_tilingOption(AutoTile)
{
    m_tiler = CCLayerTilingData::create(IntSize(defaultTileSize, defaultTileSize), CCLayerTilingData::HasBorderTexels);
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
    const IntSize tileSize(min(defaultTileSize, contentBounds().width()), min(defaultTileSize, contentBounds().height()));

    // Tile if both dimensions large, or any one dimension large and the other
    // extends into a second tile. This heuristic allows for long skinny layers
    // (e.g. scrollbars) that are Nx1 tiles to minimize wasted texture space.
    const bool anyDimensionLarge = contentBounds().width() > maxUntiledSize || contentBounds().height() > maxUntiledSize;
    const bool anyDimensionOneTile = contentBounds().width() <= defaultTileSize || contentBounds().height() <= defaultTileSize;
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
        invalidateRect(rects[i]);
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

void TiledLayerChromium::updateCompositorResources(GraphicsContext3D*, CCTextureUpdater& updater)
{
    // Painting could cause compositing to get turned off, which may cause the tiler to become invalidated mid-update.
    if (m_skipsDraw || m_requestedUpdateTilesRect.isEmpty() || m_tiler->isEmpty())
        return;

    int left = m_requestedUpdateTilesRect.x();
    int top = m_requestedUpdateTilesRect.y();
    int right = m_requestedUpdateTilesRect.maxX() - 1;
    int bottom = m_requestedUpdateTilesRect.maxY() - 1;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);

            // Required tiles are created in prepareToUpdate(). A tile should
            // never be removed between the call to prepareToUpdate() and the
            // call to updateCompositorResources().
            if (!tile)
                CRASH();

            if (!tile->m_updated)
                continue;

            IntRect sourceRect = tile->m_updateRect;
            if (tile->m_updateRect.isEmpty())
                continue;

            ASSERT(tile->managedTexture()->isReserved());
            const IntPoint anchor = m_tiler->tileRect(tile).location();

            // Calculate tile-space rectangle to upload into.
            IntRect destRect(IntPoint(sourceRect.x() - anchor.x(), sourceRect.y() - anchor.y()), sourceRect.size());
            if (destRect.x() < 0)
                CRASH();
            if (destRect.y() < 0)
                CRASH();

            // Offset from paint rectangle to this tile's dirty rectangle.
            IntPoint paintOffset(sourceRect.x() - m_paintRect.x(), sourceRect.y() - m_paintRect.y());
            if (paintOffset.x() < 0)
                CRASH();
            if (paintOffset.y() < 0)
                CRASH();
            if (paintOffset.x() + destRect.width() > m_paintRect.width())
                CRASH();
            if (paintOffset.y() + destRect.height() > m_paintRect.height())
                CRASH();

            if (tile->m_partialUpdate)
                updater.appendPartial(tile->texture(), sourceRect, destRect);
            else
                updater.append(tile->texture(), sourceRect, destRect);
        }
    }

    // The updateRect should be in layer space. So we have to convert the paintRect from content space to layer space.
    m_updateRect = FloatRect(m_paintRect);
    float widthScale = bounds().width() / static_cast<float>(contentBounds().width());
    float heightScale = bounds().height() / static_cast<float>(contentBounds().height());
    m_updateRect.scale(widthScale, heightScale);
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
        if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat)) {
            invalidTiles.append(tile);
            continue;
        }
        if (tile->isDirtyForCurrentFrame())
            continue;

        tiledLayer->pushTileProperties(i, j, tile->managedTexture()->textureId(), tile->opaqueRect());
    }
    for (Vector<UpdatableTile*>::const_iterator iter = invalidTiles.begin(); iter != invalidTiles.end(); ++iter)
        m_tiler->takeTile((*iter)->i(), (*iter)->j());
}

TextureManager* TiledLayerChromium::textureManager() const
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
    OwnPtr<UpdatableTile> tile(UpdatableTile::create(textureUpdater()->createTexture(textureManager())));
    UpdatableTile* addedTile = tile.get();
    m_tiler->addTile(tile.release(), i, j);

    addedTile->m_dirtyRect = m_tiler->tileRect(addedTile);
    return addedTile;
}

void TiledLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    FloatRect scaledDirtyRect(dirtyRect);
    scaledDirtyRect.scale(contentsScale());
    IntRect dirty = enclosingIntRect(scaledDirtyRect);
    invalidateRect(dirty);
    LayerChromium::setNeedsDisplayRect(dirtyRect);
}

void TiledLayerChromium::setIsNonCompositedContent(bool isNonCompositedContent)
{
    LayerChromium::setIsNonCompositedContent(isNonCompositedContent);

    CCLayerTilingData::BorderTexelOption borderTexelOption;
#if OS(ANDROID)
    // Always want border texels and GL_LINEAR due to pinch zoom.
    borderTexelOption = CCLayerTilingData::HasBorderTexels;
#else
    borderTexelOption = isNonCompositedContent ? CCLayerTilingData::NoBorderTexels : CCLayerTilingData::HasBorderTexels;
#endif
    setBorderTexelOption(borderTexelOption);
}

void TiledLayerChromium::invalidateRect(const IntRect& layerRect)
{
    updateBounds();
    if (m_tiler->isEmpty() || layerRect.isEmpty() || m_skipsDraw)
        return;

    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        ASSERT(tile);
        IntRect bound = m_tiler->tileRect(tile);
        bound.intersect(layerRect);
        tile->m_dirtyRect.unite(bound);
    }
}

void TiledLayerChromium::protectVisibleTileTextures()
{
    protectTileTextures(visibleLayerRect());
}

void TiledLayerChromium::protectTileTextures(const IntRect& layerRect)
{
    if (m_tiler->isEmpty() || layerRect.isEmpty())
        return;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            if (!tile || !tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat))
                continue;

            tile->managedTexture()->reserve(m_tiler->tileSize(), m_textureFormat);
        }
    }
}

// Returns true if tile is dirty and only part of it needs to be updated.
bool TiledLayerChromium::tileOnlyNeedsPartialUpdate(UpdatableTile* tile)
{
    if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat))
        return false;

    if (!tile->isDirty())
        return false;

    return !tile->m_dirtyRect.contains(m_tiler->tileRect(tile));
}

// Dirty tiles with valid textures needs buffered update to guarantee that
// we don't modify textures currently used for drawing by the impl thread.
bool TiledLayerChromium::tileNeedsBufferedUpdate(UpdatableTile* tile)
{
    // No impl thread?.
    if (!CCProxy::hasImplThread())
        return false;

    if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat))
        return false;

    if (!tile->isDirty())
        return false;

    return true;
}

void TiledLayerChromium::prepareToUpdateTiles(bool idle, int left, int top, int right, int bottom, const CCOcclusionTracker* occlusion)
{
    createTextureUpdaterIfNeeded();

    // Create tiles as needed, expanding a dirty rect to contain all
    // the dirty regions currently being drawn. All dirty tiles that are to be painted
    // get their m_updateRect set to m_dirtyRect and m_dirtyRect cleared. This way if
    // invalidateRect is invoked during prepareToUpdate we don't lose the request.
    IntRect dirtyLayerRect;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            if (!tile)
                tile = createTile(i, j);

            if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat)) {
                // Sets the dirty rect to a full-sized tile with border texels.
                tile->m_dirtyRect = m_tiler->tileRect(tile);
            }

            // When not idle painting, if the visible region of the tile is occluded, don't reserve a texture or update the tile.
            // If any part of the tile is visible, then we need to update it so the tile is pushed to the impl thread.
            if (!idle && occlusion) {
                IntRect visibleTileRect = intersection(m_tiler->tileBounds(i, j), visibleLayerRect());
                if (occlusion->occluded(this, visibleTileRect)) {
                    ASSERT(!tile->m_updated);
                    continue;
                }
            }

            // We come through this function multiple times during a commit, and m_updated should be true if the tile is not culled
            // any single time through the function.
            tile->m_updated = true;

            // FIXME: Decide if partial update should be allowed based on cost
            // of update. https://bugs.webkit.org/show_bug.cgi?id=77376
            if (tileOnlyNeedsPartialUpdate(tile) && layerTreeHost() && layerTreeHost()->requestPartialTextureUpdate())
                tile->m_partialUpdate = true;
            else if (tileNeedsBufferedUpdate(tile) && layerTreeHost()) {
                layerTreeHost()->deleteTextureAfterCommit(tile->managedTexture()->steal());
                // Sets the dirty rect to a full-sized tile with border texels.
                tile->m_dirtyRect = m_tiler->tileRect(tile);
            }

            if (!tile->managedTexture()->reserve(m_tiler->tileSize(), m_textureFormat)) {
                m_skipsIdlePaint = true;
                if (!idle) {
                    // If the background covers the viewport, always draw this
                    // layer so that checkerboarded tiles will still draw.
                    if (!backgroundCoversViewport())
                        m_skipsDraw = true;
                    m_tiler->reset();
                    m_paintRect = IntRect();
                    m_requestedUpdateTilesRect = IntRect();
                }
                return;
            }

            dirtyLayerRect.unite(tile->m_dirtyRect);
        }
    }

    // For tiles that were not culled, we are going to update the area currently marked as dirty. So
    // clear that dirty area and mark it for update instead.
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            if (tile->m_updated)
                tile->copyAndClearDirty();
            else if (!idle && occlusion && tile->isDirty())
                occlusion->overdrawMetrics().didCullTileForUpload();
        }
    }

    m_paintRect = dirtyLayerRect;
    if (dirtyLayerRect.isEmpty())
        return;

    if (occlusion)
        occlusion->overdrawMetrics().didPaint(m_paintRect);

    // Due to borders, when the paint rect is extended to tile boundaries, it
    // may end up overlapping more tiles than the original content rect. Record
    // the original tiles so we don't upload more tiles than necessary.
    m_requestedUpdateTilesRect = IntRect(left, top, right - left + 1, bottom - top + 1);

    // Calling prepareToUpdate() calls into WebKit to paint, which may have the side
    // effect of disabling compositing, which causes our reference to the texture updater to be deleted.
    // However, we can't free the memory backing the GraphicsContext until the paint finishes,
    // so we grab a local reference here to hold the updater alive until the paint completes.
    RefPtr<LayerTextureUpdater> protector(textureUpdater());
    IntRect paintedOpaqueRect;
    textureUpdater()->prepareToUpdate(m_paintRect, m_tiler->tileSize(), m_tiler->hasBorderTexels(), contentsScale(), &paintedOpaqueRect);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);

            // Tiles are created before prepareToUpdate() is called.
            if (!tile)
                CRASH();

            IntRect tileRect = m_tiler->tileBounds(i, j);

            if (!tile->m_updated)
                continue;

            // Use m_updateRect as the above loop copied the dirty rect for this frame to m_updateRect.
            const IntRect& dirtyRect = tile->m_updateRect;
            if (dirtyRect.isEmpty())
                continue;

            // Save what was painted opaque in the tile. Keep the old area if the paint didn't touch it, and didn't paint some
            // other part of the tile opaque.
            IntRect tilePaintedRect = intersection(tileRect, m_paintRect);
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
            sourceRect.intersect(m_paintRect);

            tile->m_updateRect = sourceRect;
            if (sourceRect.isEmpty())
                continue;

            tile->texture()->prepareRect(sourceRect);
            if (occlusion)
                occlusion->overdrawMetrics().didUpload(TransformationMatrix(), sourceRect, tile->opaqueRect());
        }
    }
}

void TiledLayerChromium::reserveTextures()
{
    updateBounds();

    const IntRect& layerRect = visibleLayerRect();
    if (layerRect.isEmpty() || m_tiler->hasEmptyBounds())
        return;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);

    createTextureUpdaterIfNeeded();
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            if (!tile)
                tile = createTile(i, j);

            if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat))
                tile->m_dirtyRect = m_tiler->tileRect(tile);

            if (!tile->managedTexture()->reserve(m_tiler->tileSize(), m_textureFormat))
                return;
        }
    }
}

Region TiledLayerChromium::visibleContentOpaqueRegion() const
{
    if (m_skipsDraw)
        return Region();

    return m_tiler->opaqueRegionInLayerRect(visibleLayerRect());
}

void TiledLayerChromium::resetUpdateState()
{
    // Reset m_updateRect for all tiles.
    CCLayerTilingData::TileMap::const_iterator end = m_tiler->tiles().end();
    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != end; ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        tile->m_updateRect = IntRect();
        tile->m_partialUpdate = false;
        tile->m_updated = false;
    }
}

void TiledLayerChromium::prepareToUpdate(const IntRect& layerRect, const CCOcclusionTracker* occlusion)
{
    m_skipsDraw = false;
    m_skipsIdlePaint = false;
    m_requestedUpdateTilesRect = IntRect();
    m_paintRect = IntRect();

    updateBounds();

    resetUpdateState();

    if (layerRect.isEmpty() || m_tiler->hasEmptyBounds())
        return;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);

    prepareToUpdateTiles(false, left, top, right, bottom, occlusion);
}

void TiledLayerChromium::prepareToUpdateIdle(const IntRect& layerRect, const CCOcclusionTracker* occlusion)
{
    // Abort if we have already prepared a paint or run out of memory.
    if (m_skipsIdlePaint || !m_paintRect.isEmpty())
        return;

    ASSERT(m_tiler);

    updateBounds();

    if (m_tiler->hasEmptyBounds())
        return;

    IntRect idlePaintLayerRect = idlePaintRect(layerRect);
    if (idlePaintLayerRect.isEmpty())
        return;

    // Protect any textures in the pre-paint area, as we would steal them from other layers
    // over time anyhow. This ensures we don't lose tiles in the first rounds of idle painting
    // that we have already painted.
    protectTileTextures(idlePaintLayerRect);

    int prepaintLeft, prepaintTop, prepaintRight, prepaintBottom;
    m_tiler->layerRectToTileIndices(idlePaintLayerRect, prepaintLeft, prepaintTop, prepaintRight, prepaintBottom);

    // If the layer is not visible, we have nothing to expand from, so instead we prepaint the outer-most set of tiles.
    if (layerRect.isEmpty()) {
        prepareToUpdateTiles(true, prepaintLeft, prepaintTop, prepaintRight, prepaintTop, 0);
        if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
            return;
        prepareToUpdateTiles(true, prepaintLeft, prepaintBottom, prepaintRight, prepaintBottom, 0);
        if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
            return;
        prepareToUpdateTiles(true, prepaintLeft, prepaintTop, prepaintLeft, prepaintBottom, 0);
        if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
            return;
        prepareToUpdateTiles(true, prepaintRight, prepaintTop, prepaintRight, prepaintBottom, 0);

        return;
    }

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);

    // Otherwise, prepaint anything that was occluded but inside the layer's visible region.
    prepareToUpdateTiles(true, left, top, right, bottom, 0);
    if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
        return;

    // Then expand outwards from the visible area until we find a dirty row or column to update.
    while (!m_skipsIdlePaint && (left > prepaintLeft || top > prepaintTop || right < prepaintRight || bottom < prepaintBottom)) {
        if (bottom < prepaintBottom) {
            ++bottom;
            prepareToUpdateTiles(true, left, bottom, right, bottom, 0);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (top > prepaintTop) {
            --top;
            prepareToUpdateTiles(true, left, top, right, top, 0);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (left > prepaintLeft) {
            --left;
            prepareToUpdateTiles(true, left, top, left, bottom, 0);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (right < prepaintRight) {
            ++right;
            prepareToUpdateTiles(true, right, top, right, bottom, 0);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
    }
}

bool TiledLayerChromium::needsIdlePaint(const IntRect& layerRect)
{
    if (m_skipsIdlePaint)
        return false;

    if (m_tiler->hasEmptyBounds())
        return false;

    IntRect idlePaintLayerRect = idlePaintRect(layerRect);
    if (idlePaintLayerRect.isEmpty())
        return false;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(idlePaintLayerRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            // If the layerRect is empty, then we are painting the outer-most set of tiles only.
            if (layerRect.isEmpty() && i != left && i != right && j != top && j != bottom)
                continue;
            if (m_requestedUpdateTilesRect.contains(IntPoint(i, j)))
                continue;
            UpdatableTile* tile = tileAt(i, j);
            if (!tile || !tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat) || tile->isDirty())
                return true;
        }
    }
    return false;
}

IntRect TiledLayerChromium::idlePaintRect(const IntRect& visibleLayerRect)
{
    // For layers that are animating transforms but not visible at all, we don't know what part
    // of them is going to become visible. For small layers we return the entire layer, for larger
    // ones we avoid prepainting the layer at all.
    if (visibleLayerRect.isEmpty()) {
        bool isSmallLayer = m_tiler->numTilesX() <= 9 && m_tiler->numTilesY() <= 9 && m_tiler->numTilesX() * m_tiler->numTilesY() <= 9;
        if ((drawTransformIsAnimating() || screenSpaceTransformIsAnimating()) && isSmallLayer)
            return IntRect(IntPoint(), contentBounds());
        return IntRect();
    }

    IntRect prepaintRect = visibleLayerRect;
    // FIXME: This can be made a lot larger if we can:
    // - reserve memory at a lower priority than for visible content
    // - only reserve idle paint tiles up to a memory reclaim threshold and
    // - insure we play nicely with other layers
    prepaintRect.inflateX(m_tiler->tileSize().width());
    prepaintRect.inflateY(m_tiler->tileSize().height());
    prepaintRect.intersect(IntRect(IntPoint::zero(), contentBounds()));
    return prepaintRect;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
