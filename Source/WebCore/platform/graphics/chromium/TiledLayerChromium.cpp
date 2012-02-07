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
#include "MathExtras.h"
#include "Region.h"
#include "TextStream.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCTiledLayerImpl.h"
#include <wtf/CurrentTime.h>

// Start tiling when the width and height of a layer are larger than this size.
static int maxUntiledSize = 512;

// When tiling is enabled, use tiles of this dimension squared.
static int defaultTileSize = 256;

using namespace std;

namespace WebCore {

class UpdatableTile : public CCLayerTilingData::Tile {
    WTF_MAKE_NONCOPYABLE(UpdatableTile);
public:
    explicit UpdatableTile(PassOwnPtr<LayerTextureUpdater::Texture> texture) : m_texture(texture) { }

    LayerTextureUpdater::Texture* texture() { return m_texture.get(); }
    ManagedTexture* managedTexture() { return m_texture->texture(); }

    bool isDirty() const { return !m_dirtyRect.isEmpty(); }
    void copyAndClearDirty()
    {
        m_updateRect = m_dirtyRect;
        m_dirtyRect = IntRect();
    }

    IntRect m_dirtyRect;
    IntRect m_updateRect;
    IntRect m_opaqueRect;
private:
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

PassRefPtr<CCLayerImpl> TiledLayerChromium::createCCLayerImpl()
{
    return CCTiledLayerImpl::create(id());
}

void TiledLayerChromium::cleanupResources()
{
    LayerChromium::cleanupResources();

    m_tiler->reset();
    m_paintRect = IntRect();
    m_requestedUpdateTilesRect = IntRect();
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

    if (m_tilingOption == NeverTile && m_tiler->numTiles() > 1)
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
        if (tile->isDirty())
            continue;

        tiledLayer->pushTileProperties(i, j, tile->managedTexture()->textureId(), tile->m_opaqueRect);
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

UpdatableTile* TiledLayerChromium::tileAt(int i, int j) const
{
    return static_cast<UpdatableTile*>(m_tiler->tileAt(i, j));
}

UpdatableTile* TiledLayerChromium::createTile(int i, int j)
{
    RefPtr<UpdatableTile> tile = adoptRef(new UpdatableTile(textureUpdater()->createTexture(textureManager())));
    m_tiler->addTile(tile, i, j);
    tile->m_dirtyRect = m_tiler->tileRect(tile.get());

    return tile.get();
}

void TiledLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    IntRect dirty = enclosingIntRect(dirtyRect);
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
    protectTileTextures(IntRect(IntPoint::zero(), contentBounds()));
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

void TiledLayerChromium::prepareToUpdateTiles(bool idle, int left, int top, int right, int bottom)
{
    // Reset m_updateRect for all tiles.
    for (CCLayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second.get());
        tile->m_updateRect = IntRect();
    }

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

            // Do post commit deletion of current texture when partial texture
            // updates are not used.
            if (tile->isDirty() && layerTreeHost() && !layerTreeHost()->settings().partialTextureUpdates)
                layerTreeHost()->deleteTextureAfterCommit(tile->managedTexture()->steal());

            if (!tile->managedTexture()->isValid(m_tiler->tileSize(), m_textureFormat))
                tile->m_dirtyRect = m_tiler->tileRect(tile);

            if (!tile->managedTexture()->reserve(m_tiler->tileSize(), m_textureFormat)) {
                m_skipsIdlePaint = true;
                if (!idle) {
                    // If the background covers the viewport, always draw this
                    // layer so that checkerboarded tiles will still draw.
                    if (!backgroundCoversViewport())
                        m_skipsDraw = true;
                    cleanupResources();
                }
                return;
            }

            dirtyLayerRect.unite(tile->m_dirtyRect);
            tile->copyAndClearDirty();
        }
    }

    m_paintRect = dirtyLayerRect;
    if (dirtyLayerRect.isEmpty())
        return;

    // Due to borders, when the paint rect is extended to tile boundaries, it
    // may end up overlapping more tiles than the original content rect. Record
    // the original tiles so we don't upload more tiles than necessary.
    if (!m_paintRect.isEmpty())
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

