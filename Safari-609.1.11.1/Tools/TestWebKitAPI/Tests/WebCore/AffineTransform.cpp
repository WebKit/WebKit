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

#include <WebCore/AffineTransform.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/TransformationMatrix.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::AffineTransform& affineTransform)
{
    affineTransform.setA(1.1);
    EXPECT_DOUBLE_EQ(1.1, affineTransform.a());
    affineTransform.setB(2.2);
    EXPECT_DOUBLE_EQ(2.2, affineTransform.b());
    affineTransform.setC(3.3);
    EXPECT_DOUBLE_EQ(3.3, affineTransform.c());
    affineTransform.setD(4.4);
    EXPECT_DOUBLE_EQ(4.4, affineTransform.d());
    affineTransform.setE(5.5);
    EXPECT_DOUBLE_EQ(5.5, affineTransform.e());
    affineTransform.setF(6.6);
    EXPECT_DOUBLE_EQ(6.6, affineTransform.f());
}

static void testIdentity(const WebCore::AffineTransform& transform)
{
    EXPECT_DOUBLE_EQ(1.0, transform.a());
    EXPECT_DOUBLE_EQ(0.0, transform.b());
    EXPECT_DOUBLE_EQ(0.0, transform.c());
    EXPECT_DOUBLE_EQ(1.0, transform.d());
    EXPECT_DOUBLE_EQ(0.0, transform.e());
    EXPECT_DOUBLE_EQ(0.0, transform.f());
}

