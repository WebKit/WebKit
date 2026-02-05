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
#include "Test.h"
#include <WebCore/SkiaTextureAtlasPacker.h>

namespace TestWebKitAPI {

using namespace WebCore;

// Helper to verify packed rectangles don't overlap.
static bool rectanglesOverlap(const Vector<SkiaTextureAtlasPacker::PackedRect>& packed)
{
    for (size_t i = 0; i < packed.size(); ++i) {
        const auto& rectA = packed[i].rect;
        for (size_t j = i + 1; j < packed.size(); ++j) {
            const auto& rectB = packed[j].rect;
            if (rectA.intersects(rectB))
                return true;
        }
    }
    return false;
}

// Helper to verify all packed rectangles are within atlas bounds.
static bool allWithinBounds(const Vector<SkiaTextureAtlasPacker::PackedRect>& packed, const IntSize& atlasSize)
{
    IntRect atlasBounds(0, 0, atlasSize.width(), atlasSize.height());
    for (const auto& packedRect : packed) {
        if (!atlasBounds.contains(packedRect.rect))
            return false;
    }
    return true;
}

// Helper to verify all input rectangles are present in output.
static bool allInputsPresent(const Vector<SkiaTextureAtlasPacker::PackedRect>& packed, std::span<const IntSize> sizes)
{
    if (packed.size() != sizes.size())
        return false;

    Vector<bool> found(sizes.size(), false);
    for (const auto& packedRect : packed) {
        if (packedRect.imageIndex >= sizes.size())
            return false;
        if (found[packedRect.imageIndex])
            return false;
        if (packedRect.rect.size() != sizes[packedRect.imageIndex])
            return false;
        found[packedRect.imageIndex] = true;
    }
    return true;
}

// MaxRects algorithm tests

TEST(SkiaTextureAtlasPackerMaxRects, EmptyInput)
{
    Vector<IntSize> sizes;
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerMaxRects, SingleRectangle)
{
    Vector<IntSize> sizes { { 50, 50 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(1U, result.size());
    EXPECT_EQ(0U, result[0].imageIndex);
    EXPECT_EQ(50, result[0].rect.width());
    EXPECT_EQ(50, result[0].rect.height());
    EXPECT_EQ(0, result[0].rect.x());
    EXPECT_EQ(0, result[0].rect.y());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerMaxRects, ExactFit)
{
    Vector<IntSize> sizes { { 100, 100 } };
    IntSize atlasSize(100, 100);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerMaxRects, RectangleTooLarge)
{
    Vector<IntSize> sizes { { 300, 300 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerMaxRects, MultipleSimilarSized)
{
    Vector<IntSize> sizes { { 50, 50 }, { 50, 50 }, { 50, 50 }, { 50, 50 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(4U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerMaxRects, MultipleVariableSized)
{
    Vector<IntSize> sizes { { 100, 50 }, { 30, 80 }, { 60, 60 }, { 20, 20 }, { 45, 90 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(5U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerMaxRects, ManySmallRectangles)
{
    Vector<IntSize> sizes(20, { 20, 20 });
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(20U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerMaxRects, TotalAreaExceedsAtlas)
{
    Vector<IntSize> sizes(10, { 100, 100 });
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerMaxRects, WideRectangle)
{
    Vector<IntSize> sizes { { 200, 30 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerMaxRects, TallRectangle)
{
    Vector<IntSize> sizes { { 30, 200 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerMaxRects, MixedWideAndTall)
{
    Vector<IntSize> sizes { { 120, 30 }, { 30, 120 }, { 120, 30 }, { 30, 120 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(4U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerMaxRects, MinimumSizeRectangles)
{
    Vector<IntSize> sizes(10, { 1, 1 });
    IntSize atlasSize(32, 32);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(10U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

// ShelfNextFit algorithm tests

TEST(SkiaTextureAtlasPackerShelfNextFit, EmptyInput)
{
    Vector<IntSize> sizes;
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerShelfNextFit, SingleRectangle)
{
    Vector<IntSize> sizes { { 50, 50 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(1U, result.size());
    EXPECT_EQ(0U, result[0].imageIndex);
    EXPECT_EQ(50, result[0].rect.width());
    EXPECT_EQ(50, result[0].rect.height());
    EXPECT_EQ(0, result[0].rect.x());
    EXPECT_EQ(0, result[0].rect.y());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, ExactFit)
{
    Vector<IntSize> sizes { { 100, 100 } };
    IntSize atlasSize(100, 100);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, RectangleTooLarge)
{
    Vector<IntSize> sizes { { 300, 300 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerShelfNextFit, MultipleSimilarSized)
{
    Vector<IntSize> sizes { { 50, 50 }, { 50, 50 }, { 50, 50 }, { 50, 50 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(4U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, MultipleVariableSized)
{
    Vector<IntSize> sizes { { 100, 50 }, { 30, 80 }, { 60, 60 }, { 20, 20 }, { 45, 90 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(5U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, ManySmallRectangles)
{
    Vector<IntSize> sizes(20, { 20, 20 });
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(20U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, TotalAreaExceedsAtlas)
{
    Vector<IntSize> sizes(10, { 100, 100 });
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_TRUE(result.isEmpty());
}

TEST(SkiaTextureAtlasPackerShelfNextFit, WideRectangle)
{
    Vector<IntSize> sizes { { 200, 30 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, TallRectangle)
{
    Vector<IntSize> sizes { { 30, 200 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(1U, result.size());
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, MixedWideAndTall)
{
    Vector<IntSize> sizes { { 120, 30 }, { 30, 120 }, { 120, 30 }, { 30, 120 } };
    IntSize atlasSize(256, 256);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(4U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
    EXPECT_TRUE(allInputsPresent(result, sizes));
}

TEST(SkiaTextureAtlasPackerShelfNextFit, MinimumSizeRectangles)
{
    Vector<IntSize> sizes(10, { 1, 1 });
    IntSize atlasSize(32, 32);

    auto result = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_EQ(10U, result.size());
    EXPECT_FALSE(rectanglesOverlap(result));
    EXPECT_TRUE(allWithinBounds(result, atlasSize));
}

// General tests

// Verify that the default algorithm is MaxRects by constructing a case where
// ShelfNextFit fails but MaxRects succeeds. The atlas is 200x100 (area 20000)
// and the four rectangles tile it exactly:
//
//   +--------+------+
//   | 100x80 |100x60|  <- MaxRects fills the gap at (100,60) with 100x40
//   |        +------+
//   |        |100x40|
//   +--------+------+
//   |100x20  |         <- and places 100x20 at (0,80)
//   +--------+
//
// ShelfNextFit sorts by height and creates a shelf of height 80, wasting 20px
// next to the 100x60. The remaining 20px of atlas height can't fit 100x40.
TEST(SkiaTextureAtlasPacker, DefaultAlgorithmIsMaxRects)
{
    Vector<IntSize> sizes { { 100, 80 }, { 100, 60 }, { 100, 40 }, { 100, 20 } };
    IntSize atlasSize(200, 100);

    auto resultShelf = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::ShelfNextFit);
    EXPECT_TRUE(resultShelf.isEmpty());

    auto resultMaxRects = SkiaTextureAtlasPacker::pack(sizes, atlasSize, SkiaTextureAtlasPacker::Algorithm::MaxRects);
    EXPECT_EQ(4U, resultMaxRects.size());
    EXPECT_FALSE(rectanglesOverlap(resultMaxRects));
    EXPECT_TRUE(allWithinBounds(resultMaxRects, atlasSize));
    EXPECT_TRUE(allInputsPresent(resultMaxRects, sizes));

    // Default algorithm (no explicit algorithm) should also succeed, proving it uses MaxRects.
    auto resultDefault = SkiaTextureAtlasPacker::pack(sizes, atlasSize);
    EXPECT_EQ(4U, resultDefault.size());
    EXPECT_FALSE(rectanglesOverlap(resultDefault));
    EXPECT_TRUE(allWithinBounds(resultDefault, atlasSize));
    EXPECT_TRUE(allInputsPresent(resultDefault, sizes));
}

} // namespace TestWebKitAPI
