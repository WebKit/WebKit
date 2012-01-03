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

#include "TilingData.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(TilingDataTest, numTiles_NoTiling)
{
    EXPECT_EQ(1, TilingData(16, 16, 16, false).numTiles());
    EXPECT_EQ(1, TilingData(16, 15, 15, true).numTiles());
    EXPECT_EQ(1, TilingData(16, 16, 16, true).numTiles());
    EXPECT_EQ(1, TilingData(16,  1, 16, false).numTiles());
    EXPECT_EQ(1, TilingData(15, 15, 15, true).numTiles());
}

TEST(TilingDataTest, numTiles_TilingNoBorders)
{
    EXPECT_EQ(0, TilingData(0,  0,  0, false).numTiles());
    EXPECT_EQ(0, TilingData(0,  4,  0, false).numTiles());
    EXPECT_EQ(0, TilingData(0,  0,  4, false).numTiles());
    EXPECT_EQ(0, TilingData(4,  4,  0, false).numTiles());
    EXPECT_EQ(0, TilingData(4,  0,  4, false).numTiles());
    EXPECT_EQ(0, TilingData(-8, 1,  1, false).numTiles());
    EXPECT_EQ(0, TilingData(-1, 1,  1, false).numTiles());
    EXPECT_EQ(0, TilingData(0,  1,  1, false).numTiles());

    EXPECT_EQ(1, TilingData(1,  1,  1, false).numTiles());
    EXPECT_EQ(2, TilingData(1,  1,  2, false).numTiles());
    EXPECT_EQ(2, TilingData(1,  2,  1, false).numTiles());
    EXPECT_EQ(1, TilingData(2,  1,  1, false).numTiles());
    EXPECT_EQ(1, TilingData(2,  1,  2, false).numTiles());
    EXPECT_EQ(1, TilingData(2,  2,  1, false).numTiles());
    EXPECT_EQ(1, TilingData(2,  2,  2, false).numTiles());
    EXPECT_EQ(1, TilingData(3,  3,  3, false).numTiles());

    EXPECT_EQ(1, TilingData(4,  1,  4, false).numTiles());
    EXPECT_EQ(1, TilingData(4,  2,  4, false).numTiles());
    EXPECT_EQ(1, TilingData(4,  3,  4, false).numTiles());
    EXPECT_EQ(1, TilingData(4,  4,  4, false).numTiles());
    EXPECT_EQ(2, TilingData(4,  5,  4, false).numTiles());
    EXPECT_EQ(2, TilingData(4,  6,  4, false).numTiles());
    EXPECT_EQ(2, TilingData(4,  7,  4, false).numTiles());
    EXPECT_EQ(2, TilingData(4,  8,  4, false).numTiles());
    EXPECT_EQ(3, TilingData(4,  9,  4, false).numTiles());
    EXPECT_EQ(3, TilingData(4, 10,  4, false).numTiles());
    EXPECT_EQ(3, TilingData(4, 11,  4, false).numTiles());

    EXPECT_EQ(1, TilingData(5,  1,  5, false).numTiles());
    EXPECT_EQ(1, TilingData(5,  2,  5, false).numTiles());
    EXPECT_EQ(1, TilingData(5,  3,  5, false).numTiles());
    EXPECT_EQ(1, TilingData(5,  4,  5, false).numTiles());
    EXPECT_EQ(1, TilingData(5,  5,  5, false).numTiles());
    EXPECT_EQ(2, TilingData(5,  6,  5, false).numTiles());
    EXPECT_EQ(2, TilingData(5,  7,  5, false).numTiles());
    EXPECT_EQ(2, TilingData(5,  8,  5, false).numTiles());
    EXPECT_EQ(2, TilingData(5,  9,  5, false).numTiles());
    EXPECT_EQ(2, TilingData(5, 10,  5, false).numTiles());
    EXPECT_EQ(3, TilingData(5, 11,  5, false).numTiles());

    EXPECT_EQ(1, TilingData(16, 16, 16, false).numTiles());
    EXPECT_EQ(1, TilingData(17, 16, 16, false).numTiles());
    EXPECT_EQ(4, TilingData(15, 16, 16, false).numTiles());
    EXPECT_EQ(4, TilingData(8, 16, 16, false).numTiles());
    EXPECT_EQ(6, TilingData(8, 17, 16, false).numTiles());
}