TEST(AffineTransform, DefaultConstruction)
{
    WebCore::AffineTransform test;

    testIdentity(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}

static void testValueConstruction(const WebCore::AffineTransform& transform)
{
    EXPECT_DOUBLE_EQ(6.0, transform.a());
    EXPECT_DOUBLE_EQ(5.0, transform.b());
    EXPECT_DOUBLE_EQ(4.0, transform.c());
    EXPECT_DOUBLE_EQ(3.0, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

static void testDoubled(const WebCore::AffineTransform& transform)
{
    EXPECT_DOUBLE_EQ(12.0, transform.a());
    EXPECT_DOUBLE_EQ(10.0, transform.b());
    EXPECT_DOUBLE_EQ(8.0, transform.c());
    EXPECT_DOUBLE_EQ(6.0, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

static void testHalved(const WebCore::AffineTransform& transform)
{
    EXPECT_DOUBLE_EQ(3.0, transform.a());
    EXPECT_DOUBLE_EQ(2.5, transform.b());
    EXPECT_DOUBLE_EQ(2.0, transform.c());
    EXPECT_DOUBLE_EQ(1.5, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

TEST(AffineTransform, ValueConstruction)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
    ASSERT_FALSE(test.preservesAxisAlignment());
}

#if USE(CG)
TEST(AffineTransform, CGAffineTransformConstruction)
{
    CGAffineTransform cgTransform = CGAffineTransformMake(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::AffineTransform test(cgTransform);

    testValueConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}
#endif

#if PLATFORM(WIN)
TEST(AffineTransform, D2D1MatrixConstruction)
{
    D2D1_MATRIX_3X2_F d2dTransform = D2D1::Matrix3x2F(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::AffineTransform test(d2dTransform);

    testValueConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}
#endif

TEST(AffineTransform, Identity)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    ASSERT_FALSE(test.isIdentity());
    ASSERT_FALSE(test.isIdentityOrTranslation());
    ASSERT_FALSE(test.isIdentityOrTranslationOrFlipped());
    ASSERT_FALSE(test.preservesAxisAlignment());

    test.makeIdentity();

    ASSERT_TRUE(test.isIdentity());
    ASSERT_TRUE(test.isIdentityOrTranslation());
    ASSERT_TRUE(test.isIdentityOrTranslationOrFlipped());
    ASSERT_TRUE(test.preservesAxisAlignment());

    testIdentity(test);
}

TEST(AffineTransform, MapFloatPoint)
{
    WebCore::AffineTransform test;
    WebCore::FloatPoint point(100.0f, 50.0f);

    auto mappedPoint = test.mapPoint(point);

    ASSERT_FLOAT_EQ(100.0f, mappedPoint.x());
    ASSERT_FLOAT_EQ(50.0f, mappedPoint.y());

    test.setD(2.0);

    auto mappedPoint2 = test.mapPoint(point);

    ASSERT_FLOAT_EQ(100.0f, mappedPoint2.x());
    ASSERT_FLOAT_EQ(100.0f, mappedPoint2.y());

    test.setA(0.5);

    auto mappedPoint3 = test.mapPoint(point);

    ASSERT_FLOAT_EQ(50.0f, mappedPoint3.x());
    ASSERT_FLOAT_EQ(100.0f, mappedPoint3.y());
}

TEST(AffineTransform, MapIntPoint)
{
    WebCore::AffineTransform test;
    WebCore::IntPoint point(100, 50);

    auto mappedPoint = test.mapPoint(point);

    ASSERT_EQ(100, mappedPoint.x());
    ASSERT_EQ(50, mappedPoint.y());

    test.setD(2.0);

    auto mappedPoint2 = test.mapPoint(point);

    ASSERT_EQ(100, mappedPoint2.x());
    ASSERT_EQ(100, mappedPoint2.y());

    test.setA(0.5);

    auto mappedPoint3 = test.mapPoint(point);

    ASSERT_EQ(50, mappedPoint3.x());
    ASSERT_EQ(100, mappedPoint3.y());
}

TEST(AffineTransform, MapIntSize)
{
    WebCore::AffineTransform test;
    WebCore::IntSize size(200, 300);

    auto mappedSize = test.mapSize(size);

    ASSERT_EQ(200, mappedSize.width());
    ASSERT_EQ(300, mappedSize.height());

    test.setD(2.0);

    auto mappedSize2 = test.mapSize(size);

    ASSERT_EQ(200, mappedSize2.width());
    ASSERT_EQ(600, mappedSize2.height());

    test.setA(0.5);

    auto mappedSize3 = test.mapSize(size);

    ASSERT_EQ(100, mappedSize3.width());
    ASSERT_EQ(600, mappedSize3.height());
}

TEST(AffineTransform, MapFloatSize)
{
    WebCore::AffineTransform test;
    WebCore::FloatSize size(200.0f, 300.0f);

    auto mappedSize = test.mapSize(size);

    ASSERT_EQ(200.0f, mappedSize.width());
    ASSERT_EQ(300.0f, mappedSize.height());

    test.setD(2.0);

    auto mappedSize2 = test.mapSize(size);

    ASSERT_EQ(200.0f, mappedSize2.width());
    ASSERT_EQ(600.0f, mappedSize2.height());

    test.setA(0.5);

    auto mappedSize3 = test.mapSize(size);

    ASSERT_EQ(100.0f, mappedSize3.width());
    ASSERT_EQ(600.0f, mappedSize3.height());
}

TEST(AffineTransform, MapIntRect)
{
    WebCore::AffineTransform test;
    WebCore::IntRect rect(10, 20, 200, 300);

    auto mappedRect = test.mapRect(rect);

    ASSERT_EQ(10, mappedRect.x());
    ASSERT_EQ(20, mappedRect.y());
    ASSERT_EQ(200, mappedRect.width());
    ASSERT_EQ(300, mappedRect.height());

    test.setD(2.0);

    auto mappedRect2 = test.mapRect(rect);

    ASSERT_EQ(10, mappedRect2.x());
    ASSERT_EQ(40, mappedRect2.y());
    ASSERT_EQ(200, mappedRect2.width());
    ASSERT_EQ(600, mappedRect2.height());

    test.setA(0.5);

    auto mappedRect3 = test.mapRect(rect);

    ASSERT_EQ(5, mappedRect3.x());
    ASSERT_EQ(40, mappedRect3.y());
    ASSERT_EQ(100, mappedRect3.width());
    ASSERT_EQ(600, mappedRect3.height());
}

TEST(AffineTransform, MapFloatRect)
{
    WebCore::AffineTransform test;
    WebCore::FloatRect rect(10.f, 20.0f, 200.0f, 300.0f);

    auto mappedRect = test.mapRect(rect);

    ASSERT_FLOAT_EQ(10.0f, mappedRect.x());
    ASSERT_FLOAT_EQ(20.0f, mappedRect.y());
    ASSERT_FLOAT_EQ(200.0f, mappedRect.width());
    ASSERT_FLOAT_EQ(300.0f, mappedRect.height());

    test.setD(2.0);

    auto mappedRect2 = test.mapRect(rect);

    ASSERT_FLOAT_EQ(10.0f, mappedRect2.x());
    ASSERT_FLOAT_EQ(40.0f, mappedRect2.y());
    ASSERT_FLOAT_EQ(200.0f, mappedRect2.width());
    ASSERT_FLOAT_EQ(600.0f, mappedRect2.height());

    test.setA(0.5);

    auto mappedRect3 = test.mapRect(rect);

    ASSERT_FLOAT_EQ(5.0f, mappedRect3.x());
    ASSERT_FLOAT_EQ(40.0f, mappedRect3.y());
    ASSERT_FLOAT_EQ(100.0f, mappedRect3.width());
    ASSERT_FLOAT_EQ(600.0f, mappedRect3.height());
}

TEST(AffineTransform, MapFloatQuad)
{
    WebCore::FloatRect rect(100.0f, 100.0f, 100.0f, 50.0f);
    WebCore::FloatQuad quad(rect);

    ASSERT_FLOAT_EQ(100.0f, quad.p1().x());
    ASSERT_FLOAT_EQ(100.0f, quad.p1().y());
    ASSERT_FLOAT_EQ(200.0f, quad.p2().x());
    ASSERT_FLOAT_EQ(100.0f, quad.p2().y());
    ASSERT_FLOAT_EQ(200.0f, quad.p3().x());
    ASSERT_FLOAT_EQ(150.0f, quad.p3().y());
    ASSERT_FLOAT_EQ(100.0f, quad.p4().x());
    ASSERT_FLOAT_EQ(150.0f, quad.p4().y());

    WebCore::AffineTransform test;
    auto mappedQuad = test.mapQuad(quad);

    ASSERT_FLOAT_EQ(100.0f, mappedQuad.p1().x());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad.p1().y());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad.p2().x());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad.p2().y());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad.p3().x());
    ASSERT_FLOAT_EQ(150.0f, mappedQuad.p3().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad.p4().x());
    ASSERT_FLOAT_EQ(150.0f, mappedQuad.p4().y());

    test.setD(2.0);

    auto mappedQuad2 = test.mapQuad(quad);

    ASSERT_FLOAT_EQ(100.0f, mappedQuad2.p1().x());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad2.p1().y());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad2.p2().x());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad2.p2().y());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad2.p3().x());
    ASSERT_FLOAT_EQ(300.0f, mappedQuad2.p3().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad2.p4().x());
    ASSERT_FLOAT_EQ(300.0f, mappedQuad2.p4().y());

    test.setA(0.5);

    auto mappedQuad3 = test.mapQuad(quad);

    ASSERT_FLOAT_EQ(50.0f, mappedQuad3.p1().x());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad3.p1().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad3.p2().x());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad3.p2().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad3.p3().x());
    ASSERT_FLOAT_EQ(300.0f, mappedQuad3.p3().y());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad3.p4().x());
    ASSERT_FLOAT_EQ(300.0f, mappedQuad3.p4().y());
}