            // Save what was painted opaque in the tile. If everything painted in the tile was opaque, and the area is a subset of an
            // already opaque area, keep the old area.
            IntRect tilePaintedRect = intersection(tileRect, m_paintRect);
            IntRect tilePaintedOpaqueRect = intersection(tileRect, paintedOpaqueRect);
            if (tilePaintedOpaqueRect != tilePaintedRect || !tile->m_opaqueRect.contains(tilePaintedOpaqueRect))
                tile->m_opaqueRect = tilePaintedOpaqueRect;

            // Use m_updateRect as copyAndClearDirty above moved the existing dirty rect to m_updateRect.
            const IntRect& dirtyRect = tile->m_updateRect;
            if (dirtyRect.isEmpty())
                continue;

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
        }
    }
}

void TiledLayerChromium::reserveTextures()
{
    updateBounds();

    const IntRect& layerRect = visibleLayerRect();
    if (layerRect.isEmpty() || !m_tiler->numTiles())
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

void TiledLayerChromium::addSelfToOccludedScreenSpace(Region& occludedScreenSpace)
{
    if (m_skipsDraw || drawOpacity() != 1 || !isPaintedAxisAlignedInScreen())
        return;

    if (opaque()) {
        LayerChromium::addSelfToOccludedScreenSpace(occludedScreenSpace);
        return;
    }

    IntRect visibleRect = visibleLayerRect();
    TransformationMatrix contentTransform = contentToScreenSpaceTransform();

    // FIXME: Create/Use a FloatRegion for the occludedScreenSpace, instead of a Region based on ints, to avoid this step and get better accuracy between layers in target space.
    Region tileRegion;
    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(visibleLayerRect(), left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            if (tile) {
                IntRect visibleTileOpaqueRect = intersection(visibleRect, tile->m_opaqueRect);
                FloatRect screenRect = contentTransform.mapRect(FloatRect(visibleTileOpaqueRect));
                occludedScreenSpace.unite(enclosedIntRect(screenRect));
            }
        }
    }
}

void TiledLayerChromium::prepareToUpdate(const IntRect& layerRect)
{
    m_skipsDraw = false;
    m_skipsIdlePaint = false;
    m_requestedUpdateTilesRect = IntRect();
    m_paintRect = IntRect();

    updateBounds();

    if (layerRect.isEmpty() || !m_tiler->numTiles())
        return;

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);

    prepareToUpdateTiles(false, left, top, right, bottom);
}

void TiledLayerChromium::prepareToUpdateIdle(const IntRect& layerRect)
{
    // Abort if we have already prepared a paint or run out of memory.
    if (m_skipsIdlePaint || !m_paintRect.isEmpty())
        return;

    ASSERT(m_tiler);

    updateBounds();

    if (m_tiler->isEmpty())
        return;

    // Protect any textures in the pre-paint area so we don't end up just
    // reclaiming them below.
    IntRect idlePaintLayerRect = idlePaintRect(layerRect);
    protectTileTextures(idlePaintLayerRect);

    // Expand outwards until we find a dirty row or column to update.
    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(layerRect, left, top, right, bottom);
    int prepaintLeft, prepaintTop, prepaintRight, prepaintBottom;
    m_tiler->layerRectToTileIndices(idlePaintLayerRect, prepaintLeft, prepaintTop, prepaintRight, prepaintBottom);
    while (!m_skipsIdlePaint && (left > prepaintLeft || top > prepaintTop || right < prepaintRight || bottom < prepaintBottom)) {
        if (bottom < prepaintBottom) {
            ++bottom;
            prepareToUpdateTiles(true, left, bottom, right, bottom);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (top > prepaintTop) {
            --top;
            prepareToUpdateTiles(true, left, top, right, top);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (left > prepaintLeft) {
            --left;
            prepareToUpdateTiles(true, left, top, left, bottom);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
        if (right < prepaintRight) {
            ++right;
            prepareToUpdateTiles(true, right, top, right, bottom);
            if (!m_paintRect.isEmpty() || m_skipsIdlePaint)
                break;
        }
    }
}

bool TiledLayerChromium::needsIdlePaint(const IntRect& layerRect)
{
    if (m_skipsIdlePaint)
        return false;

    IntRect idlePaintLayerRect = idlePaintRect(layerRect);

    int left, top, right, bottom;
    m_tiler->layerRectToTileIndices(idlePaintLayerRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
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
