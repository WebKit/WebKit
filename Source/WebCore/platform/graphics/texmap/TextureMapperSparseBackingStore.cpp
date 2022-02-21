/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperSparseBackingStore.h"

#if USE(GRAPHICS_LAYER_WC)

namespace WebCore {

TextureMapperSparseBackingStore::TextureMapperSparseBackingStore(int tileSize)
    : m_tileSize(tileSize)
{
}

void TextureMapperSparseBackingStore::setSize(const IntSize& size)
{
    if (m_size == size)
        return;
    m_tiles.clear();
    m_size = size;
    auto tiles = tileDimension();
    for (int y = 0; y < tiles.height(); y++) {
        for (int x = 0; x < tiles.width(); x++) {
            IntRect rect = { x * m_tileSize, y * m_tileSize, m_tileSize, m_tileSize };
            rect.intersect({ { }, m_size });
            m_tiles.append(TextureMapperTile(rect));
        }
    }
}

void TextureMapperSparseBackingStore::removeUncoveredTiles(const IntRect& coverage)
{
    auto minCoveredX = coverage.x() / m_tileSize;
    auto maxCoveredX = (coverage.maxX() + m_tileSize - 1) / m_tileSize;
    auto minCoveredY = coverage.y() / m_tileSize;
    auto maxCoveredY = (coverage.maxY() + m_tileSize - 1) / m_tileSize;
    auto tiles = tileDimension();
    for (int y = 0; y < tiles.height(); y++) {
        bool coveredY = minCoveredY <= y && y < maxCoveredY;
        for (int x = 0; x < tiles.width(); x++) {
            bool covered = coveredY && minCoveredX <= x && x < maxCoveredX;
            if (!covered)
                m_tiles[x + y * tiles.width()].setTexture(nullptr);
        }
    }
}

TransformationMatrix TextureMapperSparseBackingStore::adjustedTransformForRect(const FloatRect& targetRect)
{
    return TransformationMatrix::rectToRect({ { }, m_size }, targetRect);
}

void TextureMapperSparseBackingStore::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity)
{
    IntRect rect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        tile.paint(textureMapper, adjustedTransform, opacity, calculateExposedTileEdges(rect, tile.rect()));
}

void TextureMapperSparseBackingStore::drawBorder(TextureMapper& textureMapper, const Color& borderColor, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        textureMapper.drawBorder(borderColor, borderWidth, tile.rect(), adjustedTransform);
}

void TextureMapperSparseBackingStore::drawRepaintCounter(TextureMapper& textureMapper, int repaintCount, const Color& borderColor, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        textureMapper.drawNumber(repaintCount, borderColor, tile.rect().location(), adjustedTransform);
}

IntSize TextureMapperSparseBackingStore::tileDimension() const
{
    return { (m_size.width() + m_tileSize - 1) / m_tileSize,  (m_size.height() + m_tileSize - 1) / m_tileSize };
}

void TextureMapperSparseBackingStore::updateContents(TextureMapper& textureMapper, Image& image, const IntRect& dirtyRect)
{
    for (auto& tile : m_tiles)
        tile.updateContents(textureMapper, &image, dirtyRect);
}

} // namespace WebCore

#endif // USE(GRAPHICS_LAYER_WC)