TEST(AffineTransform, Multiply)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::AffineTransform identity;

    testValueConstruction(test);

    test.multiply(identity);

    testValueConstruction(test);

    WebCore::AffineTransform doubler(2.0, 0.0, 0.0, 2.0, 0.0, 0.0);

    test.multiply(doubler);

    testDoubled(test);

    WebCore::AffineTransform halver(0.5, 0.0, 0.0, 0.5, 0.0, 0.0);

    test.multiply(halver);

    testValueConstruction(test);

    test.multiply(halver);

    testHalved(test);
}

TEST(AffineTransform, Scale)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.scale(1.0);

    testValueConstruction(test);

    test.scale(2.0);

    testDoubled(test);

    test.scale(0.5);

    testValueConstruction(test);

    test.scale(0.5);

    testHalved(test);
}

TEST(AffineTransform, ScaleUniformNonUniform)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(1.0, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(2.0, 2.0);

    testDoubled(test);

    test.scaleNonUniform(0.5, 0.5);

    testValueConstruction(test);

    test.scaleNonUniform(0.5, 0.5);

    testHalved(test);
}

TEST(AffineTransform, ScaleNonUniform)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(1.0, 2.0);

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(8.0, test.c());
    EXPECT_DOUBLE_EQ(6.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.scaleNonUniform(1.0, 0.5);

    testValueConstruction(test);

    test.scaleNonUniform(2.0, 1.0);

    EXPECT_DOUBLE_EQ(12.0, test.a());
    EXPECT_DOUBLE_EQ(10.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.scaleNonUniform(0.5, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(0.5, 2.0);

    EXPECT_DOUBLE_EQ(3.0, test.a());
    EXPECT_DOUBLE_EQ(2.5, test.b());
    EXPECT_DOUBLE_EQ(8.0, test.c());
    EXPECT_DOUBLE_EQ(6.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());
}

TEST(AffineTransform, ScaleFloatSize)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    WebCore::FloatSize first(1.0f, 2.0f);

    test.scale(first);

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(8.0, test.c());
    EXPECT_DOUBLE_EQ(6.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    WebCore::FloatSize second(1.0f, 0.5f);

    test.scale(second);

    testValueConstruction(test);

    WebCore::FloatSize third(2.0f, 1.0f);

    test.scale(third);

    EXPECT_DOUBLE_EQ(12.0, test.a());
    EXPECT_DOUBLE_EQ(10.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    WebCore::FloatSize fourth(0.5f, 1.0f);

    test.scale(fourth);

    testValueConstruction(test);

    WebCore::FloatSize fifth(0.5f, 2.0f);

    test.scale(fifth);

    EXPECT_DOUBLE_EQ(3.0, test.a());
    EXPECT_DOUBLE_EQ(2.5, test.b());
    EXPECT_DOUBLE_EQ(8.0, test.c());
    EXPECT_DOUBLE_EQ(6.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());
}

TEST(AffineTransform, Rotate)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test.rotate(360.0);

    testValueConstruction(test);

    test.rotate(180.0);

    static double epsilon = 0.0001;

    EXPECT_NEAR(-6.0, test.a(), epsilon);
    EXPECT_NEAR(-5.0, test.b(), epsilon);
    EXPECT_NEAR(-4.0, test.c(), epsilon);
    EXPECT_NEAR(-3.0, test.d(), epsilon);
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.rotate(-180.0);

    testValueConstruction(test);

    test.rotate(90.0);

    EXPECT_NEAR(4.0, test.a(), epsilon);
    EXPECT_NEAR(3.0, test.b(), epsilon);
    EXPECT_NEAR(-6.0, test.c(), epsilon);
    EXPECT_NEAR(-5.0, test.d(), epsilon);
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.rotate(-90.0);

    testValueConstruction(test);
}

TEST(AffineTransform, TranslateXY)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test.translate(0.0, 0.0);

    testValueConstruction(test);

    test.translate(5.0, 0.0);

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(32.0, test.e());
    EXPECT_DOUBLE_EQ(26.0, test.f());

    test.translate(0.0, -1.2);

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(27.2, test.e());
    EXPECT_DOUBLE_EQ(22.4, test.f());
}

TEST(AffineTransform, TranslateFloatPoint)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    WebCore::FloatPoint none;
    test.translate(none);

    testValueConstruction(test);

    WebCore::FloatPoint first(5.0f, 0.0f);

    test.translate(first);

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(32.0, test.e());
    EXPECT_DOUBLE_EQ(26.0, test.f());

    WebCore::FloatPoint second(0.0f, -1.2f);

    test.translate(second);

    static double epsilon = 0.0001;

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_NEAR(27.2, test.e(), epsilon);
    EXPECT_NEAR(22.4, test.f(), epsilon);

    WebCore::AffineTransform test2;

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.translate(second);

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());
}

