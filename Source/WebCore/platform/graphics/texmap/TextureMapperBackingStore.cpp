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

#if USE(ACCELERATED_COMPOSITING)
#include "TextureMapperBackingStore.h"

#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "TextureMapper.h"

#if USE(GRAPHICS_SURFACE)
#include "GraphicsSurface.h"
#include "TextureMapperGL.h"
#endif

namespace WebCore {

#if USE(GRAPHICS_SURFACE)
void TextureMapperSurfaceBackingStore::setGraphicsSurface(PassRefPtr<GraphicsSurface> surface)
{
    m_graphicsSurface = surface;
}

void TextureMapperSurfaceBackingStore::swapBuffersIfNeeded(uint32_t)
{
    if (m_graphicsSurface)
        m_graphicsSurface->swapBuffers();
}

PassRefPtr<BitmapTexture> TextureMapperSurfaceBackingStore::texture() const
{
    // FIXME: Instead of just returning an empty texture, we should wrap the texture contents into a BitmapTexture.
    RefPtr<BitmapTexture> emptyTexture;
    return emptyTexture;
}

void TextureMapperSurfaceBackingStore::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity, BitmapTexture* mask)
{
    if (m_graphicsSurface)
        m_graphicsSurface->paintToTextureMapper(textureMapper, targetRect, transform, opacity, mask);
}
#endif

void TextureMapperTile::updateContents(TextureMapper* textureMapper, Image* image, const IntRect& dirtyRect, BitmapTexture::UpdateContentsFlag updateContentsFlag)
{
    IntRect targetRect = enclosingIntRect(m_rect);
    targetRect.intersect(dirtyRect);
    if (targetRect.isEmpty())
        return;
    IntPoint sourceOffset = targetRect.location();

    // Normalize sourceRect to the buffer's coordinates.
    sourceOffset.move(-dirtyRect.x(), -dirtyRect.y());

    // Normalize targetRect to the texture's coordinates.
    targetRect.move(-m_rect.x(), -m_rect.y());
    if (!m_texture) {
        m_texture = textureMapper->createTexture();
        m_texture->reset(targetRect.size(), image->currentFrameHasAlpha() ? BitmapTexture::SupportsAlpha : 0);
    }

    m_texture->updateContents(image, targetRect, sourceOffset, updateContentsFlag);
}

void TextureMapperTile::updateContents(TextureMapper* textureMapper, GraphicsLayer* sourceLayer, const IntRect& dirtyRect, BitmapTexture::UpdateContentsFlag updateContentsFlag)
{
    IntRect targetRect = enclosingIntRect(m_rect);
    targetRect.intersect(dirtyRect);
    if (targetRect.isEmpty())
        return;
    IntPoint sourceOffset = targetRect.location();

    // Normalize targetRect to the texture's coordinates.
    targetRect.move(-m_rect.x(), -m_rect.y());

    if (!m_texture) {
        m_texture = textureMapper->createTexture();
        m_texture->reset(targetRect.size(), BitmapTexture::SupportsAlpha);
    }

    m_texture->updateContents(textureMapper, sourceLayer, targetRect, sourceOffset, updateContentsFlag);
}

void TextureMapperTile::paint(TextureMapper* textureMapper, const TransformationMatrix& transform, float opacity, BitmapTexture* mask, const unsigned exposedEdges)
{
    if (texture().get())
        textureMapper->drawTexture(*texture().get(), rect(), transform, opacity, mask, exposedEdges);
}

TextureMapperTiledBackingStore::TextureMapperTiledBackingStore()
    : m_drawsDebugBorders(false)
{
}

void TextureMapperTiledBackingStore::updateContentsFromImageIfNeeded(TextureMapper* textureMapper)
{
    if (!m_image)
        return;

    updateContents(textureMapper, m_image.get(), m_image->size(), m_image->rect(), BitmapTexture::UpdateCannotModifyOriginalImageData);
    m_image.clear();
}

unsigned TextureMapperBackingStore::calculateExposedTileEdges(const FloatRect& totalRect, const FloatRect& tileRect)
{
    unsigned exposedEdges = TextureMapper::NoEdges;
    if (!tileRect.x())
        exposedEdges |= TextureMapper::LeftEdge;
    if (!tileRect.y())
        exposedEdges |= TextureMapper::TopEdge;
    if (tileRect.width() + tileRect.x() >= totalRect.width())
        exposedEdges |= TextureMapper::RightEdge;
    if (tileRect.height() + tileRect.y() >= totalRect.height())
        exposedEdges |= TextureMapper::BottomEdge;
    return exposedEdges;
}

