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
#include "BitmapTexture.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsLayer.h"
#include "TextureMapper.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

void CoordinatedBackingStore::createTile(uint32_t id)
{
    m_tiles.add(id, CoordinatedBackingStoreTile(m_scale));
}

void CoordinatedBackingStore::removeTile(uint32_t id)
{
    ASSERT(m_tiles.contains(id));
    m_tiles.remove(id);
}

void CoordinatedBackingStore::updateTile(uint32_t id, const IntRect& sourceRect, const IntRect& tileRect, RefPtr<CoordinatedTileBuffer>&& buffer, const IntPoint& offset)
{
    auto it = m_tiles.find(id);
    ASSERT(it != m_tiles.end());
    it->value.addUpdate({ WTFMove(buffer), sourceRect, tileRect, offset });
}

void CoordinatedBackingStore::processPendingUpdates(TextureMapper& textureMapper)
{
    for (auto& tile : m_tiles.values())
        tile.processPendingUpdates(textureMapper);
}

void CoordinatedBackingStore::resize(const FloatSize& size, float scale)
{
    m_size = size;
    m_scale = scale;
}

void CoordinatedBackingStore::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity)
{
    if (m_tiles.isEmpty())
        return;

    ASSERT(!m_size.isZero());

    Vector<CoordinatedBackingStoreTile*, 16> tilesToPaint;
    Vector<CoordinatedBackingStoreTile*, 16> previousTilesToPaint;

    // We have to do this every time we paint, in case the opacity has changed.
    FloatRect coveredRect;
    for (auto& tile : m_tiles.values()) {
        if (!tile.canBePainted())
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

    // targetRect is on the contents coordinate system, so we must compare two rects on the contents coordinate system.
    // See CoodinatedBackingStoreProxy.
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);

    auto paintTile = [&](CoordinatedBackingStoreTile& tile) {
        textureMapper.drawTexture(tile.texture(), tile.rect(), adjustedTransform, opacity, allTileEdgesExposed(layerRect, tile.rect()) ? TextureMapper::AllEdgesExposed::Yes : TextureMapper::AllEdgesExposed::No);
    };

    for (auto* tile : previousTilesToPaint)
        paintTile(*tile);
    for (auto* tile : tilesToPaint)
        paintTile(*tile);
}

void CoordinatedBackingStore::drawBorder(TextureMapper& textureMapper, const Color& borderColor, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);
    for (auto& tile : m_tiles.values())
        textureMapper.drawBorder(borderColor, borderWidth, tile.rect(), adjustedTransform);
}

void CoordinatedBackingStore::drawRepaintCounter(TextureMapper& textureMapper, int repaintCount, const Color& borderColor, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);
    for (auto& tile : m_tiles.values())
        textureMapper.drawNumber(repaintCount, borderColor, tile.rect().location(), adjustedTransform);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
