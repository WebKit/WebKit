/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::FloatSize size)
{
    size.setWidth(101.1f);
    EXPECT_FLOAT_EQ(101.1f, size.width());
    size.setHeight(218.2f);
    EXPECT_FLOAT_EQ(218.2f, size.height());
}

TEST(FloatSize, DefaultConstruction)
{
    WebCore::FloatSize test;

    EXPECT_FLOAT_EQ(0, test.width());
    EXPECT_FLOAT_EQ(0, test.height());

    ASSERT_TRUE(test.isEmpty());
    ASSERT_TRUE(test.isZero());

    testGetAndSet(test);
}

TEST(FloatSize, ValueConstruction)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

    EXPECT_FLOAT_EQ(1024.0f, test.width());
    EXPECT_FLOAT_EQ(768.0f, test.height());

    ASSERT_FALSE(test.isEmpty());
    ASSERT_FALSE(test.isZero());

    static const float epsilon = 0.0001f;
    EXPECT_NEAR(1.3333f, test.aspectRatio(), epsilon);

    testGetAndSet(test);
}

TEST(FloatSize, IntSizeConstruction)
{
    WebCore::IntSize size(1024, 768);
    WebCore::FloatSize test(size);

    EXPECT_FLOAT_EQ(1024.0f, test.width());
    EXPECT_FLOAT_EQ(768.0f, test.height());

    ASSERT_FALSE(test.isEmpty());
    ASSERT_FALSE(test.isZero());

    static const double epsilon = 0.0001;
    EXPECT_NEAR(1.3333f, test.aspectRatio(), epsilon);

    testGetAndSet(test);
}

TEST(FloatSize, Scale)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

    test.scale(2.0f);

    EXPECT_FLOAT_EQ(2048.0f, test.width());
    EXPECT_FLOAT_EQ(1536.0f, test.height());

    test.scale(0.5f);

    EXPECT_FLOAT_EQ(1024.0f, test.width());
    EXPECT_FLOAT_EQ(768.0f, test.height());

    test.scale(2.0f, 0.5f);

    EXPECT_FLOAT_EQ(2048.0f, test.width());
    EXPECT_FLOAT_EQ(384.0f, test.height());
}

TEST(FloatSize, Expand)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

    EXPECT_FLOAT_EQ(1024.0f, test.width());
    EXPECT_FLOAT_EQ(768.0f, test.height());

    test.expand(100.0f, 50.0f);

    EXPECT_FLOAT_EQ(1124.0f, test.width());
    EXPECT_FLOAT_EQ(818.0f, test.height());

    WebCore::FloatSize other(2048.0f, 700.0f);

    auto expanded = test.expandedTo(other);

    EXPECT_FLOAT_EQ(2048.0f, expanded.width());
    EXPECT_FLOAT_EQ(818.0f, expanded.height());
}

TEST(FloatSize, Shrink)
{
    WebCore::FloatSize test(1024.0f, 768.0f);
    WebCore::FloatSize other(1000.0f, 700.0f);

    auto shrunken = test.shrunkTo(other);

    EXPECT_FLOAT_EQ(1000.0f, shrunken.width());
    EXPECT_FLOAT_EQ(700.0f, shrunken.height());

    WebCore::FloatSize other2(2000.0f, 700.0f);

    auto shrunken2 = test.shrunkTo(other2);

    EXPECT_FLOAT_EQ(1024.0f, shrunken2.width());
    EXPECT_FLOAT_EQ(700.0f, shrunken2.height());
}

TEST(FloatSize, DiagonalLengthAndArea)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

    EXPECT_FLOAT_EQ(1280.0f, test.diagonalLength());
    EXPECT_FLOAT_EQ(1638400.0f, test.diagonalLengthSquared());
    EXPECT_FLOAT_EQ(786432.0f, test.area());
}

TEST(FloatSize, TransposedSize)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

    auto transposedSize = test.transposedSize();

    EXPECT_FLOAT_EQ(768.0f, transposedSize.width());
    EXPECT_FLOAT_EQ(1024.0f, transposedSize.height());
}

TEST(FloatSize, Casting)
{
    WebCore::FloatSize test(1024.0f, 768.0f);

#if USE(CG)
    CGSize cgSize = test;

    EXPECT_FLOAT_EQ(1024.0f, cgSize.width);
    EXPECT_FLOAT_EQ(768.0f, cgSize.height);

    CGSize cgSize2 = CGSizeMake(-22.3f, 14.2f);

    WebCore::FloatSize testCG(cgSize2);

    EXPECT_FLOAT_EQ(-22.3f, testCG.width());
    EXPECT_FLOAT_EQ(14.2f, testCG.height());
#endif

#if PLATFORM(WIN)
    D2D1_SIZE_F d2dSize = test;

    EXPECT_FLOAT_EQ(1024.0f, d2dSize.width);
    EXPECT_FLOAT_EQ(768.0f, d2dSize.height);

    D2D1_SIZE_F d2dSize2 = D2D1::SizeF(-22.3f, 14.2f);

    WebCore::FloatSize testD2D(d2dSize2);

    EXPECT_FLOAT_EQ(-22.3f, testD2D.width());
    EXPECT_FLOAT_EQ(14.2f, testD2D.height());
#endif
}