TEST(TilingDataTest, numTiles_TilingWithBorders)
{
    EXPECT_EQ(0, TilingData(0,  0,  0, true).numTiles());
    EXPECT_EQ(0, TilingData(0,  4,  0, true).numTiles());
    EXPECT_EQ(0, TilingData(0,  0,  4, true).numTiles());
    EXPECT_EQ(0, TilingData(4,  4,  0, true).numTiles());
    EXPECT_EQ(0, TilingData(4,  0,  4, true).numTiles());
    EXPECT_EQ(0, TilingData(-8, 1,  1, true).numTiles());
    EXPECT_EQ(0, TilingData(-1, 1,  1, true).numTiles());
    EXPECT_EQ(0, TilingData(0,  1,  1, true).numTiles());

    EXPECT_EQ(1, TilingData(1,  1,  1, true).numTiles());
    EXPECT_EQ(0, TilingData(1,  1,  2, true).numTiles());
    EXPECT_EQ(0, TilingData(1,  2,  1, true).numTiles());
    EXPECT_EQ(1, TilingData(2,  1,  1, true).numTiles());
    EXPECT_EQ(1, TilingData(2,  1,  2, true).numTiles());
    EXPECT_EQ(1, TilingData(2,  2,  1, true).numTiles());
    EXPECT_EQ(1, TilingData(2,  2,  2, true).numTiles());

    EXPECT_EQ(1, TilingData(3,  1,  3, true).numTiles());
    EXPECT_EQ(1, TilingData(3,  2,  3, true).numTiles());
    EXPECT_EQ(1, TilingData(3,  3,  3, true).numTiles());
    EXPECT_EQ(2, TilingData(3,  4,  3, true).numTiles());
    EXPECT_EQ(3, TilingData(3,  5,  3, true).numTiles());
    EXPECT_EQ(4, TilingData(3,  6,  3, true).numTiles());
    EXPECT_EQ(5, TilingData(3,  7,  3, true).numTiles());

    EXPECT_EQ(1, TilingData(4,  1,  4, true).numTiles());
    EXPECT_EQ(1, TilingData(4,  2,  4, true).numTiles());
    EXPECT_EQ(1, TilingData(4,  3,  4, true).numTiles());
    EXPECT_EQ(1, TilingData(4,  4,  4, true).numTiles());
    EXPECT_EQ(2, TilingData(4,  5,  4, true).numTiles());
    EXPECT_EQ(2, TilingData(4,  6,  4, true).numTiles());
    EXPECT_EQ(3, TilingData(4,  7,  4, true).numTiles());
    EXPECT_EQ(3, TilingData(4,  8,  4, true).numTiles());
    EXPECT_EQ(4, TilingData(4,  9,  4, true).numTiles());
    EXPECT_EQ(4, TilingData(4, 10,  4, true).numTiles());
    EXPECT_EQ(5, TilingData(4, 11,  4, true).numTiles());

    EXPECT_EQ(1, TilingData(5,  1,  5, true).numTiles());
    EXPECT_EQ(1, TilingData(5,  2,  5, true).numTiles());
    EXPECT_EQ(1, TilingData(5,  3,  5, true).numTiles());
    EXPECT_EQ(1, TilingData(5,  4,  5, true).numTiles());
    EXPECT_EQ(1, TilingData(5,  5,  5, true).numTiles());
    EXPECT_EQ(2, TilingData(5,  6,  5, true).numTiles());
    EXPECT_EQ(2, TilingData(5,  7,  5, true).numTiles());
    EXPECT_EQ(2, TilingData(5,  8,  5, true).numTiles());
    EXPECT_EQ(3, TilingData(5,  9,  5, true).numTiles());
    EXPECT_EQ(3, TilingData(5, 10,  5, true).numTiles());
    EXPECT_EQ(3, TilingData(5, 11,  5, true).numTiles());
}

