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

#include "VivanteSuperTiledTexture.h"
#include <cstring>
#include <wtf/StdLibExtras.h>

// Cross-compiler loop unroll pragma.
#if COMPILER(CLANG)
#define VIVANTE_UNROLL(N) _Pragma(STRINGIZE(unroll N))
#elif COMPILER(GCC)
#define VIVANTE_UNROLL(N) _Pragma(STRINGIZE(GCC unroll N))
#else
#define VIVANTE_UNROLL(N)
#endif

namespace WebCore {

ALWAYS_INLINE VivanteSuperTiledTexture::VivanteSuperTiledTexture(std::span<uint32_t> texels, unsigned stride)
    : m_texels(texels)
    , m_superTilesInStride(stride / sizeof(uint32_t) / SuperTileWidth)
{
    // Stride must be super-tile aligned.
    ASSERT(!(stride % (SuperTileWidth * sizeof(uint32_t))));
}

// This implementation iterates over super-tiles first, then tiles within each super-tile,
// avoiding repeated x-axis super-tile resolution for each tile.
ALWAYS_INLINE void VivanteSuperTiledTexture::writeLine(unsigned x, unsigned y, unsigned width, std::span<const uint32_t> colors)
{
    unsigned ySuperTile = y / SuperTileWidth;
    unsigned xSuperTile = x / SuperTileWidth;

    unsigned currentSuperTileIndex = ySuperTile * m_superTilesInStride + xSuperTile;
    auto currentSuperTile = m_texels.subspan(currentSuperTileIndex * SuperTileTexels);

    unsigned xTexelInSuperTile = x % SuperTileWidth;
    unsigned yTexelInSuperTile = y % SuperTileWidth;

    const auto& zCurveTexelLUTY = zCurveTexelLUT()[yTexelInSuperTile];

    unsigned texelsConsumed = 0;

    auto writeTexels = [&zCurveTexelLUTY, &currentSuperTile, &colors](unsigned xTexelInSuperTile, unsigned sourceOffset, unsigned writeWidth) ALWAYS_INLINE_LAMBDA {
        unsigned texelIndexInSuperTile = zCurveTexelLUTY[xTexelInSuperTile];
        memcpySpan(currentSuperTile.subspan(texelIndexInSuperTile, writeWidth), colors.subspan(sourceOffset, writeWidth));
    };

    if (xTexelInSuperTile > 0) {
        unsigned widthInSuperTile = SuperTileWidth - xTexelInSuperTile;
        texelsConsumed = widthInSuperTile % TileWidth;
        if (texelsConsumed > 0) {
            if (width > texelsConsumed)
                writeTexels(xTexelInSuperTile, 0, texelsConsumed);
            else {
                writeTexels(xTexelInSuperTile, 0, width);
                return;
            }
        }

        auto writeFirstSuperTileFullTiles = [xTexelInSuperTile, &texelsConsumed, &writeTexels](unsigned widthToWrite) ALWAYS_INLINE_LAMBDA {
            unsigned fullTilesInSuperTileWidth = widthToWrite / TileWidth * TileWidth;
            do {
                writeTexels(xTexelInSuperTile + texelsConsumed, texelsConsumed, TileWidth);
                texelsConsumed += TileWidth;
            } while (texelsConsumed < fullTilesInSuperTileWidth);
        };

        if (width > widthInSuperTile) {
            if (widthInSuperTile >= TileWidth)
                writeFirstSuperTileFullTiles(widthInSuperTile);

            ++currentSuperTileIndex;
            currentSuperTile = m_texels.subspan(currentSuperTileIndex * SuperTileTexels);
        } else {
            widthInSuperTile = width - texelsConsumed;
            if (widthInSuperTile > TileWidth) {
                writeFirstSuperTileFullTiles(widthInSuperTile);

                if (width > texelsConsumed)
                    writeTexels(xTexelInSuperTile + texelsConsumed, texelsConsumed, width - texelsConsumed);
            } else
                writeTexels(xTexelInSuperTile + texelsConsumed, texelsConsumed, widthInSuperTile);

            return;
        }
    }

    auto writeLastSuperTile = [&texelsConsumed, &writeTexels](unsigned widthToWrite) ALWAYS_INLINE_LAMBDA {
        if (widthToWrite > TileWidth) {
            unsigned fullTilesInSuperTileWidth = widthToWrite / TileWidth * TileWidth;
            unsigned i = 0;
            do {
                writeTexels(i, texelsConsumed, TileWidth);
                i += TileWidth;
                texelsConsumed += TileWidth;
            } while (i < fullTilesInSuperTileWidth);

            if (widthToWrite > fullTilesInSuperTileWidth)
                writeTexels(i, texelsConsumed, widthToWrite - fullTilesInSuperTileWidth);
        } else
            writeTexels(0, texelsConsumed, widthToWrite);
    };

    unsigned widthLeft = width - texelsConsumed;
    if (widthLeft >= SuperTileWidth) {
        unsigned fullSuperTilesWidth = widthLeft / SuperTileWidth * SuperTileWidth;
        do {
            VIVANTE_UNROLL(16)
            for (unsigned i = 0; i < SuperTileWidth; i += TileWidth, texelsConsumed += TileWidth)
                writeTexels(i, texelsConsumed, TileWidth);

            ++currentSuperTileIndex;
            currentSuperTile = m_texels.subspan(currentSuperTileIndex * SuperTileTexels);
        } while (texelsConsumed < fullSuperTilesWidth);

        if (widthLeft > fullSuperTilesWidth) {
            widthLeft -= fullSuperTilesWidth;
            writeLastSuperTile(widthLeft);
        }
    } else
        writeLastSuperTile(widthLeft);
}

#undef VIVANTE_UNROLL

} // namespace WebCore

#endif // USE(GBM)
