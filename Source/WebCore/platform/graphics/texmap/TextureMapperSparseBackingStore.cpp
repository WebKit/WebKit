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

void TextureMapperSparseBackingStore::setSize(const IntSize& size)
{
    if (m_size == size)
        return;
    m_size = size;
    m_tiles.clear();
}

TransformationMatrix TextureMapperSparseBackingStore::adjustedTransformForRect(const FloatRect& targetRect)
{
    return TransformationMatrix::rectToRect({ { }, m_size }, targetRect);
}

void TextureMapperSparseBackingStore::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity)
{
    IntRect rect = { { }, m_size };
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& iterator : m_tiles)
        iterator.value->paint(textureMapper, adjustedTransform, opacity, calculateExposedTileEdges(rect, iterator.value->rect()));
}

void TextureMapperSparseBackingStore::drawBorder(TextureMapper& textureMapper, const Color& borderColor, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& iterator : m_tiles)
        textureMapper.drawBorder(borderColor, borderWidth, iterator.value->rect(), adjustedTransform);
}

void TextureMapperSparseBackingStore::drawRepaintCounter(TextureMapper& textureMapper, int repaintCount, const Color& borderColor, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& iterator : m_tiles)
        textureMapper.drawNumber(repaintCount, borderColor, iterator.value->rect().location(), adjustedTransform);
}

void TextureMapperSparseBackingStore::updateContents(TextureMapper& textureMapper, const TileIndex& index, Image& image, const IntRect& dirtyRect)
{
    auto addResult = m_tiles.ensure(index, [&]() {
        return makeUnique<TextureMapperTile>(dirtyRect);
    });
    addResult.iterator->value->updateContents(textureMapper, &image, dirtyRect);
}

void TextureMapperSparseBackingStore::removeTile(const TileIndex& index)
{
    m_tiles.remove(index);
}

} // namespace WebCore

#endif // USE(GRAPHICS_LAYER_WC)