TEST(FloatSize, AddSubtract)
{
    WebCore::FloatSize a(512.0f, 384.0f);
    WebCore::FloatSize b(100.0f, 100.0f);

    WebCore::FloatSize c = a + b;

    EXPECT_FLOAT_EQ(612.0f, c.width());
    EXPECT_FLOAT_EQ(484.0f, c.height());

    a += b;

    EXPECT_FLOAT_EQ(612.0f, a.width());
    EXPECT_FLOAT_EQ(484.0f, a.height());

    WebCore::FloatSize a2(512.0f, 384.0f);

    WebCore::FloatSize d = a2 - b;

    EXPECT_FLOAT_EQ(412.0f, d.width());
    EXPECT_FLOAT_EQ(284.0f, d.height());

    a2 -= b;

    EXPECT_FLOAT_EQ(412.0f, a2.width());
    EXPECT_FLOAT_EQ(284.0f, a2.height());
}

TEST(FloatSize, Negation)
{
    WebCore::FloatSize a(512.0f, 384.0f);

    WebCore::FloatSize negated = -a;

    EXPECT_FLOAT_EQ(-512.0f, negated.width());
    EXPECT_FLOAT_EQ(-384.0f, negated.height());
}

TEST(FloatSize, Multiply)
{
    WebCore::FloatSize a(512.0f, 384.0f);

    WebCore::FloatSize multiplied = a * 2.0f;

    EXPECT_FLOAT_EQ(1024.0f, multiplied.width());
    EXPECT_FLOAT_EQ(768.0f, multiplied.height());

    WebCore::FloatSize multiplied2 = 3.0f * a;

    EXPECT_FLOAT_EQ(1536.0f, multiplied2.width());
    EXPECT_FLOAT_EQ(1152.0f, multiplied2.height());

    WebCore::FloatSize b(1024.0f, 768.0f);

    WebCore::FloatSize multiplied3 = a * b;

    EXPECT_FLOAT_EQ(524288.0f, multiplied3.width());
    EXPECT_FLOAT_EQ(294912.0f, multiplied3.height());
}

TEST(FloatSize, Divide)
{
    WebCore::FloatSize a(1024.0f, 768.0f);

    WebCore::FloatSize divided = a / 2.0f;

    EXPECT_FLOAT_EQ(512.0f, divided.width());
    EXPECT_FLOAT_EQ(384.0f, divided.height());

    WebCore::FloatSize b(512.0f, 256.0f);

    WebCore::FloatSize divided2 = 1024.0f / b;

    EXPECT_FLOAT_EQ(2.0f, divided2.width());
    EXPECT_FLOAT_EQ(4.0f, divided2.height());
}

TEST(FloatSize, Equality)
{
    WebCore::FloatSize a(1024.0f, 768.0f);
    WebCore::FloatSize b(1024.0f, 768.0f);
    WebCore::FloatSize c(768.0f, 534.0F);

    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    ASSERT_FALSE(a == c);
    ASSERT_TRUE(a != c);

    ASSERT_TRUE(WebCore::areEssentiallyEqual(a, b));
    ASSERT_FALSE(WebCore::areEssentiallyEqual(a, c));
}

TEST(FloatSize, Floors)
{
    WebCore::FloatSize a(1024.4f, 768.8f);

    WebCore::IntSize floorSize = WebCore::flooredIntSize(a);

    EXPECT_EQ(1024, floorSize.width());
    EXPECT_EQ(768, floorSize.height());

    WebCore::IntPoint floorPoint = WebCore::flooredIntPoint(a);

    EXPECT_EQ(1024, floorPoint.x());
    EXPECT_EQ(768, floorPoint.y());
}

TEST(FloatSize, Rounded)
{
    WebCore::FloatSize a(1024.4f, 768.8f);

    WebCore::IntSize roundedSize = WebCore::roundedIntSize(a);

    EXPECT_EQ(1024, roundedSize.width());
    EXPECT_EQ(769, roundedSize.height());

    WebCore::IntSize expandedSize = WebCore::expandedIntSize(a);

    EXPECT_EQ(1025, expandedSize.width());
    EXPECT_EQ(769, expandedSize.height());
}

}