TEST(TilingDataTest, tileXIndexFromSrcCoord)
{
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(3));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(4));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(5));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(6));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(7));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(8));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(9));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(10));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileXIndexFromSrcCoord(11));

    EXPECT_EQ(0, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(1, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(2, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(3));
    EXPECT_EQ(3, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(4));
    EXPECT_EQ(4, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(5));
    EXPECT_EQ(5, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(6));
    EXPECT_EQ(6, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(7));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(8));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(9));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(10));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileXIndexFromSrcCoord(11));

    EXPECT_EQ(0, TilingData(1, 1, 1, false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileXIndexFromSrcCoord(2));

    EXPECT_EQ(0, TilingData(3, 4, 3, false).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 4, 3, false).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 4, 3, false).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 4, 3, false).tileXIndexFromSrcCoord(3));

    EXPECT_EQ(0, TilingData(1, 1, 1, true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileXIndexFromSrcCoord(2));

    EXPECT_EQ(0, TilingData(3, 4, 3, true).tileXIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 4, 3, true).tileXIndexFromSrcCoord(1));
    EXPECT_EQ(1, TilingData(3, 4, 3, true).tileXIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 4, 3, true).tileXIndexFromSrcCoord(3));
}
TEST(TilingDataTest, tileYIndexFromSrcCoord)
{
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(3));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(4));
    EXPECT_EQ(1, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(5));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(6));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(7));
    EXPECT_EQ(2, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(8));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(9));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(10));
    EXPECT_EQ(3, TilingData(3, 10, 10, false).tileYIndexFromSrcCoord(11));

    EXPECT_EQ(0, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(1, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(2, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(3));
    EXPECT_EQ(3, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(4));
    EXPECT_EQ(4, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(5));
    EXPECT_EQ(5, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(6));
    EXPECT_EQ(6, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(7));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(8));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(9));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(10));
    EXPECT_EQ(7, TilingData(3, 10, 10, true).tileYIndexFromSrcCoord(11));

    EXPECT_EQ(0, TilingData(1, 1, 1, false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, false).tileYIndexFromSrcCoord(2));

    EXPECT_EQ(0, TilingData(3, 3, 4, false).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 4, false).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 4, false).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 3, 4, false).tileYIndexFromSrcCoord(3));

    EXPECT_EQ(0, TilingData(1, 1, 1, true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(2, 2, 2, true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(0, TilingData(3, 3, 3, true).tileYIndexFromSrcCoord(2));

    EXPECT_EQ(0, TilingData(3, 3, 4, true).tileYIndexFromSrcCoord(0));
    EXPECT_EQ(0, TilingData(3, 3, 4, true).tileYIndexFromSrcCoord(1));
    EXPECT_EQ(1, TilingData(3, 3, 4, true).tileYIndexFromSrcCoord(2));
    EXPECT_EQ(1, TilingData(3, 3, 4, true).tileYIndexFromSrcCoord(3));
}

TEST(TilingDataTest, tileSizeX)
{
    EXPECT_EQ(5, TilingData(5,  5,  5, false).tileSizeX(0));
    EXPECT_EQ(5, TilingData(5,  5,  5, true).tileSizeX(0));

    EXPECT_EQ(5, TilingData(5,  6,  6, false).tileSizeX(0));
    EXPECT_EQ(1, TilingData(5,  6,  6, false).tileSizeX(1));
    EXPECT_EQ(4, TilingData(5,  6,  6, true).tileSizeX(0));
    EXPECT_EQ(2, TilingData(5,  6,  6, true).tileSizeX(1));

    EXPECT_EQ(5, TilingData(5,  8,  8, false).tileSizeX(0));
    EXPECT_EQ(3, TilingData(5,  8,  8, false).tileSizeX(1));
    EXPECT_EQ(4, TilingData(5,  8,  8, true).tileSizeX(0));
    EXPECT_EQ(4, TilingData(5,  8,  8, true).tileSizeX(1));

    EXPECT_EQ(5, TilingData(5, 10, 10, false).tileSizeX(0));
    EXPECT_EQ(5, TilingData(5, 10, 10, false).tileSizeX(1));
    EXPECT_EQ(4, TilingData(5, 10, 10, true).tileSizeX(0));
    EXPECT_EQ(3, TilingData(5, 10, 10, true).tileSizeX(1));
    EXPECT_EQ(3, TilingData(5, 10, 10, true).tileSizeX(2));

    EXPECT_EQ(4, TilingData(5, 11, 11, true).tileSizeX(2));
    EXPECT_EQ(3, TilingData(5, 12, 12, true).tileSizeX(2));
}
TEST(TilingDataTest, tileSizeY)
{
    EXPECT_EQ(5, TilingData(5,  5,  5, false).tileSizeY(0));
    EXPECT_EQ(5, TilingData(5,  5,  5, true).tileSizeY(0));

    EXPECT_EQ(5, TilingData(5,  6,  6, false).tileSizeY(0));
    EXPECT_EQ(1, TilingData(5,  6,  6, false).tileSizeY(1));
    EXPECT_EQ(4, TilingData(5,  6,  6, true).tileSizeY(0));
    EXPECT_EQ(2, TilingData(5,  6,  6, true).tileSizeY(1));

    EXPECT_EQ(5, TilingData(5,  8,  8, false).tileSizeY(0));
    EXPECT_EQ(3, TilingData(5,  8,  8, false).tileSizeY(1));
    EXPECT_EQ(4, TilingData(5,  8,  8, true).tileSizeY(0));
    EXPECT_EQ(4, TilingData(5,  8,  8, true).tileSizeY(1));

    EXPECT_EQ(5, TilingData(5, 10, 10, false).tileSizeY(0));
    EXPECT_EQ(5, TilingData(5, 10, 10, false).tileSizeY(1));
    EXPECT_EQ(4, TilingData(5, 10, 10, true).tileSizeY(0));
    EXPECT_EQ(3, TilingData(5, 10, 10, true).tileSizeY(1));
    EXPECT_EQ(3, TilingData(5, 10, 10, true).tileSizeY(2));

    EXPECT_EQ(4, TilingData(5, 11, 11, true).tileSizeY(2));
    EXPECT_EQ(3, TilingData(5, 12, 12, true).tileSizeY(2));
}

TEST(TilingDataTest, tileSizeX_and_tilePositionX)
{
    // Single tile cases:
    EXPECT_EQ(1, TilingData(3, 1,   1, false).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 1,   1, false).tilePositionX(0));
    EXPECT_EQ(1, TilingData(3, 1, 100, false).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 1, 100, false).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 3,   1, false).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 3,   1, false).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 3, 100, false).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 3, 100, false).tilePositionX(0));
    EXPECT_EQ(1, TilingData(3, 1,   1, true).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 1,   1, true).tilePositionX(0));
    EXPECT_EQ(1, TilingData(3, 1, 100, true).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 1, 100, true).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 3,   1, true).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 3,   1, true).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 3, 100, true).tileSizeX(0));
    EXPECT_EQ(0, TilingData(3, 3, 100, true).tilePositionX(0));

    // Multiple tiles:
    // no border
    // positions 0, 3
    EXPECT_EQ(2, TilingData(3, 6,   1, false).numTiles());
    EXPECT_EQ(3, TilingData(3, 6,   1, false).tileSizeX(0));
    EXPECT_EQ(3, TilingData(3, 6,   1, false).tileSizeX(1));
    EXPECT_EQ(0, TilingData(3, 6,   1, false).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 6,   1, false).tilePositionX(1));
    EXPECT_EQ(3, TilingData(3, 6, 100, false).tileSizeX(0));
    EXPECT_EQ(3, TilingData(3, 6, 100, false).tileSizeX(1));
    EXPECT_EQ(0, TilingData(3, 6, 100, false).tilePositionX(0));
    EXPECT_EQ(3, TilingData(3, 6, 100, false).tilePositionX(1));

    // Multiple tiles:
    // with border
    // positions 0, 2, 3, 4
    EXPECT_EQ(4, TilingData(3, 6,   1, true).numTiles());
    EXPECT_EQ(2, TilingData(3, 6,   1, true).tileSizeX(0));
    EXPECT_EQ(1, TilingData(3, 6,   1, true).tileSizeX(1));
    EXPECT_EQ(1, TilingData(3, 6,   1, true).tileSizeX(2));
    EXPECT_EQ(2, TilingData(3, 6,   1, true).tileSizeX(3));
    EXPECT_EQ(0, TilingData(3, 6,   1, true).tilePositionX(0));
    EXPECT_EQ(2, TilingData(3, 6,   1, true).tilePositionX(1));
    EXPECT_EQ(3, TilingData(3, 6,   1, true).tilePositionX(2));
    EXPECT_EQ(4, TilingData(3, 6,   1, true).tilePositionX(3));
    EXPECT_EQ(2, TilingData(3, 6, 100, true).tileSizeX(0));
    EXPECT_EQ(1, TilingData(3, 6, 100, true).tileSizeX(1));
    EXPECT_EQ(1, TilingData(3, 6, 100, true).tileSizeX(2));
    EXPECT_EQ(2, TilingData(3, 6, 100, true).tileSizeX(3));
    EXPECT_EQ(0, TilingData(3, 6, 100, true).tilePositionX(0));
    EXPECT_EQ(2, TilingData(3, 6, 100, true).tilePositionX(1));
    EXPECT_EQ(3, TilingData(3, 6, 100, true).tilePositionX(2));
    EXPECT_EQ(4, TilingData(3, 6, 100, true).tilePositionX(3));
}

