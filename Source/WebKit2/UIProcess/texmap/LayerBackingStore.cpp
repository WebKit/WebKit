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
#include "LayerBackingStore.h"

#if USE(UI_SIDE_COMPOSITING)
#include "GraphicsLayer.h"
#include "ShareableSurface.h"
#include "TextureMapper.h"
#include "TextureMapperGL.h"

using namespace WebCore;

namespace WebKit {

void LayerBackingStoreTile::swapBuffers(WebCore::TextureMapper* textureMapper)
{
    if (!m_surface)
        return;

    FloatRect targetRect(m_targetRect);
    targetRect.scale(1. / m_scale);
    bool shouldReset = false;
    if (targetRect != rect()) {
        setRect(targetRect);
        shouldReset = true;
    }
    RefPtr<BitmapTexture> texture = this->texture();
    if (!texture) {
        texture = textureMapper->createTexture();
        setTexture(texture.get());
        shouldReset = true;
    }

    if (shouldReset)
        texture->reset(m_targetRect.size(), m_surface->flags() & ShareableBitmap::SupportsAlpha ? BitmapTexture::SupportsAlpha : 0);

    m_surface->copyToTexture(texture, m_sourceRect, m_surfaceOffset);
    m_surface.clear();
}

void LayerBackingStoreTile::setBackBuffer(const IntRect& targetRect, const IntRect& sourceRect, PassRefPtr<ShareableSurface> buffer, const IntPoint& offset)
{
    m_sourceRect = sourceRect;
    m_targetRect = targetRect;
    m_surfaceOffset = offset;
    m_surface = buffer;
}

void LayerBackingStore::createTile(int id, float scale)
{
    m_tiles.add(id, LayerBackingStoreTile(scale));
    m_scale = scale;
}

void LayerBackingStore::removeTile(int id)
{
    m_tilesToRemove.append(id);
}


void LayerBackingStore::updateTile(int id, const IntRect& sourceRect, const IntRect& targetRect, PassRefPtr<ShareableSurface> backBuffer, const IntPoint& offset)
{
    HashMap<int, LayerBackingStoreTile>::iterator it = m_tiles.find(id);
    ASSERT(it != m_tiles.end());
    it->second.setBackBuffer(targetRect, sourceRect, backBuffer, offset);
}

PassRefPtr<BitmapTexture> LayerBackingStore::texture() const
{
    HashMap<int, LayerBackingStoreTile>::const_iterator end = m_tiles.end();
    for (HashMap<int, LayerBackingStoreTile>::const_iterator it = m_tiles.begin(); it != end; ++it) {
        RefPtr<BitmapTexture> texture = it->second.texture();
        if (texture)
            return texture;
    }

    return PassRefPtr<BitmapTexture>();
}

void LayerBackingStore::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity, BitmapTexture* mask)
{
    Vector<TextureMapperTile*> tilesToPaint;

    // We have to do this every time we paint, in case the opacity has changed.
    HashMap<int, LayerBackingStoreTile>::iterator end = m_tiles.end();
    FloatRect coveredRect;
    for (HashMap<int, LayerBackingStoreTile>::iterator it = m_tiles.begin(); it != end; ++it) {
        LayerBackingStoreTile& tile = it->second;
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

        tilesToPaint.prepend(&tile);
    }

    for (size_t i = 0; i < tilesToPaint.size(); ++i)
        tilesToPaint[i]->paint(textureMapper, transform, opacity, mask);
}

void LayerBackingStore::commitTileOperations(TextureMapper* textureMapper)
{
    Vector<int>::iterator tilesToRemoveEnd = m_tilesToRemove.end();
    for (Vector<int>::iterator it = m_tilesToRemove.begin(); it != tilesToRemoveEnd; ++it)
        m_tiles.remove(*it);
    m_tilesToRemove.clear();

    HashMap<int, LayerBackingStoreTile>::iterator tilesEnd = m_tiles.end();
    for (HashMap<int, LayerBackingStoreTile>::iterator it = m_tiles.begin(); it != tilesEnd; ++it)
        it->second.swapBuffers(textureMapper);
}

} // namespace WebKit
#endif
