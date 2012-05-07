/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "IntSize.h"
#include "TilingData.h"
#include <wtf/Assertions.h>

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestTiling : public TilingData {
public:
    TestTiling(IntSize maxTextureSize, IntSize totalSize, bool hasBorderTexels)
        : TilingData(maxTextureSize, totalSize, hasBorderTexels)
    {
    }

    // This function only available for testing because of potential overflow.
    int numTiles() const
    {
        int numTiles = numTilesX() * numTilesY();

        // Assert no overflow.
        EXPECT_GE(numTiles, 0);
        if (numTiles > 0)
            EXPECT_EQ(numTiles / numTilesX(), numTilesY());

        return numTiles;
    }
};

TEST(TilingDataTest, numTiles_NoTiling)
{
    EXPECT_EQ(1, TestTiling(IntSize(16, 16), IntSize(16, 16), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(16, 16), IntSize(15, 15), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(16, 16), IntSize(16, 16), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(16, 16), IntSize(1, 16), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(15, 15), IntSize(15, 15), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(32, 16), IntSize(32, 16), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(32, 16), IntSize(32, 16), true).numTiles());
}

TEST(TilingDataTest, numTiles_TilingNoBorders)
{
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(0,  0), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(4,  0), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(0,  4), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(4, 4), IntSize(4,  0), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(4, 4), IntSize(0,  4), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(-8, -8), IntSize(1,  1), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(-1, -1), IntSize(1,  1), false).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(1,  1), false).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(1, 1), IntSize(1,  1), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(1, 1), IntSize(1,  2), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(1, 1), IntSize(2,  1), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(1,  1), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(1,  2), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(2,  1), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(2,  2), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(3,  3), false).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(1,  4), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(2,  4), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(3,  4), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(4,  4), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(5,  4), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(6,  4), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(7,  4), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(8,  4), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(4, 4), IntSize(9,  4), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(4, 4), IntSize(10,  4), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(4, 4), IntSize(11,  4), false).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(1,  5), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(2,  5), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(3,  5), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(4,  5), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(5,  5), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(6,  5), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(7,  5), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(8,  5), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(9,  5), false).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(10,  5), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(11,  5), false).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(16, 16), IntSize(16, 16), false).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(17, 17), IntSize(16, 16), false).numTiles());
    EXPECT_EQ(4, TestTiling(IntSize(15, 15), IntSize(16, 16), false).numTiles());
    EXPECT_EQ(4, TestTiling(IntSize(8, 8), IntSize(16, 16), false).numTiles());
    EXPECT_EQ(6, TestTiling(IntSize(8, 8), IntSize(17, 16), false).numTiles());

    EXPECT_EQ(8, TestTiling(IntSize(5, 8), IntSize(17, 16), false).numTiles());
}

TEST(TilingDataTest, numTiles_TilingWithBorders)
{
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(0,  0), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(4,  0), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(0,  4), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(4, 4), IntSize(4,  0), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(4, 4), IntSize(0,  4), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(-8, -8), IntSize(1,  1), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(-1, -1), IntSize(1,  1), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(0, 0), IntSize(1,  1), true).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(1, 1), IntSize(1,  1), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(1,  2), true).numTiles());
    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(2,  1), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(1,  1), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(1,  2), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(2,  1), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(2, 2), IntSize(2,  2), true).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1,  3), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(2,  3), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(3,  3), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(4,  3), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(5,  3), true).numTiles());
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(6,  3), true).numTiles());
    EXPECT_EQ(5, TestTiling(IntSize(3, 3), IntSize(7,  3), true).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(1,  4), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(2,  4), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(3,  4), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(4, 4), IntSize(4,  4), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(5,  4), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(4, 4), IntSize(6,  4), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(4, 4), IntSize(7,  4), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(4, 4), IntSize(8,  4), true).numTiles());
    EXPECT_EQ(4, TestTiling(IntSize(4, 4), IntSize(9,  4), true).numTiles());
    EXPECT_EQ(4, TestTiling(IntSize(4, 4), IntSize(10,  4), true).numTiles());
    EXPECT_EQ(5, TestTiling(IntSize(4, 4), IntSize(11,  4), true).numTiles());

    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(1,  5), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(2,  5), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(3,  5), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(4,  5), true).numTiles());
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(5,  5), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(6,  5), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(7,  5), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(8,  5), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(9,  5), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(10,  5), true).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(11,  5), true).numTiles());

    EXPECT_EQ(30, TestTiling(IntSize(8, 5), IntSize(16, 32), true).numTiles());
}

