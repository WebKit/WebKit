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

#include "config.h"
#include "VivanteSuperTiledTexture.h"

#if USE(GBM)

namespace WebCore {

// Interleave x and y coordinates to produce Morton code (z-curve index).
static constexpr unsigned interleave(uint32_t x, uint32_t y)
{
    uint64_t z = 0;
    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t xMasked = x & (1 << i);
        uint32_t yMasked = y & (1 << i);

        z |= static_cast<uint64_t>(xMasked) << i;
        z |= static_cast<uint64_t>(yMasked) << (i + 1);
    }

    return static_cast<unsigned>(z);
}

const VivanteSuperTiledTexture::ZCurveLUT<VivanteSuperTiledTexture::TilesInSuperTileWidth>& VivanteSuperTiledTexture::zCurveTileLUT()
{
    static const auto lut = []() {
        ZCurveLUT<TilesInSuperTileWidth> result;
        for (unsigned y = 0; y < TilesInSuperTileWidth; ++y) {
            for (unsigned x = 0; x < TilesInSuperTileWidth; ++x)
                result[y][x] = interleave(x, y);
        }
        return result;
    }();

    return lut;
}

const VivanteSuperTiledTexture::ZCurveLUT<VivanteSuperTiledTexture::SuperTileWidth>& VivanteSuperTiledTexture::zCurveTexelLUT()
{
    static const auto lut = []() {
        ZCurveLUT<SuperTileWidth> result;
        const auto& tileLUT = zCurveTileLUT();
        for (unsigned y = 0; y < SuperTileWidth; ++y) {
            unsigned yTileInSuperTile = y / TileWidth;
            unsigned yTexelInTile = y % TileWidth;
            for (unsigned x = 0; x < SuperTileWidth; ++x) {
                unsigned xTileInSuperTile = x / TileWidth;
                unsigned xTexelInTile = x % TileWidth;

                unsigned tileIndex = tileLUT[yTileInSuperTile][xTileInSuperTile];
                result[y][x] = tileIndex * TileTexels + yTexelInTile * TileWidth + xTexelInTile;
            }
        }
        return result;
    }();

    return lut;
}

} // namespace WebCore

#endif // USE(GBM)
