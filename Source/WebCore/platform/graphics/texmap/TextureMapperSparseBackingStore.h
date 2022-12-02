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

#pragma once

#if USE(GRAPHICS_LAYER_WC)

#include "IntPointHash.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperTile.h"
#include <wtf/HashMap.h>

namespace WebCore {

class TextureMapperSparseBackingStore final : public TextureMapperBackingStore {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using TileIndex = WebCore::IntPoint;

    WEBCORE_EXPORT void setSize(const IntSize&);
    WEBCORE_EXPORT void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
    WEBCORE_EXPORT void drawBorder(TextureMapper&, const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    WEBCORE_EXPORT void drawRepaintCounter(TextureMapper&, int repaintCount, const Color&, const FloatRect&, const TransformationMatrix&) override;
    WEBCORE_EXPORT void updateContents(TextureMapper&, const TileIndex&, Image&, const IntRect& dirtyRect);
    WEBCORE_EXPORT void removeTile(const TileIndex&);

private:
    TransformationMatrix adjustedTransformForRect(const FloatRect&);

    IntSize m_size;
    HashMap<TileIndex, std::unique_ptr<TextureMapperTile>> m_tiles;
};

} // namespace WebCore

#endif // USE(GRAPHICS_LAYER_WC)