void TextureMapperTiledBackingStore::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity, BitmapTexture* mask)
{
    updateContentsFromImageIfNeeded(textureMapper);
    TransformationMatrix adjustedTransform = transform;
    adjustedTransform.multiply(TransformationMatrix::rectToRect(rect(), targetRect));
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        m_tiles[i].paint(textureMapper, adjustedTransform, opacity, mask, calculateExposedTileEdges(rect(), m_tiles[i].rect()));
        if (m_drawsDebugBorders)
            textureMapper->drawBorder(m_debugBorderColor, m_debugBorderWidth, m_tiles[i].rect(), adjustedTransform);
    }
}

void TextureMapperTiledBackingStore::createOrDestroyTilesIfNeeded(const FloatSize& size, const IntSize& tileSize, bool hasAlpha)
{
    if (size == m_size)
        return;

    m_size = size;

    Vector<FloatRect> tileRectsToAdd;
    Vector<int> tileIndicesToRemove;
    static const size_t TileEraseThreshold = 6;

    // This method recycles tiles. We check which tiles we need to add, which to remove, and use as many
    // removable tiles as replacement for new tiles when possible.
    for (float y = 0; y < m_size.height(); y += tileSize.height()) {
        for (float x = 0; x < m_size.width(); x += tileSize.width()) {
            FloatRect tileRect(x, y, tileSize.width(), tileSize.height());
            tileRect.intersect(rect());
            tileRectsToAdd.append(tileRect);
        }
    }

    // Check which tiles need to be removed, and which already exist.
    for (int i = m_tiles.size() - 1; i >= 0; --i) {
        FloatRect oldTile = m_tiles[i].rect();
        bool existsAlready = false;

        for (int j = tileRectsToAdd.size() - 1; j >= 0; --j) {
            FloatRect newTile = tileRectsToAdd[j];
            if (oldTile != newTile)
                continue;

            // A tile that we want to add already exists, no need to add or remove it.
            existsAlready = true;
            tileRectsToAdd.remove(j);
            break;
        }

        // This tile is not needed.
        if (!existsAlready)
            tileIndicesToRemove.append(i);
    }

    // Recycle removable tiles to be used for newly requested tiles.
    for (size_t i = 0; i < tileRectsToAdd.size(); ++i) {
        if (!tileIndicesToRemove.isEmpty()) {
            // We recycle an existing tile for usage with a new tile rect.
            TextureMapperTile& tile = m_tiles[tileIndicesToRemove.last()];
            tileIndicesToRemove.removeLast();
            tile.setRect(tileRectsToAdd[i]);

            if (tile.texture())
                tile.texture()->reset(enclosingIntRect(tile.rect()).size(), hasAlpha ? BitmapTexture::SupportsAlpha : 0);
            continue;
        }

        m_tiles.append(TextureMapperTile(tileRectsToAdd[i]));
    }

    // Remove unnecessary tiles, if they weren't recycled.
    // We use a threshold to make sure we don't create/destroy tiles too eagerly.
    for (size_t i = 0; i < tileIndicesToRemove.size() && m_tiles.size() > TileEraseThreshold; ++i)
        m_tiles.remove(tileIndicesToRemove[i]);
}

void TextureMapperTiledBackingStore::updateContents(TextureMapper* textureMapper, Image* image, const FloatSize& totalSize, const IntRect& dirtyRect, BitmapTexture::UpdateContentsFlag updateContentsFlag)
{
    createOrDestroyTilesIfNeeded(totalSize, textureMapper->maxTextureSize(), image->currentFrameHasAlpha());
    for (size_t i = 0; i < m_tiles.size(); ++i)
        m_tiles[i].updateContents(textureMapper, image, dirtyRect, updateContentsFlag);
}

void TextureMapperTiledBackingStore::updateContents(TextureMapper* textureMapper, GraphicsLayer* sourceLayer, const FloatSize& totalSize, const IntRect& dirtyRect, BitmapTexture::UpdateContentsFlag updateContentsFlag)
{
    createOrDestroyTilesIfNeeded(totalSize, textureMapper->maxTextureSize(), true);
    for (size_t i = 0; i < m_tiles.size(); ++i)
        m_tiles[i].updateContents(textureMapper, sourceLayer, dirtyRect, updateContentsFlag);
}

PassRefPtr<BitmapTexture> TextureMapperTiledBackingStore::texture() const
{
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        RefPtr<BitmapTexture> texture = m_tiles[i].texture();
        if (texture)
            return texture;
    }

    return PassRefPtr<BitmapTexture>();
}

void TextureMapperTiledBackingStore::setDebugBorder(const Color& color, float width)
{
    m_debugBorderColor = color;
    m_debugBorderWidth = width;
}

}
#endif
