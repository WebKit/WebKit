/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CoordinatedBackingStore.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedSurface.h"
#include "GraphicsLayer.h"
#include "TextureMapper.h"
#include "TextureMapperGL.h"

using namespace WebCore;

namespace WebKit {

void CoordinatedBackingStoreTile::swapBuffers(WebCore::TextureMapper* textureMapper)
{
    if (!m_surface)
        return;

    FloatRect tileRect(m_tileRect);
    tileRect.scale(1. / m_scale);
    bool shouldReset = false;
    if (tileRect != rect()) {
        setRect(tileRect);
        shouldReset = true;
    }
    RefPtr<BitmapTexture> texture = this->texture();
    if (!texture) {
        texture = textureMapper->createTexture();
        setTexture(texture.get());
        shouldReset = true;
    }

    ASSERT(textureMapper->maxTextureSize().width() >= m_tileRect.size().width() && textureMapper->maxTextureSize().height() >= m_tileRect.size().height());
    if (shouldReset)
        texture->reset(m_tileRect.size(), m_surface->supportsAlpha());

    m_surface->copyToTexture(texture, m_sourceRect, m_surfaceOffset);
    m_surface.clear();
}

void CoordinatedBackingStoreTile::setBackBuffer(const IntRect& tileRect, const IntRect& sourceRect, PassRefPtr<CoordinatedSurface> buffer, const IntPoint& offset)
{
    m_sourceRect = sourceRect;
    m_tileRect = tileRect;
    m_surfaceOffset = offset;
    m_surface = buffer;
}

void CoordinatedBackingStore::createTile(uint32_t id, float scale)
{
    m_tiles.add(id, CoordinatedBackingStoreTile(scale));
    m_scale = scale;
}

void CoordinatedBackingStore::removeTile(uint32_t id)
{
    ASSERT(m_tiles.contains(id));
    m_tilesToRemove.add(id);
}

void CoordinatedBackingStore::removeAllTiles()
{
    HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator end = m_tiles.end();
    for (HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator it = m_tiles.begin(); it != end; ++it)
        m_tilesToRemove.add(it->key);
}

void CoordinatedBackingStore::updateTile(uint32_t id, const IntRect& sourceRect, const IntRect& tileRect, PassRefPtr<CoordinatedSurface> backBuffer, const IntPoint& offset)
{
    HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator it = m_tiles.find(id);
    ASSERT(it != m_tiles.end());
    it->value.incrementRepaintCount();
    it->value.setBackBuffer(tileRect, sourceRect, backBuffer, offset);
}

PassRefPtr<BitmapTexture> CoordinatedBackingStore::texture() const
{
    HashMap<uint32_t, CoordinatedBackingStoreTile>::const_iterator end = m_tiles.end();
    for (HashMap<uint32_t, CoordinatedBackingStoreTile>::const_iterator it = m_tiles.begin(); it != end; ++it) {
        RefPtr<BitmapTexture> texture = it->value.texture();
        if (texture)
            return texture;
    }

    return PassRefPtr<BitmapTexture>();
}

void CoordinatedBackingStore::setSize(const WebCore::FloatSize& size)
{
    m_size = size;
}

static bool shouldShowTileDebugVisuals()
{
#if PLATFORM(QT)
    return (qgetenv("QT_WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS") == "1");
#elif USE(CAIRO)
    return (String(getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS")) == "1");
#endif
    return false;
}

void CoordinatedBackingStore::paintTilesToTextureMapper(Vector<TextureMapperTile*>& tiles, TextureMapper* textureMapper, const TransformationMatrix& transform, float opacity, BitmapTexture* mask, const FloatRect& rect)
{
    for (size_t i = 0; i < tiles.size(); ++i) {
        TextureMapperTile* tile = tiles[i];
        tile->paint(textureMapper, transform, opacity, mask, calculateExposedTileEdges(rect, tile->rect()));
        static bool shouldDebug = shouldShowTileDebugVisuals();
        if (!shouldDebug)
            continue;

        textureMapper->drawBorder(Color(0xFF, 0, 0), 2, tile->rect(), transform);
        textureMapper->drawRepaintCounter(static_cast<CoordinatedBackingStoreTile*>(tile)->repaintCount(), 8, tile->rect().location(), transform);
    }
}

void CoordinatedBackingStore::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity, BitmapTexture* mask)
{
    if (m_tiles.isEmpty())
        return;
    ASSERT(!m_size.isZero());

    Vector<TextureMapperTile*> tilesToPaint;
    Vector<TextureMapperTile*> previousTilesToPaint;

    // We have to do this every time we paint, in case the opacity has changed.
    HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator end = m_tiles.end();
    FloatRect coveredRect;
    for (HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator it = m_tiles.begin(); it != end; ++it) {
        CoordinatedBackingStoreTile& tile = it->value;
        if (!tile.texture())
            continue;

        if (tile.scale() == m_scale) {
            tilesToPaint.append(&tile);
            coveredRect.unite(tile.rect());
            continue;
        }

        // Only show the previous tile if the opacity is high, otherwise effect looks like a bug.
        // We show the previous-scale tile anyway if it doesn't intersect with any current-scale tile.
        if (opacity < 0.95 && coveredRect.intersects(tile.rect()))
            continue;

        previousTilesToPaint.append(&tile);
    }

    FloatRect rectOnContents(FloatPoint::zero(), m_size);
    TransformationMatrix adjustedTransform = transform;
    // targetRect is on the contents coordinate system, so we must compare two rects on the contents coordinate system.
    // See TiledBackingStore.
    adjustedTransform.multiply(TransformationMatrix::rectToRect(rectOnContents, targetRect));

    paintTilesToTextureMapper(previousTilesToPaint, textureMapper, adjustedTransform, opacity, mask, rectOnContents);
    paintTilesToTextureMapper(tilesToPaint, textureMapper, adjustedTransform, opacity, mask, rectOnContents);
}

void CoordinatedBackingStore::commitTileOperations(TextureMapper* textureMapper)
{
    HashSet<uint32_t>::iterator tilesToRemoveEnd = m_tilesToRemove.end();
    for (HashSet<uint32_t>::iterator it = m_tilesToRemove.begin(); it != tilesToRemoveEnd; ++it)
        m_tiles.remove(*it);
    m_tilesToRemove.clear();

    HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator tilesEnd = m_tiles.end();
    for (HashMap<uint32_t, CoordinatedBackingStoreTile>::iterator it = m_tiles.begin(); it != tilesEnd; ++it)
        it->value.swapBuffers(textureMapper);
}

} // namespace WebKit
#endif