TEST(TilingDataTest, tileXIndexFromSrcCoord)
{
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(3));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(4));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(5));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(6));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(7));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(8));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(9));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(10));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileXIndexFromSrcCoord(11));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(3));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(4));
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(5));
    EXPECT_EQ(5, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(6));
    EXPECT_EQ(6, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(7));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(8));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(9));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(10));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileXIndexFromSrcCoord(11));

    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(1, 1), false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileXIndexFromSrcCoord(2));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(4, 3), false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(4, 3), false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(4, 3), false).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(4, 3), false).tileXIndexFromSrcCoord(3));

    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(1, 1), true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileXIndexFromSrcCoord(2));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(4, 3), true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(4, 3), true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(4, 3), true).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(4, 3), true).tileXIndexFromSrcCoord(3));
}
TEST(TilingDataTest, tileYIndexFromSrcCoord)
{
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(3));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(4));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(5));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(6));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(7));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(8));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(9));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(10));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), false).tileYIndexFromSrcCoord(11));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(3));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(4));
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(5));
    EXPECT_EQ(5, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(6));
    EXPECT_EQ(6, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(7));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(8));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(9));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(10));
    EXPECT_EQ(7, TestTiling(IntSize(3, 3), IntSize(10, 10), true).tileYIndexFromSrcCoord(11));

    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(1, 1), false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), false).tileYIndexFromSrcCoord(2));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 4), false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 4), false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 4), false).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(3, 4), false).tileYIndexFromSrcCoord(3));

    EXPECT_EQ(0, TestTiling(IntSize(1, 1), IntSize(1, 1), true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(2, 2), IntSize(2, 2), true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 3), true).tileYIndexFromSrcCoord(2));

    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 4), true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 4), true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(3, 4), true).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(3, 4), true).tileYIndexFromSrcCoord(3));
}

TEST(TilingDataTest, tileSizeX)
{
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(5,  5), false).tileSizeX(0));
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(5,  5), true).tileSizeX(0));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(6,  6), false).tileSizeX(0));
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(6,  6), false).tileSizeX(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(6,  6), true).tileSizeX(0));
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(6,  6), true).tileSizeX(1));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(8,  8), false).tileSizeX(0));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(8,  8), false).tileSizeX(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(8,  8), true).tileSizeX(0));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(8,  8), true).tileSizeX(1));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(10, 10), false).tileSizeX(0));
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(10, 10), false).tileSizeX(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeX(0));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeX(1));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeX(2));

    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(11, 11), true).tileSizeX(2));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(12, 12), true).tileSizeX(2));

    EXPECT_EQ(3, TestTiling(IntSize(5, 9), IntSize(12, 17), true).tileSizeX(2));
}

TEST(TilingDataTest, tileSizeY)
{
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(5,  5), false).tileSizeY(0));
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(5,  5), true).tileSizeY(0));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(6,  6), false).tileSizeY(0));
    EXPECT_EQ(1, TestTiling(IntSize(5, 5), IntSize(6,  6), false).tileSizeY(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(6,  6), true).tileSizeY(0));
    EXPECT_EQ(2, TestTiling(IntSize(5, 5), IntSize(6,  6), true).tileSizeY(1));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(8,  8), false).tileSizeY(0));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(8,  8), false).tileSizeY(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(8,  8), true).tileSizeY(0));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(8,  8), true).tileSizeY(1));

    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(10, 10), false).tileSizeY(0));
    EXPECT_EQ(5, TestTiling(IntSize(5, 5), IntSize(10, 10), false).tileSizeY(1));
    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeY(0));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeY(1));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(10, 10), true).tileSizeY(2));

    EXPECT_EQ(4, TestTiling(IntSize(5, 5), IntSize(11, 11), true).tileSizeY(2));
    EXPECT_EQ(3, TestTiling(IntSize(5, 5), IntSize(12, 12), true).tileSizeY(2));

    EXPECT_EQ(3, TestTiling(IntSize(9, 5), IntSize(17, 12), true).tileSizeY(2));
}