TEST(TilingDataTest, tileSizeY_and_tilePositionY)
{
    // Single tile cases:
    EXPECT_EQ(1, TilingData(3,   1, 1, false).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3,   1, 1, false).tilePositionY(0));
    EXPECT_EQ(1, TilingData(3, 100, 1, false).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3, 100, 1, false).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3,   1, 3, false).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3,   1, 3, false).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3, 100, 3, false).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3, 100, 3, false).tilePositionY(0));
    EXPECT_EQ(1, TilingData(3,   1, 1, true).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3,   1, 1, true).tilePositionY(0));
    EXPECT_EQ(1, TilingData(3, 100, 1, true).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3, 100, 1, true).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3,   1, 3, true).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3,   1, 3, true).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3, 100, 3, true).tileSizeY(0));
    EXPECT_EQ(0, TilingData(3, 100, 3, true).tilePositionY(0));

    // Multiple tiles:
    // no border
    // positions 0, 3
    EXPECT_EQ(2, TilingData(3,   1, 6, false).numTiles());
    EXPECT_EQ(3, TilingData(3,   1, 6, false).tileSizeY(0));
    EXPECT_EQ(3, TilingData(3,   1, 6, false).tileSizeY(1));
    EXPECT_EQ(0, TilingData(3,   1, 6, false).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3,   1, 6, false).tilePositionY(1));
    EXPECT_EQ(3, TilingData(3, 100, 6, false).tileSizeY(0));
    EXPECT_EQ(3, TilingData(3, 100, 6, false).tileSizeY(1));
    EXPECT_EQ(0, TilingData(3, 100, 6, false).tilePositionY(0));
    EXPECT_EQ(3, TilingData(3, 100, 6, false).tilePositionY(1));

    // Multiple tiles:
    // with border
    // positions 0, 2, 3, 4
    EXPECT_EQ(4, TilingData(3,   1, 6, true).numTiles());
    EXPECT_EQ(2, TilingData(3,   1, 6, true).tileSizeY(0));
    EXPECT_EQ(1, TilingData(3,   1, 6, true).tileSizeY(1));
    EXPECT_EQ(1, TilingData(3,   1, 6, true).tileSizeY(2));
    EXPECT_EQ(2, TilingData(3,   1, 6, true).tileSizeY(3));
    EXPECT_EQ(0, TilingData(3,   1, 6, true).tilePositionY(0));
    EXPECT_EQ(2, TilingData(3,   1, 6, true).tilePositionY(1));
    EXPECT_EQ(3, TilingData(3,   1, 6, true).tilePositionY(2));
    EXPECT_EQ(4, TilingData(3,   1, 6, true).tilePositionY(3));
    EXPECT_EQ(2, TilingData(3, 100, 6, true).tileSizeY(0));
    EXPECT_EQ(1, TilingData(3, 100, 6, true).tileSizeY(1));
    EXPECT_EQ(1, TilingData(3, 100, 6, true).tileSizeY(2));
    EXPECT_EQ(2, TilingData(3, 100, 6, true).tileSizeY(3));
    EXPECT_EQ(0, TilingData(3, 100, 6, true).tilePositionY(0));
    EXPECT_EQ(2, TilingData(3, 100, 6, true).tilePositionY(1));
    EXPECT_EQ(3, TilingData(3, 100, 6, true).tilePositionY(2));
    EXPECT_EQ(4, TilingData(3, 100, 6, true).tilePositionY(3));
}

