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

void CoordinatedBackingStore::updateTile(uint32_t id, const IntRect& sourceRect, const IntRect& tileRect, Ref<CoordinatedTileBuffer>&& buffer, const IntPoint& offset)
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
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);
    for (const auto& tile : m_tiles.values()) {
        ASSERT(tile.scale() == m_scale);
        const auto allEdgesExposed = allTileEdgesExposed(layerRect, tile.rect()) ? TextureMapper::AllEdgesExposed::Yes : TextureMapper::AllEdgesExposed::No;
#if ENABLE(DAMAGE_TRACKING)
        const auto canUseDamageToDrawTextureFragment = [&]() {
            return !textureMapper.damage().isInvalid()
                && !textureMapper.damage().isEmpty()
                && adjustedTransform.isIdentity()
                && allEdgesExposed == TextureMapper::AllEdgesExposed::No
                && opacity == 1.0
                && tile.texture().isOpaque()
                && !tile.texture().filterOperation();
        }();
        if (canUseDamageToDrawTextureFragment) {
            // We define damagedTileRect as a minimum bounding rectangle of all damage rects that intersect tile.rect()
            // - this way we can keep a single texture draw call yet with potentially smaller sourceRect.
            FloatRect damagedTileRect;
            for (const auto& damageRect : textureMapper.damage().rects()) {
                if (!damageRect.isEmpty())
                    damagedTileRect.unite(intersection(tile.rect(), damageRect));
            }
            if (damagedTileRect.isEmpty())
                continue;
            const auto sourceRect = FloatRect { FloatPoint { damagedTileRect.location() - tile.rect().location() }, damagedTileRect.size() };
            textureMapper.drawTextureFragment(tile.texture(), sourceRect, damagedTileRect);
        } else
#endif
        textureMapper.drawTexture(tile.texture(), tile.rect(), adjustedTransform, opacity, allEdgesExposed);
    }
}

void CoordinatedBackingStore::drawBorder(TextureMapper& textureMapper, const Color& borderColor, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    ASSERT(!m_size.isZero());
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);
    for (const auto& tile : m_tiles.values())
        textureMapper.drawBorder(borderColor, borderWidth, tile.rect(), adjustedTransform);
}

void CoordinatedBackingStore::drawRepaintCounter(TextureMapper& textureMapper, int repaintCount, const Color& borderColor, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    ASSERT(!m_size.isZero());
    FloatRect layerRect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * TransformationMatrix::rectToRect(layerRect, targetRect);
    for (const auto& tile : m_tiles.values())
        textureMapper.drawNumber(repaintCount, borderColor, tile.rect().location(), adjustedTransform);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
