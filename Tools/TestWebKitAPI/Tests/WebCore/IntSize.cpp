/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#include <WebCore/FloatSize.h>
#include <WebCore/IntSize.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::IntSize rect)
{
    rect.setWidth(203);
    EXPECT_EQ(203, rect.width());
    rect.setHeight(73);
    EXPECT_EQ(73, rect.height());
}

static void testEmptySize(const WebCore::IntSize& rect)
{
    EXPECT_EQ(0, rect.width());
    EXPECT_EQ(0, rect.height());
    EXPECT_TRUE(rect.isEmpty());
    EXPECT_TRUE(rect.isZero());
}

TEST(IntSize, DefaultConstruction)
{
    WebCore::IntSize test;

    testEmptySize(test);

    testGetAndSet(test);
}

TEST(IntSize, ValueConstruction)
{
    WebCore::IntSize test(100, 200);

    EXPECT_EQ(100, test.width());
    EXPECT_EQ(200, test.height());
    EXPECT_FALSE(test.isEmpty());
    EXPECT_FALSE(test.isZero());

    static const float epsilon = 0.0001f;
    EXPECT_NEAR(0.5f, test.aspectRatio(), epsilon);
}

TEST(IntSize, FloatSizeConstruction)
{
    WebCore::FloatSize size(1024.2, 767.8);
    WebCore::IntSize test(size);

    EXPECT_EQ(1024, test.width());
    EXPECT_EQ(767, test.height());

    ASSERT_FALSE(test.isEmpty());
    ASSERT_FALSE(test.isZero());

    static const double epsilon = 0.001;
    EXPECT_NEAR(1.335f, test.aspectRatio(), epsilon);

    testGetAndSet(test);
}

TEST(IntSize, DiagonalLengthAndArea)
{
    WebCore::IntSize test(1024, 768);

    EXPECT_EQ(1638400, test.diagonalLengthSquared());
    EXPECT_EQ(786432U, test.area().unsafeGet());
}

TEST(IntSize, Scale)
{
    WebCore::IntSize test(1024, 768);

    test.scale(2.0f);

    EXPECT_EQ(2048, test.width());
    EXPECT_EQ(1536, test.height());

    test.scale(0.5f);

    EXPECT_EQ(1024, test.width());
    EXPECT_EQ(768, test.height());

    test.scale(2.0f, 0.5f);

    EXPECT_EQ(2048, test.width());
    EXPECT_EQ(384, test.height());
}

TEST(IntSize, Expand)
{
    WebCore::IntSize test(1024, 768);

    EXPECT_EQ(1024, test.width());
    EXPECT_EQ(768, test.height());

    test.expand(100, 50);

    EXPECT_EQ(1124, test.width());
    EXPECT_EQ(818, test.height());

    WebCore::IntSize other(2048, 700);

    auto expanded = test.expandedTo(other);

    EXPECT_EQ(2048, expanded.width());
    EXPECT_EQ(818, expanded.height());
}

TEST(IntSize, Shrink)
{
    WebCore::IntSize test(1024, 768);
    WebCore::IntSize other(1000, 700);

    auto shrunken = test.shrunkTo(other);

    EXPECT_EQ(1000, shrunken.width());
    EXPECT_EQ(700, shrunken.height());

    WebCore::IntSize other2(2000.0f, 700.0f);

    auto shrunken2 = test.shrunkTo(other2);

    EXPECT_EQ(1024, shrunken2.width());
    EXPECT_EQ(700, shrunken2.height());
}

TEST(IntSize, TransposedSize)
{
    WebCore::IntSize test(1024, 768);

    auto transposedSize = test.transposedSize();

    EXPECT_EQ(768, transposedSize.width());
    EXPECT_EQ(1024, transposedSize.height());
}