TEST(AffineTransform, Shear)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test.shear(0.0, 0.0);

    testValueConstruction(test);

    test.shear(2.0, 2.0);

    EXPECT_DOUBLE_EQ(14.0, test.a());
    EXPECT_DOUBLE_EQ(11.0, test.b());
    EXPECT_DOUBLE_EQ(16.0, test.c());
    EXPECT_DOUBLE_EQ(13.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.shear(-1.0, 2.0);

    EXPECT_DOUBLE_EQ(46.0, test.a());
    EXPECT_DOUBLE_EQ(37.0, test.b());
    EXPECT_DOUBLE_EQ(2.0, test.c());
    EXPECT_DOUBLE_EQ(2.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());
}

TEST(AffineTransform, FlipX)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.flipX();

    EXPECT_DOUBLE_EQ(-6.0, test.a());
    EXPECT_DOUBLE_EQ(-5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipX();

    testValueConstruction(test);

    WebCore::AffineTransform test2;

    testIdentity(test2);

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipX();

    EXPECT_DOUBLE_EQ(-1.0, test2.a());
    EXPECT_DOUBLE_EQ(0.0, test2.b());
    EXPECT_DOUBLE_EQ(0.0, test2.c());
    EXPECT_DOUBLE_EQ(1.0, test2.d());
    EXPECT_DOUBLE_EQ(0.0, test2.e());
    EXPECT_DOUBLE_EQ(0.0, test2.f());

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());
    // 'Flipped' just means in the Y direction
    ASSERT_FALSE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipX();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());
}