TEST(TilingDataTest, tileSizeX_and_tilePositionX)
{
    // Single tile cases:
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1,   1), false).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1,   1), false).tilePositionX(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 100), false).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 100), false).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(3,   1), false).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3,   1), false).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(3, 100), false).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 100), false).tilePositionX(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1,   1), true).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1,   1), true).tilePositionX(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 100), true).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 100), true).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(3,   1), true).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3,   1), true).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(3, 100), true).tileSizeX(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(3, 100), true).tilePositionX(0));

    // Multiple tiles:
    // no border
    // positions 0, 3
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(6,   1), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6,   1), false).tileSizeX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6,   1), false).tileSizeX(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(6,   1), false).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6,   1), false).tilePositionX(1));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6, 100), false).tileSizeX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6, 100), false).tileSizeX(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(6, 100), false).tilePositionX(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6, 100), false).tilePositionX(1));

    // Multiple tiles:
    // with border
    // positions 0, 2, 3, 4
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(6,   1), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tileSizeX(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tileSizeX(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tileSizeX(2));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tileSizeX(3));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tilePositionX(0));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tilePositionX(1));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tilePositionX(2));
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(6,   1), true).tilePositionX(3));
    EXPECT_EQ(2, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tileSizeX(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tileSizeX(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tileSizeX(2));
    EXPECT_EQ(2, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tileSizeX(3));
    EXPECT_EQ(0, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tilePositionX(0));
    EXPECT_EQ(2, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tilePositionX(1));
    EXPECT_EQ(3, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tilePositionX(2));
    EXPECT_EQ(4, TestTiling(IntSize(3, 7), IntSize(6, 100), true).tilePositionX(3));
}

TEST(TilingDataTest, tileSizeY_and_tilePositionY)
{
    // Single tile cases:
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 1), false).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 1), false).tilePositionY(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(100, 1), false).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(100, 1), false).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 3), false).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 3), false).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(100, 3), false).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(100, 3), false).tilePositionY(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 1), true).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 1), true).tilePositionY(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(100, 1), true).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(100, 1), true).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 3), true).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 3), true).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(100, 3), true).tileSizeY(0));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(100, 3), true).tilePositionY(0));

    // Multiple tiles:
    // no border
    // positions 0, 3
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(1, 6), false).numTiles());
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 6), false).tileSizeY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 6), false).tileSizeY(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 6), false).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 6), false).tilePositionY(1));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(100, 6), false).tileSizeY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(100, 6), false).tileSizeY(1));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(100, 6), false).tilePositionY(0));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(100, 6), false).tilePositionY(1));

    // Multiple tiles:
    // with border
    // positions 0, 2, 3, 4
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(1, 6), true).numTiles());
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tileSizeY(0));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tileSizeY(1));
    EXPECT_EQ(1, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tileSizeY(2));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tileSizeY(3));
    EXPECT_EQ(0, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tilePositionY(0));
    EXPECT_EQ(2, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tilePositionY(1));
    EXPECT_EQ(3, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tilePositionY(2));
    EXPECT_EQ(4, TestTiling(IntSize(3, 3), IntSize(1, 6), true).tilePositionY(3));
    EXPECT_EQ(2, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tileSizeY(0));
    EXPECT_EQ(1, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tileSizeY(1));
    EXPECT_EQ(1, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tileSizeY(2));
    EXPECT_EQ(2, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tileSizeY(3));
    EXPECT_EQ(0, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tilePositionY(0));
    EXPECT_EQ(2, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tilePositionY(1));
    EXPECT_EQ(3, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tilePositionY(2));
    EXPECT_EQ(4, TestTiling(IntSize(7, 3), IntSize(100, 6), true).tilePositionY(3));
}

TEST(TilingDataTest, setTotalSize)
{
    TestTiling data(IntSize(5, 5), IntSize(5, 5), false);
    EXPECT_EQ(5, data.totalSize().width());
    EXPECT_EQ(5, data.totalSize().height());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(1, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));

    data.setTotalSize(IntSize(6, 5));
    EXPECT_EQ(6, data.totalSize().width());
    EXPECT_EQ(5, data.totalSize().height());
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(1, data.tileSizeX(1));
    EXPECT_EQ(1, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));

    data.setTotalSize(IntSize(5, 12));
    EXPECT_EQ(5, data.totalSize().width());
    EXPECT_EQ(12, data.totalSize().height());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(3, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));
    EXPECT_EQ(5, data.tileSizeY(1));
    EXPECT_EQ(2, data.tileSizeY(2));
}

