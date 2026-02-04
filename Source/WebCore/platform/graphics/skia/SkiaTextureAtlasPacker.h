/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(SKIA)

#include "IntRect.h"
#include "IntSize.h"
#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// 2D bin packing algorithms for texture atlas layout computation.
//
// References:
// - Jukka Jylänki, "A Thousand Ways to Pack the Bin - A Practical Approach to Two-Dimensional Rectangle Bin Packing", 2010.
//   https://github.com/juj/RectangleBinPack
//
// Algorithms:
// - SHELF-NF (Shelf Next Fit): O(n log n) time, O(1) space.
//   Simple and fast, best for similar-sized rectangles. ~70-80% occupancy.
//
// - MAXRECTS-BSSF (MaxRects Best Short Side Fit): O(n^2) time, O(n) space.
//   Tracks all maximal free rectangles. Best for variable-sized rectangles.
//   ~94% occupancy on benchmarks.
//
namespace SkiaTextureAtlasPacker {

struct PackedRect {
    IntRect rect;
    size_t imageIndex { 0 };
};

enum class Algorithm {
    ShelfNextFit,
    MaxRects
};

// Pack rectangles into a fixed-size atlas.
// Returns packed rectangles with positions, or empty vector if images don't fit.
WEBCORE_EXPORT Vector<PackedRect> pack(std::span<const IntSize> sizes, const IntSize& atlasSize, Algorithm = Algorithm::MaxRects);

}; // namespace SkiaTextureAtlasPacker

} // namespace WebCore

#endif // USE(SKIA)