TEST(IntSize, Casting)
{
    WebCore::IntSize test(1024, 768);

#if USE(CG)
    CGSize cgSize = test;

    EXPECT_FLOAT_EQ(1024.0f, cgSize.width);
    EXPECT_FLOAT_EQ(768.0f, cgSize.height);

    CGSize cgSize2 = CGSizeMake(-22.3f, 14.6f);

    WebCore::IntSize testCG(cgSize2);

    EXPECT_EQ(-22, testCG.width());
    EXPECT_EQ(14, testCG.height());
#endif

#if PLATFORM(WIN)
    D2D1_SIZE_U d2dSizeU = test;
    EXPECT_EQ(1024, d2dSizeU.width);
    EXPECT_EQ(768, d2dSizeU.height);

    D2D1_SIZE_F d2dSizeF = test;
    EXPECT_FLOAT_EQ(1024.0f, d2dSizeF.width);
    EXPECT_FLOAT_EQ(768.0f, d2dSizeF.height);

    D2D1_SIZE_F d2dSizeF2 = D2D1::SizeF(-22.3f, 14.6f);

    WebCore::IntSize testD2D(d2dSizeF2);

    EXPECT_EQ(-22, testD2D.width());
    EXPECT_EQ(14, testD2D.height());

    D2D1_SIZE_F d2dSizeU2 = D2D1::SizeF(-23, 16);

    WebCore::IntSize testD2D2(d2dSizeU2);

    EXPECT_EQ(-23, testD2D2.width());
    EXPECT_EQ(16, testD2D2.height());
#endif
}

TEST(IntSize, AddSubtract)
{
    WebCore::IntSize a(512, 384);
    WebCore::IntSize b(100, 100);

    WebCore::IntSize c = a + b;

    EXPECT_EQ(612, c.width());
    EXPECT_EQ(484, c.height());

    a += b;

    EXPECT_EQ(612, a.width());
    EXPECT_EQ(484, a.height());

    WebCore::IntSize a2(512, 384);

    WebCore::IntSize d = a2 - b;

    EXPECT_EQ(412, d.width());
    EXPECT_EQ(284, d.height());

    a2 -= b;

    EXPECT_EQ(412, a2.width());
    EXPECT_EQ(284, a2.height());
}

TEST(IntSize, Negation)
{
    WebCore::IntSize a(512, 384);

    WebCore::IntSize negated = -a;

    EXPECT_EQ(-512, negated.width());
    EXPECT_EQ(-384, negated.height());
}

TEST(IntSize, Equality)
{
    WebCore::IntSize a(1024, 768);
    WebCore::IntSize b(1024, 768);
    WebCore::IntSize c(768, 534);

    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    ASSERT_FALSE(a == c);
    ASSERT_TRUE(a != c);
}

TEST(IntSize, Contract)
{
    WebCore::IntSize a(1024, 768);

    a.contract(100, 50);

    EXPECT_EQ(924, a.width());
    EXPECT_EQ(718, a.height());
}

TEST(IntSize, Clamp)
{
    WebCore::IntSize a(1024, 768);

    a.clampNegativeToZero();

    EXPECT_EQ(1024, a.width());
    EXPECT_EQ(768, a.height());

    WebCore::IntSize b(-1024, -768);

    b.clampNegativeToZero();

    EXPECT_EQ(0, b.width());
    EXPECT_EQ(0, b.height());

    WebCore::IntSize minimumSize(1024, 1000);

    a.clampToMinimumSize(minimumSize);

    EXPECT_EQ(1024, a.width());
    EXPECT_EQ(1000, a.height());
}

TEST(IntSize, ConstrainedBetween)
{
    WebCore::IntSize minimumSize(384, 256);
    WebCore::IntSize maximumSize(2048, 1536);

    WebCore::IntSize a(1024, 768);

    auto constrained1 = a.constrainedBetween(minimumSize, maximumSize);

    EXPECT_EQ(1024, constrained1.width());
    EXPECT_EQ(768, constrained1.height());

    WebCore::IntSize b(200, 100);

    auto constrained2 = b.constrainedBetween(minimumSize, maximumSize);

    EXPECT_EQ(384, constrained2.width());
    EXPECT_EQ(256, constrained2.height());

    WebCore::IntSize c(5000, 2000);

    auto constrained3 = c.constrainedBetween(minimumSize, maximumSize);

    EXPECT_EQ(2048, constrained3.width());
    EXPECT_EQ(1536, constrained3.height());
}

/*

IntSize constrainedBetween(const IntSize& min, const IntSize& max) const;

*/

}