TEST(TilingDataTest, setTotalSize)
{
    TilingData data(5, 5, 5, false);
    EXPECT_EQ(5, data.totalSizeX());
    EXPECT_EQ(5, data.totalSizeY());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(1, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));

    data.setTotalSize(6, 5);
    EXPECT_EQ(6, data.totalSizeX());
    EXPECT_EQ(5, data.totalSizeY());
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(1, data.tileSizeX(1));
    EXPECT_EQ(1, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));

    data.setTotalSize(5, 12);
    EXPECT_EQ(5, data.totalSizeX());
    EXPECT_EQ(12, data.totalSizeY());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(5, data.tileSizeX(0));
    EXPECT_EQ(3, data.numTilesY());
    EXPECT_EQ(5, data.tileSizeY(0));
    EXPECT_EQ(5, data.tileSizeY(1));
    EXPECT_EQ(2, data.tileSizeY(2));
}

TEST(TilingDataTest, setMaxTextureSizeNoBorders)
{
    TilingData data(8, 16, 32, false);
    EXPECT_EQ(2, data.numTilesX());
    EXPECT_EQ(4, data.numTilesY());

    data.setMaxTextureSize(32);
    EXPECT_EQ(32, data.maxTextureSize());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(1, data.numTilesY());

    data.setMaxTextureSize(2);
    EXPECT_EQ(2, data.maxTextureSize());
    EXPECT_EQ(8, data.numTilesX());
    EXPECT_EQ(16, data.numTilesY());

    data.setMaxTextureSize(5);
    EXPECT_EQ(5, data.maxTextureSize());
    EXPECT_EQ(4, data.numTilesX());
    EXPECT_EQ(7, data.numTilesY());
}

