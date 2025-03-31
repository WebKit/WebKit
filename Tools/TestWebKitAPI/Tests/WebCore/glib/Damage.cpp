/*
 * Copyright (C) 2025 Igalia, S.L.
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

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "Test.h"
#include <WebCore/Damage.h>

namespace TestWebKitAPI {
using namespace WebCore;

TEST(Damage, Basics)
{
    Damage damage(IntSize { 2048, 1024 });
    EXPECT_TRUE(damage.isEmpty());
    EXPECT_EQ(damage.rects().size(), 0);
}

TEST(Damage, Mode)
{
    // Rectangles is the default mode.
    Damage rectsDamage(IntSize { 1024, 768 });
    EXPECT_TRUE(rectsDamage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_TRUE(rectsDamage.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_FALSE(rectsDamage.isEmpty());
    EXPECT_EQ(rectsDamage.rects().size(), 2);
    EXPECT_EQ(rectsDamage.bounds().x(), 100);
    EXPECT_EQ(rectsDamage.bounds().y(), 100);
    EXPECT_EQ(rectsDamage.bounds().width(), 400);
    EXPECT_EQ(rectsDamage.bounds().height(), 400);

    // BoundingBox always unite damage in bounds.
    Damage bboxDamage(IntSize { 1024, 768 }, Damage::Mode::BoundingBox);
    EXPECT_TRUE(bboxDamage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_TRUE(bboxDamage.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_FALSE(bboxDamage.isEmpty());
    EXPECT_EQ(bboxDamage.rects().size(), 1);
    EXPECT_EQ(bboxDamage.rects()[0], bboxDamage.bounds());
    EXPECT_EQ(bboxDamage.bounds().x(), 100);
    EXPECT_EQ(bboxDamage.bounds().y(), 100);
    EXPECT_EQ(bboxDamage.bounds().width(), 400);
    EXPECT_EQ(bboxDamage.bounds().height(), 400);

    // Full ignores any adds and always reports the whole area.
    Damage fullDamage(IntSize { 1024, 768 }, Damage::Mode::Full);
    EXPECT_FALSE(fullDamage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_FALSE(fullDamage.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_FALSE(fullDamage.isEmpty());
    EXPECT_EQ(fullDamage.rects().size(), 1);
    EXPECT_EQ(fullDamage.rects()[0], fullDamage.bounds());
    EXPECT_EQ(fullDamage.bounds().x(), 0);
    EXPECT_EQ(fullDamage.bounds().y(), 0);
    EXPECT_EQ(fullDamage.bounds().width(), 1024);
    EXPECT_EQ(fullDamage.bounds().height(), 768);

    // We can make a Damage full.
    Damage fullDamage2 = rectsDamage;
    fullDamage2.makeFull();
    EXPECT_FALSE(fullDamage2.isEmpty());
    EXPECT_EQ(fullDamage2.rects().size(), 1);
    EXPECT_EQ(fullDamage2.rects()[0], fullDamage.bounds());
    EXPECT_EQ(fullDamage2.bounds().x(), 0);
    EXPECT_EQ(fullDamage2.bounds().y(), 0);
    EXPECT_EQ(fullDamage2.bounds().width(), 1024);
    EXPECT_EQ(fullDamage2.bounds().height(), 768);

    // We can make a Damage full with different size.
    Damage fullDamage3 = rectsDamage;
    fullDamage3.makeFull(IntSize { 800, 600 });
    EXPECT_FALSE(fullDamage3.isEmpty());
    EXPECT_EQ(fullDamage3.rects().size(), 1);
    EXPECT_EQ(fullDamage3.bounds().x(), 0);
    EXPECT_EQ(fullDamage3.bounds().y(), 0);
    EXPECT_EQ(fullDamage3.bounds().width(), 800);
    EXPECT_EQ(fullDamage3.bounds().height(), 600);
}

TEST(Damage, Move)
{
    Damage damage(IntSize { 2048, 1024 });
    EXPECT_TRUE(damage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_TRUE(damage.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_EQ(damage.bounds().x(), 100);
    EXPECT_EQ(damage.bounds().y(), 100);
    EXPECT_EQ(damage.bounds().width(), 400);
    EXPECT_EQ(damage.bounds().height(), 400);

    Damage other = WTFMove(damage);
    EXPECT_FALSE(other.isEmpty());
    EXPECT_EQ(other.rects().size(), 2);
    EXPECT_EQ(other.bounds().x(), 100);
    EXPECT_EQ(other.bounds().y(), 100);
    EXPECT_EQ(other.bounds().width(), 400);
    EXPECT_EQ(other.bounds().height(), 400);
    EXPECT_TRUE(damage.isEmpty());
    EXPECT_EQ(damage.rects().size(), 0);
    EXPECT_EQ(damage.bounds().x(), 100);
    EXPECT_EQ(damage.bounds().y(), 100);
    EXPECT_EQ(damage.bounds().width(), 400);
    EXPECT_EQ(damage.bounds().height(), 400);
}

TEST(Damage, AddRect)
{
    Damage damage(IntSize { 2048, 1024 });
    EXPECT_TRUE(damage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_EQ(damage.rects().size(), 1);

    // When there's only one rect, that should be the bounds.
    EXPECT_EQ(damage.bounds().x(), 100);
    EXPECT_EQ(damage.bounds().y(), 100);
    EXPECT_EQ(damage.bounds().width(), 200);
    EXPECT_EQ(damage.bounds().height(), 200);

    // When there's only one rect, adding a rect already contained
    // by the bounding box does nothing.
    EXPECT_FALSE(damage.add(IntRect { 150, 150, 100, 100 }));
    EXPECT_EQ(damage.rects().size(), 1);

    // Adding an empty rect does nothing.
    EXPECT_FALSE(damage.add(IntRect { }));
    EXPECT_EQ(damage.rects().size(), 1);

    // Adding a new rect not contained by previous one adds it to the list.
    EXPECT_TRUE(damage.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_EQ(damage.rects().size(), 2);

    // Now the bounding box contains the two rectangles.
    EXPECT_EQ(damage.bounds().x(), 100);
    EXPECT_EQ(damage.bounds().y(), 100);
    EXPECT_EQ(damage.bounds().width(), 400);
    EXPECT_EQ(damage.bounds().height(), 400);

    // Adding a rect containing the bounds makes it the only rect.
    EXPECT_TRUE(damage.add(IntRect { 50, 50, 500, 500 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_EQ(damage.bounds().x(), 50);
    EXPECT_EQ(damage.bounds().y(), 50);
    EXPECT_EQ(damage.bounds().width(), 500);
    EXPECT_EQ(damage.bounds().height(), 500);

    // Adding FloatRect takes the enclosingIntRect
    EXPECT_TRUE(damage.add(FloatRect { 1024.50, 1024.25, 50.32, 25.75 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_EQ(damage.rects().last().x(), 1024);
    EXPECT_EQ(damage.rects().last().y(), 1024);
    EXPECT_EQ(damage.rects().last().width(), 51);
    EXPECT_EQ(damage.rects().last().height(), 26);

    // Adding an empty FloatRect does nothing.
    EXPECT_FALSE(damage.add(FloatRect { 1024.50, 1024.25, 0, 0 }));
    EXPECT_EQ(damage.rects().size(), 2);
}

TEST(Damage, AddDamage)
{
    Damage damage(IntSize { 2048, 1024 });
    EXPECT_TRUE(damage.add(IntRect { 100, 100, 200, 200 }));
    EXPECT_EQ(damage.rects().size(), 1);

    // Adding empty Damage does nothing.
    Damage other(IntSize { 2048, 1024 });
    EXPECT_FALSE(damage.add(other));
    EXPECT_EQ(damage.rects().size(), 1);

    // Adding a valid Damage adds its rectangles.
    EXPECT_TRUE(other.add(IntRect { 300, 300, 200, 200 }));
    EXPECT_EQ(other.rects().size(), 1);
    EXPECT_TRUE(damage.add(other));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_EQ(damage.bounds().x(), 100);
    EXPECT_EQ(damage.bounds().y(), 100);
    EXPECT_EQ(damage.bounds().width(), 400);
    EXPECT_EQ(damage.bounds().height(), 400);
}

TEST(Damage, Unite)
{
    // Add several rects to the first tile.
    Damage damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 200, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 0, 200, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 200, 200, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 128, 128, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_FALSE(damage.rects()[0].isEmpty());
    EXPECT_TRUE(damage.rects()[1].isEmpty());
    EXPECT_TRUE(damage.rects()[2].isEmpty());
    EXPECT_TRUE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[0], damage.bounds());

    // Add several rects to the second tile.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 300, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 500, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 300, 200, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 500, 200, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 384, 128, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.rects()[0].isEmpty());
    EXPECT_FALSE(damage.rects()[1].isEmpty());
    EXPECT_TRUE(damage.rects()[2].isEmpty());
    EXPECT_TRUE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[1], damage.bounds());

    // Add several rects to the third tile.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 200, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 0, 500, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 200, 500, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 128, 384, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.rects()[0].isEmpty());
    EXPECT_TRUE(damage.rects()[1].isEmpty());
    EXPECT_FALSE(damage.rects()[2].isEmpty());
    EXPECT_TRUE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[2], damage.bounds());

    // Add several rects to the fourth tile.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 300, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 500, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 300, 500, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 500, 500, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 384, 384, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.rects()[0].isEmpty());
    EXPECT_TRUE(damage.rects()[1].isEmpty());
    EXPECT_TRUE(damage.rects()[2].isEmpty());
    EXPECT_FALSE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[3], damage.bounds());

    // Add one rect per tile.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 300, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 0, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 300, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_FALSE(damage.rects()[0].isEmpty());
    EXPECT_EQ(damage.rects()[0].x(), 0);
    EXPECT_EQ(damage.rects()[0].y(), 0);
    EXPECT_EQ(damage.rects()[0].width(), 4);
    EXPECT_EQ(damage.rects()[0].height(), 4);
    EXPECT_FALSE(damage.rects()[1].isEmpty());
    EXPECT_EQ(damage.rects()[1].x(), 300);
    EXPECT_EQ(damage.rects()[1].y(), 0);
    EXPECT_EQ(damage.rects()[1].width(), 4);
    EXPECT_EQ(damage.rects()[1].height(), 4);
    EXPECT_FALSE(damage.rects()[2].isEmpty());
    EXPECT_EQ(damage.rects()[2].x(), 0);
    EXPECT_EQ(damage.rects()[2].y(), 300);
    EXPECT_EQ(damage.rects()[2].width(), 4);
    EXPECT_EQ(damage.rects()[2].height(), 4);
    EXPECT_FALSE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[3].x(), 300);
    EXPECT_EQ(damage.rects()[3].y(), 300);
    EXPECT_EQ(damage.rects()[3].width(), 4);
    EXPECT_EQ(damage.rects()[3].height(), 4);

    // Add rects with points off the grid area.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { -2, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 50, -2, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 2);
    EXPECT_TRUE(damage.add(IntRect { 550, 0, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 3);
    EXPECT_TRUE(damage.add(IntRect { 300, -2, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { -2, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 50, 550, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 300, 550, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 550, 300, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_FALSE(damage.rects()[0].isEmpty());
    EXPECT_EQ(damage.rects()[0].x(), -2);
    EXPECT_EQ(damage.rects()[0].y(), -2);
    EXPECT_EQ(damage.rects()[0].width(), 56);
    EXPECT_EQ(damage.rects()[0].height(), 6);
    EXPECT_FALSE(damage.rects()[1].isEmpty());
    EXPECT_EQ(damage.rects()[1].x(), 300);
    EXPECT_EQ(damage.rects()[1].y(), -2);
    EXPECT_EQ(damage.rects()[1].width(), 254);
    EXPECT_EQ(damage.rects()[1].height(), 6);
    EXPECT_FALSE(damage.rects()[2].isEmpty());
    EXPECT_EQ(damage.rects()[2].x(), -2);
    EXPECT_EQ(damage.rects()[2].y(), 300);
    EXPECT_EQ(damage.rects()[2].width(), 56);
    EXPECT_EQ(damage.rects()[2].height(), 254);
    EXPECT_FALSE(damage.rects()[3].isEmpty());
    EXPECT_EQ(damage.rects()[3].x(), 300);
    EXPECT_EQ(damage.rects()[3].y(), 300);
    EXPECT_EQ(damage.rects()[3].width(), 254);
    EXPECT_EQ(damage.rects()[3].height(), 254);

    // Add several rects and check that unite works for single tile grid.
    damage = Damage(IntSize { 128, 128 });
    EXPECT_TRUE(damage.add(IntRect { 10, 10, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 60, 60, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 70, 10, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 120, 60, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 10, 70, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);
    EXPECT_TRUE(damage.add(IntRect { 120, 120, 4, 4 }));
    EXPECT_EQ(damage.rects().size(), 1);

    // Grid size should be ceiled.
    damage = Damage(IntSize { 512, 333 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 1, 1, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 2, 2, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 3, 3, 1, 1 }));
    EXPECT_EQ(damage.rects().size(), 4);

    // Grid size should be ceiled with high precision.
    damage = Damage(IntSize { 257, 50 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 1, 1, 1, 1 }));
    EXPECT_EQ(damage.rects().size(), 2);

    // Unification should work correctly when grid does not start and { 0, 0 }.
    damage = Damage(IntRect { 256, 256, 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 300, 300, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 600, 300, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 300, 600, 1, 1 }));
    EXPECT_TRUE(damage.add(IntRect { 600, 600, 1, 1 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_TRUE(damage.add(IntRect { 301, 301, 1, 1 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_EQ(damage.rects()[0], IntRect(300, 300, 2, 2));
    EXPECT_EQ(damage.rects()[1], IntRect(600, 300, 1, 1));
    EXPECT_EQ(damage.rects()[2], IntRect(300, 600, 1, 1));
    EXPECT_EQ(damage.rects()[3], IntRect(600, 600, 1, 1));
}

TEST(Damage, RectsForPainting)
{
    // The function should return the original rect when theres only a single one.
    Damage damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 250, 250, 12, 12 }));
    ASSERT_EQ(damage.rectsForPainting().size(), 1);
    EXPECT_EQ(damage.rectsForPainting()[0], IntRect(250, 250, 12, 12));

    // The function should remove overlaps.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 100, 100 }));
    EXPECT_TRUE(damage.add(IntRect { 50, 50, 100, 100 }));
    ASSERT_EQ(damage.rectsForPainting().size(), 1);
    EXPECT_EQ(damage.rectsForPainting()[0], IntRect(0, 0, 150, 150));

    // The function should remove empty rects.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 10, 10, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 20, 20, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 30, 30, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 40, 40, 10, 10 }));
    EXPECT_EQ(damage.rects().size(), 4);
    ASSERT_EQ(damage.rectsForPainting().size(), 1);
    EXPECT_EQ(damage.rectsForPainting()[0], IntRect(0, 0, 50, 50));

    // The function should clip the rects.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { -2, -2, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 504, 504, 10, 10 }));
    ASSERT_EQ(damage.rectsForPainting().size(), 2);
    EXPECT_EQ(damage.rectsForPainting()[0], IntRect(0, 0, 8, 8));
    EXPECT_EQ(damage.rectsForPainting()[1], IntRect(504, 504, 8, 8));

    // The function should preserve the layout of cells when unification is enabled.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 0, 0, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 10, 10, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 0, 256, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256, 0, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256, 256, 10, 10 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_EQ(damage.rects(), damage.rectsForPainting());

    // The function should preserve the layout of cells when unification is enabled
    // and the grid does not start at { 0, 0 }.
    damage = Damage(IntRect { 256, 256, 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 256, 256, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256 + 10, 256 + 10, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256, 256 + 256, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256 + 256, 256, 10, 10 }));
    EXPECT_TRUE(damage.add(IntRect { 256 + 256, 256 + 256, 10, 10 }));
    EXPECT_EQ(damage.rects().size(), 4);
    EXPECT_EQ(damage.rects(), damage.rectsForPainting());

    // The function should split a rect spanning multiple cells.
    damage = Damage(IntSize { 512, 512 });
    EXPECT_TRUE(damage.add(IntRect { 250, 250, 12, 12 }));
    EXPECT_TRUE(damage.add(IntRect { 249, 249, 1, 1 }));
    ASSERT_EQ(damage.rectsForPainting().size(), 4);
    EXPECT_EQ(damage.rectsForPainting()[0], IntRect(249, 249, 7, 7));
    EXPECT_EQ(damage.rectsForPainting()[1], IntRect(256, 250, 6, 6));
    EXPECT_EQ(damage.rectsForPainting()[2], IntRect(250, 256, 6, 6));
    EXPECT_EQ(damage.rectsForPainting()[3], IntRect(256, 256, 6, 6));

    // The function should just return original rects when mode != Mode::Rectangles.
    damage = Damage(IntRect { 1024, 512, 512, 512 }, Damage::Mode::BoundingBox);
    EXPECT_TRUE(damage.add(IntRect { 1278, 678, 9, 341 }));
    EXPECT_TRUE(damage.add(IntRect { 1285, 678, 5, 341 }));
    EXPECT_FALSE(damage.add(IntRect { 1279, 678, 9, 341 }));
    EXPECT_TRUE(damage.add(IntRect { 1286, 678, 5, 341 }));
    EXPECT_EQ(damage.rects(), damage.rectsForPainting());
    damage = Damage(IntRect { 1024, 512, 512, 512 }, Damage::Mode::Full);
    EXPECT_EQ(damage.rects(), damage.rectsForPainting());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK) || PLATFORM(WPE)
