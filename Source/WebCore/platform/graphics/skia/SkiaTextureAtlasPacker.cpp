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

#include "config.h"
#include "SkiaTextureAtlasPacker.h"

#if USE(SKIA)

#include <algorithm>
#include <limits>
#include <wtf/StdLibExtras.h>

namespace WebCore {

namespace SkiaTextureAtlasPacker {

static Vector<PackedRect> shelfNextFitAlgorithm(std::span<const IntSize> sizes, const IntSize& atlasSize)
{
    if (sizes.empty())
        return { };

    if (auto it = std::ranges::find_if(sizes, &IntSize::isEmpty); it != sizes.end())
        return { };

    // Create indices sorted by height (descending) for better shelf packing.
    Vector<size_t, 32> sortedIndices(sizes.size(), [](size_t index) {
        return index;
    });

    std::ranges::sort(sortedIndices, [&sizes](size_t a, size_t b) {
        return sizes[a].unclampedArea() > sizes[b].unclampedArea();
    });

    Vector<PackedRect> result;
    result.reserveInitialCapacity(sizes.size());

    int shelfY = 0;
    int shelfX = 0;
    int shelfHeight = 0;

    for (size_t index : sortedIndices) {
        const auto& size = sizes[index];

        // Check if image fits on current shelf.
        if (shelfX + size.width() > atlasSize.width()) {
            // Start new shelf.
            shelfY += shelfHeight;
            shelfX = 0;
            shelfHeight = 0;
        }

        // Check if image fits vertically.
        if (shelfY + size.height() > atlasSize.height())
            return { };

        result.append({ IntRect(shelfX, shelfY, size.width(), size.height()), index });
        shelfX += size.width();
        shelfHeight = std::max(shelfHeight, size.height());
    }

    return result;
}

// Prune free rectangles: remove any rectangle fully contained within another.
// Two-pass approach: first mark contained rectangles, then remove them.
// This is O(n^2) but keeps the free list small for better performance overall.
static void pruneFreeRectangles(Vector<IntRect, 16>& freeRectangles)
{
    Vector<bool, 16> shouldRemove(freeRectangles.size(), false);

    for (size_t i = 0; i < freeRectangles.size(); ++i) {
        if (shouldRemove[i])
            continue;
        for (size_t j = i + 1; j < freeRectangles.size(); ++j) {
            if (shouldRemove[j])
                continue;
            if (freeRectangles[i].contains(freeRectangles[j]))
                shouldRemove[j] = true;
            else if (freeRectangles[j].contains(freeRectangles[i])) {
                shouldRemove[i] = true;
                break;
            }
        }
    }

    for (size_t i = freeRectangles.size(); i-- > 0;) {
        if (shouldRemove[i])
            freeRectangles.removeAt(i);
    }
}

// MAXRECTS-BSSF (Best Short Side Fit) implementation.
//
// The algorithm maintains a list of free rectangles representing available space.
// When placing a rectangle:
// 1. Find the free rectangle where the shorter leftover side is minimized (BSSF)
// 2. Split affected free rectangles around the placed rectangle
// 3. Prune free rectangles that are fully contained within others
//
// Reference: Jukka Jylänki, "A Thousand Ways to Pack the Bin", Section 2.1.1
static Vector<PackedRect> maxRectsAlgorithm(std::span<const IntSize> sizes, const IntSize& atlasSize)
{
    if (sizes.empty())
        return { };

    if (auto it = std::ranges::find_if(sizes, &IntSize::isEmpty); it != sizes.end())
        return { };

    // Free rectangles representing available space - starting with the entire atlas.
    Vector<IntRect, 16> freeRectangles;
    freeRectangles.append({ IntPoint::zero(), atlasSize });

    // Sort input by area, descending - larger rectangles first improves packing.
    Vector<size_t, 32> sortedIndices(sizes.size(), [](size_t index) {
        return index;
    });

    std::ranges::sort(sortedIndices, [&sizes](size_t a, size_t b) {
        return sizes[a].unclampedArea() > sizes[b].unclampedArea();
    });

    constexpr int maxInteger = std::numeric_limits<int>::max();
    constexpr size_t maxSize = std::numeric_limits<size_t>::max();

    Vector<PackedRect> result;
    result.reserveInitialCapacity(sizes.size());

    for (size_t index : sortedIndices) {
        const auto& size = sizes[index];

        // Find best free rectangle using BSSF (Best Short Side Fit) heuristic.
        // This minimizes the shorter leftover side after placement.
        int bestShortSideFit = maxInteger;
        int bestLongSideFit = maxInteger;
        size_t bestIndex = maxSize;
        IntPoint bestPosition;

        for (size_t i = 0; i < freeRectangles.size(); ++i) {
            auto& freeRectangle = freeRectangles[i];

            // Check if rectangle fits in this free rectangle.
            if (size.width() <= freeRectangle.width() && size.height() <= freeRectangle.height()) {
                int leftoverHorizontal = freeRectangle.width() - size.width();
                int leftoverVertical = freeRectangle.height() - size.height();
                int shortSideFit = std::min(leftoverHorizontal, leftoverVertical);
                int longSideFit = std::max(leftoverHorizontal, leftoverVertical);

                if (shortSideFit < bestShortSideFit || (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit)) {
                    bestShortSideFit = shortSideFit;
                    bestLongSideFit = longSideFit;
                    bestIndex = i;
                    bestPosition = freeRectangle.location();

                    // Perfect fit found, stop searching.
                    if (!shortSideFit && !longSideFit)
                        break;
                }
            }
        }

        // No suitable free rectangle found - packing failed.
        if (bestIndex == maxSize)
            return { };

        // Place the rectangle.
        IntRect placedRectangle(bestPosition, size);
        result.append({ placedRectangle, index });

        // Split all free rectangles that intersect with the placed rectangle.
        // This generates new free rectangles from the non-overlapping parts.
        // We iterate only over original rectangles and collect results in a new vector.
        Vector<IntRect, 16> newFreeRectangles;
        for (const auto& freeRectangle : freeRectangles) {
            if (!freeRectangle.intersects(placedRectangle)) {
                newFreeRectangles.append(freeRectangle);
                continue;
            }

            // Generate up to 4 new free rectangles from the non-overlapping parts.

            // Left part
            if (placedRectangle.x() > freeRectangle.x())
                newFreeRectangles.append({ freeRectangle.x(), freeRectangle.y(), placedRectangle.x() - freeRectangle.x(), freeRectangle.height() });

            // Right part
            if (placedRectangle.maxX() < freeRectangle.maxX())
                newFreeRectangles.append({ placedRectangle.maxX(), freeRectangle.y(), freeRectangle.maxX() - placedRectangle.maxX(), freeRectangle.height() });

            // Top part
            if (placedRectangle.y() > freeRectangle.y())
                newFreeRectangles.append({ freeRectangle.x(), freeRectangle.y(), freeRectangle.width(), placedRectangle.y() - freeRectangle.y() });

            // Bottom part
            if (placedRectangle.maxY() < freeRectangle.maxY())
                newFreeRectangles.append({ freeRectangle.x(), placedRectangle.maxY(), freeRectangle.width(), freeRectangle.maxY() - placedRectangle.maxY() });
        }
        freeRectangles = WTF::move(newFreeRectangles);

        pruneFreeRectangles(freeRectangles);
    }

    return result;
}

Vector<PackedRect> pack(std::span<const IntSize> sizes, const IntSize& atlasSize, Algorithm algorithm)
{
    if (atlasSize.isEmpty())
        return { };

    switch (algorithm) {
    case Algorithm::ShelfNextFit:
        return shelfNextFitAlgorithm(sizes, atlasSize);
    case Algorithm::MaxRects:
        return maxRectsAlgorithm(sizes, atlasSize);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace SkiaTextureAtlasPacker

} // namespace WebCore

#endif // USE(SKIA)