TEST(TilingDataTest, setMaxTextureSizeNoBorders)
{
    TestTiling data(IntSize(8, 8), IntSize(16, 32), false);
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(4, data.numTilesY());

    data.setMaxTextureSize(IntSize(32, 32));
    EXPECT_EQ(IntSize(32, 32), data.maxTextureSize());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(1, data.numTilesY());

    data.setMaxTextureSize(IntSize(2, 2));
    EXPECT_EQ(IntSize(2, 2), data.maxTextureSize());
    EXPECT_EQ(8, data.numTilesX());
    EXPECT_EQ(16, data.numTilesY());

    data.setMaxTextureSize(IntSize(5, 5));
    EXPECT_EQ(IntSize(5, 5), data.maxTextureSize());
    EXPECT_EQ(4, data.numTilesX());
    EXPECT_EQ(7, data.numTilesY());

    data.setMaxTextureSize(IntSize(8, 5));
    EXPECT_EQ(IntSize(8, 5), data.maxTextureSize());
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(7, data.numTilesY());
}

TEST(TilingDataTest, setMaxTextureSizeBorders)
{
    TestTiling data(IntSize(8, 8), IntSize(16, 32), true);
    EXPECT_EQ(3, data.numTilesX());
    EXPECT_EQ(5, data.numTilesY());

    data.setMaxTextureSize(IntSize(32, 32));
    EXPECT_EQ(IntSize(32, 32), data.maxTextureSize());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(1, data.numTilesY());

    data.setMaxTextureSize(IntSize(2, 2));
    EXPECT_EQ(IntSize(2, 2), data.maxTextureSize());
    EXPECT_EQ(0, data.numTilesX());
    EXPECT_EQ(0, data.numTilesY());

    data.setMaxTextureSize(IntSize(5, 5));
    EXPECT_EQ(IntSize(5, 5), data.maxTextureSize());
    EXPECT_EQ(5, data.numTilesX());
    EXPECT_EQ(10, data.numTilesY());

    data.setMaxTextureSize(IntSize(8, 5));
    EXPECT_EQ(IntSize(8, 5), data.maxTextureSize());
    EXPECT_EQ(3, data.numTilesX());
    EXPECT_EQ(10, data.numTilesY());
}

TEST(TilingDataTest, assignment)
{
    {
        TestTiling source(IntSize(8, 8), IntSize(16, 32), true);
        TestTiling dest = source;
        EXPECT_EQ(source.borderTexels(), dest.borderTexels());
        EXPECT_EQ(source.maxTextureSize(), dest.maxTextureSize());
        EXPECT_EQ(source.numTilesX(), dest.numTilesX());
        EXPECT_EQ(source.numTilesY(), dest.numTilesY());
        EXPECT_EQ(source.totalSize().width(), dest.totalSize().width());
        EXPECT_EQ(source.totalSize().height(), dest.totalSize().height());
    }
    {
        TestTiling source(IntSize(7, 3), IntSize(6, 100), false);
        TestTiling dest(source);
        EXPECT_EQ(source.borderTexels(), dest.borderTexels());
        EXPECT_EQ(source.maxTextureSize(), dest.maxTextureSize());
        EXPECT_EQ(source.numTilesX(), dest.numTilesX());
        EXPECT_EQ(source.numTilesY(), dest.numTilesY());
        EXPECT_EQ(source.totalSize().width(), dest.totalSize().width());
        EXPECT_EQ(source.totalSize().height(), dest.totalSize().height());
    }
}

TEST(TilingDataTest, setBorderTexels)
{
    TestTiling data(IntSize(8, 8), IntSize(16, 32), false);
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(4, data.numTilesY());

    data.setHasBorderTexels(true);
    EXPECT_EQ(3, data.numTilesX());
    EXPECT_EQ(5, data.numTilesY());

    data.setHasBorderTexels(true);
    EXPECT_EQ(3, data.numTilesX());
    EXPECT_EQ(5, data.numTilesY());

    data.setHasBorderTexels(false);
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(4, data.numTilesY());
}

} // namespace