TEST(AffineTransform, FlipY)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.flipY();

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(-4.0, test.c());
    EXPECT_DOUBLE_EQ(-3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipY();

    testValueConstruction(test);

    WebCore::AffineTransform test2;

    testIdentity(test2);

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipY();

    EXPECT_DOUBLE_EQ(1.0, test2.a());
    EXPECT_DOUBLE_EQ(0.0, test2.b());
    EXPECT_DOUBLE_EQ(0.0, test2.c());
    EXPECT_DOUBLE_EQ(-1.0, test2.d());
    EXPECT_DOUBLE_EQ(0.0, test2.e());
    EXPECT_DOUBLE_EQ(0.0, test2.f());

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipY();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());
}

TEST(AffineTransform, FlipXandFlipY)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.flipX();

    EXPECT_DOUBLE_EQ(-6.0, test.a());
    EXPECT_DOUBLE_EQ(-5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipY();

    EXPECT_DOUBLE_EQ(-6.0, test.a());
    EXPECT_DOUBLE_EQ(-5.0, test.b());
    EXPECT_DOUBLE_EQ(-4.0, test.c());
    EXPECT_DOUBLE_EQ(-3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipX();

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(-4.0, test.c());
    EXPECT_DOUBLE_EQ(-3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipY();

    testValueConstruction(test);

    WebCore::AffineTransform test2;

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipX();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());
    ASSERT_FALSE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipY();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());
    // False here because X is also flipped.
    ASSERT_FALSE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipX();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());

    test2.flipY();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
    ASSERT_TRUE(test2.isIdentityOrTranslationOrFlipped());
}

TEST(AffineTransform, Skew)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.skew(360.0, 360.0);

    testValueConstruction(test);

    test.skew(0.0, 0.0);

    testValueConstruction(test);

    test.skew(180.0, 180.0);

    static double epsilon = 0.0001;

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_NEAR(4.0, test.c(), epsilon);
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.skew(-180.0, -180.0);

    testValueConstruction(test);
}

TEST(AffineTransform, XandYScale)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    EXPECT_NEAR(7.8102, test.xScale(), 0.0001);
    EXPECT_NEAR(5.0, test.yScale(), 0.0001);
}

TEST(AffineTransform, Equality)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::AffineTransform test2;

    ASSERT_FALSE(test == test2);
    ASSERT_TRUE(test != test2);

    test.makeIdentity();

    ASSERT_TRUE(test == test2);
    ASSERT_FALSE(test != test2);
}

TEST(AffineTransform, Inverse)
{
    WebCore::AffineTransform test;

    auto inverse = test.inverse();

    ASSERT(inverse);

    EXPECT_DOUBLE_EQ(1.0, inverse->a());
    EXPECT_DOUBLE_EQ(0.0, inverse->b());
    EXPECT_DOUBLE_EQ(0.0, inverse->c());
    EXPECT_DOUBLE_EQ(1.0, inverse->d());
    EXPECT_DOUBLE_EQ(0.0, inverse->e());
    EXPECT_DOUBLE_EQ(0.0, inverse->f());

    auto test2 = test * inverse.value();

    testIdentity(test2);
}

