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
#include "SkiaImageAtlasLayoutBuilder.h"

#if USE(SKIA)

#include <numeric>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaImageAtlasLayoutBuilder);

SkiaImageAtlasLayoutBuilder::SkiaImageAtlasLayoutBuilder() = default;
SkiaImageAtlasLayoutBuilder::~SkiaImageAtlasLayoutBuilder() = default;

void SkiaImageAtlasLayoutBuilder::collectRasterImage(const sk_sp<SkImage>& image)
{
    if (!image)
        return;

    ASSERT(!image->isTextureBacked());

    // Check size constraints.
    int width = image->width();
    if (width < minImageSize || width > maxImageSize)
        return;

    int height = image->height();
    if (height < minImageSize || height > maxImageSize)
        return;

    // Don't collect duplicates.
    if (!m_collectedSet.add(image.get()).isNewEntry)
        return;

    add(m_hasher, image.get());
    m_collectedImages.append({ image, IntSize(width, height) });
}

// Compute tight bounding box area for packed rectangles.
static size_t computeBoundingBoxArea(const Vector<SkiaTextureAtlasPacker::PackedRect>& packedRects)
{
    int maxX = 0;
    int maxY = 0;
    for (const auto& packed : packedRects) {
        maxX = std::max(maxX, packed.rect.maxX());
        maxY = std::max(maxY, packed.rect.maxY());
    }
    return static_cast<size_t>(maxX) * maxY;
}

// Try both packing algorithms at a given atlas size and return the result with smaller bounding box.
static Vector<SkiaTextureAtlasPacker::PackedRect> packWithBestAlgorithm(const Vector<IntSize>& sizes, const IntSize& atlasSize)
{
    auto maxRectsResult = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    auto shelfResult = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);

    if (!maxRectsResult.isEmpty() && !shelfResult.isEmpty()) {
        if (computeBoundingBoxArea(shelfResult) < computeBoundingBoxArea(maxRectsResult))
            return shelfResult;
        return maxRectsResult;
    }

    if (!maxRectsResult.isEmpty())
        return maxRectsResult;

    return shelfResult;
}

Vector<Ref<SkiaImageAtlasLayout>> SkiaImageAtlasLayoutBuilder::finalize()
{
    ASSERT(!m_finalized);
    m_finalized = true;

    // Not enough images for atlasing.
    if (m_collectedImages.size() < minImagesForAtlas)
        return { };

    // Extract sizes for packing.
    Vector<IntSize> sizes(m_collectedImages.size(), [&](size_t index) {
        return m_collectedImages[index].size;
    });

    // Calculate optimal atlas size based on total image area.
    // This prevents wasting GPU memory and upload bandwidth on sparse atlases.
    auto optimalSide = calculateOptimalAtlasSize(sizes);
    IntSize atlasSize(optimalSide, optimalSide);

    // Try both packing algorithms and pick the one with smaller bounding box.
    // MaxRects is better for variable sizes, ShelfNextFit for similar sizes.
    auto bestResult = packWithBestAlgorithm(sizes, atlasSize);
    if (!bestResult.isEmpty())
        return { createAtlasLayout(bestResult) };

    // Fallback: try max atlas size if optimal size failed.
    bestResult = packWithBestAlgorithm(sizes, IntSize(maxAtlasSize, maxAtlasSize));
    if (!bestResult.isEmpty())
        return { createAtlasLayout(bestResult) };

    // Single atlas failed - use multi-atlas fallback.
    return packMultipleAtlases(sizes);
}

int SkiaImageAtlasLayoutBuilder::calculateOptimalAtlasSize(const Vector<IntSize>& sizes)
{
    // Calculate total pixel area of all images.
    uint64_t totalArea = 0;
    int maxWidth = 0;
    int maxHeight = 0;

    for (const auto& size : sizes) {
        totalArea += size.unclampedArea();
        maxWidth = std::max(maxWidth, size.width());
        maxHeight = std::max(maxHeight, size.height());
    }

    // Add overhead for packing inefficiency (typically 15-30% waste).
    // Use 1.3x to be safe with various image size distributions.
    constexpr float packingOverhead = 1.3f;
    uint64_t targetArea = static_cast<uint64_t>(totalArea * packingOverhead);

    // Compute square atlas side length.
    int side = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(targetArea))));

    // Ensure atlas can fit the largest image.
    side = std::max(side, maxWidth);
    side = std::max(side, maxHeight);

    // Clamp to valid range.
    side = std::clamp(side, minAtlasSize, maxAtlasSize);
    return side;
}

