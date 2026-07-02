/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(GBM)

#include "IntSize.h"
#include <array>
#include <cstdint>

#include <wtf/Noncopyable.h>

namespace WebCore {

// Abstraction layer to access Vivante super-tiled texture buffers.
// Super-tiles are 64x64 texel blocks, subdivided into 4x4 tiles arranged in z-curve (Morton code) order.
class VivanteSuperTiledTexture {
    WTF_MAKE_NONCOPYABLE(VivanteSuperTiledTexture);
public:
    VivanteSuperTiledTexture(std::span<uint32_t> texels, unsigned stride);

    void writeLine(unsigned x, unsigned y, unsigned width, std::span<const uint32_t> colors);

    // Align an IntSize to super-tile boundaries (multiple of 64 in both dimensions).
    static IntSize alignToSuperTileIntSize(const IntSize& size)
    {
        return IntSize(alignToSuperTile(size.width()), alignToSuperTile(size.height()));
    }

private:
    static constexpr unsigned SuperTileWidth = 64;
    static constexpr unsigned TileWidth = 4;
    static constexpr unsigned TilesInSuperTileWidth = SuperTileWidth / TileWidth;
    static constexpr unsigned SuperTileTexels = SuperTileWidth * SuperTileWidth;
    static constexpr unsigned TileTexels = TileWidth * TileWidth;

    template<unsigned size>
    using ZCurveLUT = std::array<std::array<unsigned, size>, size>;

    static const ZCurveLUT<TilesInSuperTileWidth>& zCurveTileLUT();
    static const ZCurveLUT<SuperTileWidth>& zCurveTexelLUT();

    // Align a dimension to super-tile boundary (multiple of 64).
    static constexpr unsigned alignToSuperTile(unsigned value)
    {
        return (value + SuperTileWidth - 1) & ~(SuperTileWidth - 1);
    }

    std::span<uint32_t> m_texels;
    unsigned m_superTilesInStride { 0 };
};

} // namespace WebCore

#endif // USE(GBM)