TEST(TilingDataTest, setMaxTextureSizeBorders)
{
    TilingData data(8, 16, 32, true);
    EXPECT_EQ(3, data.numTilesX());
    EXPECT_EQ(5, data.numTilesY());

    data.setMaxTextureSize(32);
    EXPECT_EQ(32, data.maxTextureSize());
    EXPECT_EQ(1, data.numTilesX());
    EXPECT_EQ(1, data.numTilesY());

    data.setMaxTextureSize(2);
    EXPECT_EQ(2, data.maxTextureSize());
    EXPECT_EQ(0, data.numTilesX());
    EXPECT_EQ(0, data.numTilesY());

    data.setMaxTextureSize(5);
    EXPECT_EQ(5, data.maxTextureSize());
    EXPECT_EQ(5, data.numTilesX());
    EXPECT_EQ(10, data.numTilesY());
}

TEST(TilingDataTest, assignment)
{
    {
        TilingData source(8, 16, 32, true);
        TilingData dest = source;
        EXPECT_EQ(source.borderTexels(), dest.borderTexels());
        EXPECT_EQ(source.maxTextureSize(), dest.maxTextureSize());
        EXPECT_EQ(source.numTilesX(), dest.numTilesX());
        EXPECT_EQ(source.numTilesY(), dest.numTilesY());
        EXPECT_EQ(source.totalSizeX(), dest.totalSizeX());
        EXPECT_EQ(source.totalSizeY(), dest.totalSizeY());
    }
    {
        TilingData source(3, 6, 100, false);
        TilingData dest(source);
        EXPECT_EQ(source.borderTexels(), dest.borderTexels());
        EXPECT_EQ(source.maxTextureSize(), dest.maxTextureSize());
        EXPECT_EQ(source.numTilesX(), dest.numTilesX());
        EXPECT_EQ(source.numTilesY(), dest.numTilesY());
        EXPECT_EQ(source.totalSizeX(), dest.totalSizeX());
        EXPECT_EQ(source.totalSizeY(), dest.totalSizeY());
    }
}

TEST(TilingDataTest, setBorderTexels)
{
    TilingData data(8, 16, 32, false);
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