Vector<Ref<SkiaImageAtlasLayout>> SkiaImageAtlasLayoutBuilder::packMultipleAtlases(const Vector<IntSize>& allSizes)
{
    Vector<Ref<SkiaImageAtlasLayout>> result;
    Vector<bool> packed(FillWith { }, allSizes.size(), false);
    size_t totalPacked = 0;

    // Sort indices by area (largest first) for better packing.
    Vector<size_t> sortedIndices(allSizes.size(), [](size_t index) {
        return index;
    });

    std::ranges::sort(sortedIndices, [&allSizes](size_t a, size_t b) {
        return allSizes[a].unclampedArea() > allSizes[b].unclampedArea();
    });

    IntSize atlasSize(maxAtlasSize, maxAtlasSize);
    while (totalPacked < allSizes.size() && result.size() < maxAtlasCount) {
        // Collect unpacked images for this atlas.
        Vector<IntSize> batchSizes;
        Vector<size_t> batchOriginalIndices;

        for (size_t index : sortedIndices) {
            if (packed[index])
                continue;
            batchSizes.append(allSizes[index]);
            batchOriginalIndices.append(index);
        }

        if (batchSizes.isEmpty())
            break;

        // Try to pack this batch.
        auto packedRects = SkiaTextureAtlasPacker::pack(batchSizes, atlasSize);
        if (packedRects.isEmpty()) {
            // Even a single image doesn't fit - try reducing batch size.
            // Binary search for max batch that fits.
            size_t maxBatch = findMaxPackableBatch(batchSizes, atlasSize);
            if (!maxBatch)
                break; // Can't pack any more images.

            batchSizes.shrink(maxBatch);
            batchOriginalIndices.shrink(maxBatch);
            packedRects = SkiaTextureAtlasPacker::pack(batchSizes, atlasSize);
            if (packedRects.isEmpty())
                break;
        }

        // Mark packed images.
        for (const auto& rect : packedRects) {
            size_t originalIndex = batchOriginalIndices[rect.imageIndex];
            packed[originalIndex] = true;
            ++totalPacked;
        }

        // Create atlas layout with remapped indices.
        Vector<SkiaTextureAtlasPacker::PackedRect> remappedRects(packedRects.size(), [&](size_t index) -> SkiaTextureAtlasPacker::PackedRect {
            return { packedRects[index].rect, batchOriginalIndices[packedRects[index].imageIndex] };
        });
        result.append(createAtlasLayout(remappedRects));
    }

    // Only return atlases if we packed enough images.
    if (totalPacked < minImagesForAtlas)
        return { };

    return result;
}

size_t SkiaImageAtlasLayoutBuilder::findMaxPackableBatch(const Vector<IntSize>& sizes, const IntSize& atlasSize)
{
    // Early return for empty input.
    if (sizes.isEmpty())
        return 0;

    // Binary search for the maximum batch size that can be packed.
    size_t lo = 1;
    size_t hi = sizes.size();
    size_t maxPackable = 0;

    while (lo <= hi) {
        size_t mid = std::midpoint(lo, hi);

        Vector<IntSize> testBatch(mid, [&sizes](size_t index) {
            return sizes[index];
        });

        auto packed = SkiaTextureAtlasPacker::pack(testBatch, atlasSize);
        if (!packed.isEmpty()) {
            maxPackable = mid;
            lo = mid + 1;
        } else {
            // Can't pack this many - try fewer.
            // Since lo starts at 1, mid is always >= 1 here.
            hi = mid - 1;
        }
    }

    return maxPackable;
}

Ref<SkiaImageAtlasLayout> SkiaImageAtlasLayoutBuilder::createAtlasLayout(const Vector<SkiaTextureAtlasPacker::PackedRect>& packedRects)
{
    // Build atlas entries from packed results.
    Vector<SkiaImageAtlasLayout::Entry> entries(packedRects.size(), [&](size_t index) -> SkiaImageAtlasLayout::Entry {
        return { m_collectedImages[packedRects[index].imageIndex].image, packedRects[index].rect };
    });

    // Compute actual atlas size needed (tight bounds).
    int actualWidth = 0;
    int actualHeight = 0;
    for (const auto& packed : packedRects) {
        actualWidth = std::max(actualWidth, packed.rect.maxX());
        actualHeight = std::max(actualHeight, packed.rect.maxY());
    }

    return SkiaImageAtlasLayout::create(IntSize(actualWidth, actualHeight), WTF::move(entries));
}

} // namespace WebCore

#endif // USE(SKIA)