TEST(AffineTransform, Blend)
{
    WebCore::AffineTransform test;

    WebCore::AffineTransform test2;
    test2.scale(2.0);

    test.blend(test2, 50);

    EXPECT_DOUBLE_EQ(-48.0, test.a());
    EXPECT_DOUBLE_EQ(0.0, test.b());
    EXPECT_DOUBLE_EQ(0.0, test.c());
    EXPECT_DOUBLE_EQ(-48.0, test.d());
    EXPECT_DOUBLE_EQ(0.0, test.e());
    EXPECT_DOUBLE_EQ(0.0, test.f());
}

TEST(AffineTransform, Translation)
{
    auto test = WebCore::AffineTransform::translation(-5.0, -7.0);
    EXPECT_DOUBLE_EQ(1.0, test.a());
    EXPECT_DOUBLE_EQ(0.0, test.b());
    EXPECT_DOUBLE_EQ(0.0, test.c());
    EXPECT_DOUBLE_EQ(1.0, test.d());
    EXPECT_DOUBLE_EQ(-5.0, test.e());
    EXPECT_DOUBLE_EQ(-7.0, test.f());
}

TEST(AffineTransform, ToTransformationMatrix)
{
    WebCore::AffineTransform transform;
    WebCore::TransformationMatrix matrix = transform.toTransformationMatrix();

    EXPECT_DOUBLE_EQ(1.0, matrix.m11());
    EXPECT_DOUBLE_EQ(0.0, matrix.m12());
    EXPECT_DOUBLE_EQ(0.0, matrix.m13());
    EXPECT_DOUBLE_EQ(0.0, matrix.m14());
    EXPECT_DOUBLE_EQ(0.0, matrix.m21());
    EXPECT_DOUBLE_EQ(1.0, matrix.m22());
    EXPECT_DOUBLE_EQ(0.0, matrix.m23());
    EXPECT_DOUBLE_EQ(0.0, matrix.m24());
    EXPECT_DOUBLE_EQ(0.0, matrix.m31());
    EXPECT_DOUBLE_EQ(0.0, matrix.m32());
    EXPECT_DOUBLE_EQ(1.0, matrix.m33());
    EXPECT_DOUBLE_EQ(0.0, matrix.m34());
}

TEST(AffineTransform, MakeMapBetweenRects)
{
    WebCore::AffineTransform transform;

    WebCore::FloatRect fromRect(10.0f, 10.0f, 100.0f, 100.0f);
    WebCore::FloatRect toRect(70.0f, 70.0f, 200.0f, 50.0f);

    auto mapBetween = WebCore::makeMapBetweenRects(fromRect, toRect);

    EXPECT_DOUBLE_EQ(2.0, mapBetween.a());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.b());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.c());
    EXPECT_DOUBLE_EQ(0.5, mapBetween.d());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.e());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.f());
}

#if USE(CG)
TEST(AffineTransform, CoreGraphicsCasting)
{
    WebCore::AffineTransform test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    CGAffineTransform test2 = CGAffineTransformMake(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    ASSERT_TRUE(CGAffineTransformEqualToTransform(test, test2));

    WebCore::AffineTransform test3;

    ASSERT_FALSE(CGAffineTransformEqualToTransform(test, test3));
}
#endif

#if PLATFORM(WIN)
TEST(AffineTransform, Direct2DCasting)
{
    WebCore::AffineTransform transform(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    D2D1_MATRIX_3X2_F test = transform;
    D2D1_MATRIX_3X2_F test2 = D2D1::Matrix3x2F(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    static const double epsilon = 0.0000001;

    EXPECT_NEAR(test._11, test2._11, epsilon);
    EXPECT_NEAR(test._12, test2._12, epsilon);
    EXPECT_NEAR(test._21, test2._21, epsilon);
    EXPECT_NEAR(test._22, test2._22, epsilon);
    EXPECT_NEAR(test._31, test2._31, epsilon);
    EXPECT_NEAR(test._32, test2._32, epsilon);
}
#endif

}
