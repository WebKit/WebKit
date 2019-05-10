/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/TransformationMatrix.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if USE(CA)
#include <QuartzCore/QuartzCore.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testIdentity(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(1.0, transform.m11());
    EXPECT_DOUBLE_EQ(0.0, transform.m12());
    EXPECT_DOUBLE_EQ(0.0, transform.m13());
    EXPECT_DOUBLE_EQ(0.0, transform.m14());
    EXPECT_DOUBLE_EQ(0.0, transform.m21());
    EXPECT_DOUBLE_EQ(1.0, transform.m22());
    EXPECT_DOUBLE_EQ(0.0, transform.m23());
    EXPECT_DOUBLE_EQ(0.0, transform.m24());
    EXPECT_DOUBLE_EQ(0.0, transform.m31());
    EXPECT_DOUBLE_EQ(0.0, transform.m32());
    EXPECT_DOUBLE_EQ(1.0, transform.m33());
    EXPECT_DOUBLE_EQ(0.0, transform.m34());
    EXPECT_DOUBLE_EQ(0.0, transform.m41());
    EXPECT_DOUBLE_EQ(0.0, transform.m42());
    EXPECT_DOUBLE_EQ(0.0, transform.m43());
    EXPECT_DOUBLE_EQ(1.0, transform.m44());
}

static void testGetAndSet(WebCore::TransformationMatrix& transform)
{
    transform.setA(1.1);
    EXPECT_DOUBLE_EQ(1.1, transform.a());
    transform.setB(2.2);
    EXPECT_DOUBLE_EQ(2.2, transform.b());
    transform.setC(3.3);
    EXPECT_DOUBLE_EQ(3.3, transform.c());
    transform.setD(4.4);
    EXPECT_DOUBLE_EQ(4.4, transform.d());
    transform.setE(5.5);
    EXPECT_DOUBLE_EQ(5.5, transform.e());
    transform.setF(6.6);
    EXPECT_DOUBLE_EQ(6.6, transform.f());

    transform.setM11(1.1);
    EXPECT_DOUBLE_EQ(1.1, transform.m11());
    transform.setM12(2.2);
    EXPECT_DOUBLE_EQ(2.2, transform.m12());
    transform.setM13(3.3);
    EXPECT_DOUBLE_EQ(3.3, transform.m13());
    transform.setM14(4.4);
    EXPECT_DOUBLE_EQ(4.4, transform.m14());
    transform.setM21(5.5);
    EXPECT_DOUBLE_EQ(5.5, transform.m21());
    transform.setM22(6.6);
    EXPECT_DOUBLE_EQ(6.6, transform.m22());
    transform.setM23(7.7);
    EXPECT_DOUBLE_EQ(7.7, transform.m23());
    transform.setM24(8.8);
    EXPECT_DOUBLE_EQ(8.8, transform.m24());
    transform.setM31(9.9);
    EXPECT_DOUBLE_EQ(9.9, transform.m31());
    transform.setM32(10.10);
    EXPECT_DOUBLE_EQ(10.10, transform.m32());
    transform.setM33(11.11);
    EXPECT_DOUBLE_EQ(11.11, transform.m33());
    transform.setM34(12.12);
    EXPECT_DOUBLE_EQ(12.12, transform.m34());
    transform.setM41(13.13);
    EXPECT_DOUBLE_EQ(13.13, transform.m41());
    transform.setM42(14.14);
    EXPECT_DOUBLE_EQ(14.14, transform.m42());
    transform.setM43(15.15);
    EXPECT_DOUBLE_EQ(15.15, transform.m43());
    transform.setM44(16.16);
    EXPECT_DOUBLE_EQ(16.16, transform.m44());
}

TEST(TransformationMatrix, DefaultConstruction)
{
    WebCore::TransformationMatrix test;

    testIdentity(test);

    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}

static void testAffineLikeConstruction(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(6.0, transform.a());
    EXPECT_DOUBLE_EQ(5.0, transform.b());
    EXPECT_DOUBLE_EQ(4.0, transform.c());
    EXPECT_DOUBLE_EQ(3.0, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

static void testValueConstruction(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(16.0, transform.m11());
    EXPECT_DOUBLE_EQ(15.0, transform.m12());
    EXPECT_DOUBLE_EQ(14.0, transform.m13());
    EXPECT_DOUBLE_EQ(13.0, transform.m14());
    EXPECT_DOUBLE_EQ(12.0, transform.m21());
    EXPECT_DOUBLE_EQ(11.0, transform.m22());
    EXPECT_DOUBLE_EQ(10.0, transform.m23());
    EXPECT_DOUBLE_EQ(9.0, transform.m24());
    EXPECT_DOUBLE_EQ(8.0, transform.m31());
    EXPECT_DOUBLE_EQ(7.0, transform.m32());
    EXPECT_DOUBLE_EQ(6.0, transform.m33());
    EXPECT_DOUBLE_EQ(5.0, transform.m34());
    EXPECT_DOUBLE_EQ(4.0, transform.m41());
    EXPECT_DOUBLE_EQ(3.0, transform.m42());
    EXPECT_DOUBLE_EQ(2.0, transform.m43());
    EXPECT_DOUBLE_EQ(1.0, transform.m44());
    ASSERT_FALSE(transform.isAffine());
}

TEST(TransformationMatrix, ValueConstruction)
{
    WebCore::TransformationMatrix test1(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test1);

    ASSERT_FALSE(test1.isIdentity());
    ASSERT_TRUE(test1.isAffine());

    WebCore::TransformationMatrix test2(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test2);

    testGetAndSet(test2);

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isAffine());

    test2.makeIdentity();

    testIdentity(test2);

    ASSERT_TRUE(test2.isIdentity());
}

#if USE(CG)
TEST(TransformationMatrix, CGAffineTransformConstruction)
{
    CGAffineTransform cgTransform = CGAffineTransformMake(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test(cgTransform);

    testAffineLikeConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}
#endif

#if PLATFORM(WIN)
TEST(TransformationMatrix, D2D1MatrixConstruction)
{
    D2D1_MATRIX_3X2_F d2dTransform = D2D1::Matrix3x2F(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test(d2dTransform);

    testAffineLikeConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}
#endif

TEST(TransformationMatrix, AffineTransformConstruction)
{
    WebCore::AffineTransform affineTransform(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test(affineTransform);

    testAffineLikeConstruction(test);
    testGetAndSet(test);

    ASSERT_FALSE(test.isIdentity());
}

TEST(TransformationMatrix, TransformMatrixConstruction)
{
    WebCore::TransformationMatrix matrix(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test(matrix);

    testValueConstruction(test);
}

TEST(TransformationMatrix, Assignment)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix matrix(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

    test = matrix;

    testValueConstruction(test);
}

TEST(TransformationMatrix, Identity)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    ASSERT_FALSE(test.isIdentity());
    ASSERT_FALSE(test.isIdentityOrTranslation());

    test.makeIdentity();

    ASSERT_TRUE(test.isIdentity());
    ASSERT_TRUE(test.isIdentityOrTranslation());

    testIdentity(test);
}

static void testAffineVersion(WebCore::TransformationMatrix affineTransformMatrix)
{
    ASSERT_FALSE(affineTransformMatrix.isIdentity());
    ASSERT_FALSE(affineTransformMatrix.isIdentityOrTranslation());
    ASSERT_TRUE(affineTransformMatrix.isAffine());

    EXPECT_DOUBLE_EQ(16.0, affineTransformMatrix.m11());
    EXPECT_DOUBLE_EQ(15.0, affineTransformMatrix.m12());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m13());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m14());
    EXPECT_DOUBLE_EQ(12.0, affineTransformMatrix.m21());
    EXPECT_DOUBLE_EQ(11.0, affineTransformMatrix.m22());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m23());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m24());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m31());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m32());
    EXPECT_DOUBLE_EQ(1.0, affineTransformMatrix.m33());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m34());
    EXPECT_DOUBLE_EQ(4.0, affineTransformMatrix.m41());
    EXPECT_DOUBLE_EQ(3.0, affineTransformMatrix.m42());
    EXPECT_DOUBLE_EQ(0.0, affineTransformMatrix.m43());
    EXPECT_DOUBLE_EQ(1.0, affineTransformMatrix.m44());
}

TEST(TransformationMatrix, AffineVersion)
{
    WebCore::TransformationMatrix test(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    ASSERT_FALSE(test.isIdentity());
    ASSERT_FALSE(test.isIdentityOrTranslation());
    ASSERT_FALSE(test.isAffine());

    auto affineCopy = test.toAffineTransform();

    testAffineVersion(affineCopy);

    test.makeAffine();

    testAffineVersion(test);
}

TEST(TransformationMatrix, MapFloatPoint)
{
    WebCore::TransformationMatrix test;
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

    test.setM22(0.5);

    auto mappedPoint4 = test.mapPoint(point);

    ASSERT_FLOAT_EQ(50.0f, mappedPoint4.x());
    ASSERT_FLOAT_EQ(25.0f, mappedPoint4.y());

    test.setM11(2.0);

    auto mappedPoint5 = test.mapPoint(point);

    ASSERT_FLOAT_EQ(200.0f, mappedPoint5.x());
    ASSERT_FLOAT_EQ(25.0f, mappedPoint5.y());
}

TEST(TransformationMatrix, MapIntPoint)
{
    WebCore::TransformationMatrix test;
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

    test.setM22(0.5);

    auto mappedPoint4 = test.mapPoint(point);

    ASSERT_EQ(50, mappedPoint4.x());
    ASSERT_EQ(25, mappedPoint4.y());

    test.setM11(2.0);

    auto mappedPoint5 = test.mapPoint(point);

    ASSERT_EQ(200, mappedPoint5.x());
    ASSERT_EQ(25, mappedPoint5.y());
}

TEST(TransformationMatrix, MapIntRect)
{
    WebCore::TransformationMatrix test;
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

    test.setM22(0.5);

    auto mappedRect4 = test.mapRect(rect);

    ASSERT_EQ(5, mappedRect4.x());
    ASSERT_EQ(10, mappedRect4.y());
    ASSERT_EQ(100, mappedRect4.width());
    ASSERT_EQ(150, mappedRect4.height());

    test.setM11(2.0);

    auto mappedRect5 = test.mapRect(rect);

    ASSERT_EQ(20, mappedRect5.x());
    ASSERT_EQ(10, mappedRect5.y());
    ASSERT_EQ(100, mappedRect4.width());
    ASSERT_EQ(150, mappedRect4.height());
}

TEST(TransformationMatrix, MapFloatRect)
{
    WebCore::TransformationMatrix test;
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

    test.setM22(0.5);

    auto mappedRect4 = test.mapRect(rect);

    ASSERT_FLOAT_EQ(5.0f, mappedRect4.x());
    ASSERT_FLOAT_EQ(10.0f, mappedRect4.y());
    ASSERT_FLOAT_EQ(100.0f, mappedRect4.width());
    ASSERT_FLOAT_EQ(150.0f, mappedRect4.height());

    test.setM11(2.0);

    auto mappedRect5 = test.mapRect(rect);

    ASSERT_FLOAT_EQ(20.0f, mappedRect5.x());
    ASSERT_FLOAT_EQ(10.0f, mappedRect5.y());
    ASSERT_FLOAT_EQ(100.0f, mappedRect4.width());
    ASSERT_FLOAT_EQ(150.0f, mappedRect4.height());
}

TEST(TransformationMatrix, MapFloatQuad)
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

    WebCore::TransformationMatrix test;
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

    test.setM22(0.5);

    auto mappedQuad4 = test.mapQuad(quad);

    ASSERT_FLOAT_EQ(50.0f, mappedQuad4.p1().x());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad4.p1().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad4.p2().x());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad4.p2().y());
    ASSERT_FLOAT_EQ(100.0f, mappedQuad4.p3().x());
    ASSERT_FLOAT_EQ(75.0f, mappedQuad4.p3().y());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad4.p4().x());
    ASSERT_FLOAT_EQ(75.0f, mappedQuad4.p4().y());

    test.setM11(2.0);

    auto mappedQuad5 = test.mapQuad(quad);

    ASSERT_FLOAT_EQ(200.0f, mappedQuad5.p1().x());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad5.p1().y());
    ASSERT_FLOAT_EQ(400.0f, mappedQuad5.p2().x());
    ASSERT_FLOAT_EQ(50.0f, mappedQuad5.p2().y());
    ASSERT_FLOAT_EQ(400.0f, mappedQuad5.p3().x());
    ASSERT_FLOAT_EQ(75.0f, mappedQuad5.p3().y());
    ASSERT_FLOAT_EQ(200.0f, mappedQuad5.p4().x());
    ASSERT_FLOAT_EQ(75.0f, mappedQuad5.p4().y());
}

static void testDoubled(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(12.0, transform.a());
    EXPECT_DOUBLE_EQ(10.0, transform.b());
    EXPECT_DOUBLE_EQ(8.0, transform.c());
    EXPECT_DOUBLE_EQ(6.0, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

static void testHalved(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(3.0, transform.a());
    EXPECT_DOUBLE_EQ(2.5, transform.b());
    EXPECT_DOUBLE_EQ(2.0, transform.c());
    EXPECT_DOUBLE_EQ(1.5, transform.d());
    EXPECT_DOUBLE_EQ(2.0, transform.e());
    EXPECT_DOUBLE_EQ(1.0, transform.f());
}

static void testDoubled2(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(32.0, transform.m11());
    EXPECT_DOUBLE_EQ(30.0, transform.m12());
    EXPECT_DOUBLE_EQ(28.0, transform.m13());
    EXPECT_DOUBLE_EQ(26.0, transform.m14());
    EXPECT_DOUBLE_EQ(24.0, transform.m21());
    EXPECT_DOUBLE_EQ(22.0, transform.m22());
    EXPECT_DOUBLE_EQ(20.0, transform.m23());
    EXPECT_DOUBLE_EQ(18.0, transform.m24());
    EXPECT_DOUBLE_EQ(16.0, transform.m31());
    EXPECT_DOUBLE_EQ(14.0, transform.m32());
    EXPECT_DOUBLE_EQ(12.0, transform.m33());
    EXPECT_DOUBLE_EQ(10.0, transform.m34());
    EXPECT_DOUBLE_EQ(8.0, transform.m41());
    EXPECT_DOUBLE_EQ(6.0, transform.m42());
    EXPECT_DOUBLE_EQ(4.0, transform.m43());
    EXPECT_DOUBLE_EQ(2.0, transform.m44());
}

static void testHalved2(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(8.0, transform.m11());
    EXPECT_DOUBLE_EQ(7.5, transform.m12());
    EXPECT_DOUBLE_EQ(7.0, transform.m13());
    EXPECT_DOUBLE_EQ(6.5, transform.m14());
    EXPECT_DOUBLE_EQ(6.0, transform.m21());
    EXPECT_DOUBLE_EQ(5.5, transform.m22());
    EXPECT_DOUBLE_EQ(5.0, transform.m23());
    EXPECT_DOUBLE_EQ(4.5, transform.m24());
    EXPECT_DOUBLE_EQ(4.0, transform.m31());
    EXPECT_DOUBLE_EQ(3.5, transform.m32());
    EXPECT_DOUBLE_EQ(3.0, transform.m33());
    EXPECT_DOUBLE_EQ(2.5, transform.m34());
    EXPECT_DOUBLE_EQ(2.0, transform.m41());
    EXPECT_DOUBLE_EQ(1.5, transform.m42());
    EXPECT_DOUBLE_EQ(1.0, transform.m43());
    EXPECT_DOUBLE_EQ(0.5, transform.m44());
}

TEST(TransformationMatrix, Multiply)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix identity;

    testAffineLikeConstruction(test);

    test.multiply(identity);

    testAffineLikeConstruction(test);

    WebCore::TransformationMatrix doubler(2.0, 0.0, 0.0, 2.0, 0.0, 0.0);

    test.multiply(doubler);

    testDoubled(test);

    WebCore::TransformationMatrix halver(0.5, 0.0, 0.0, 0.5, 0.0, 0.0);

    test.multiply(halver);

    testAffineLikeConstruction(test);

    test.multiply(halver);

    testHalved(test);

    WebCore::TransformationMatrix test2(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test2);

    test2.multiply(identity);

    testValueConstruction(test2);

    WebCore::TransformationMatrix doubler2(2.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0,
        0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 2.0);

    test2.multiply(doubler2);

    testDoubled2(test2);

    WebCore::TransformationMatrix halver2(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5);

    test2.multiply(halver2);

    testValueConstruction(test2);

    test2.multiply(halver2);

    testHalved2(test2);

    WebCore::TransformationMatrix test3(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test3 *= identity;

    testAffineLikeConstruction(test3);

    test3 *= doubler;

    testDoubled(test3);

    const WebCore::TransformationMatrix test4(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    auto result1 = test4 * identity;

    testValueConstruction(result1);

    auto result2 = test4 * doubler2;

    testDoubled2(result2);
}

static void testScaledByTwo(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(32.0, transform.m11());
    EXPECT_DOUBLE_EQ(30.0, transform.m12());
    EXPECT_DOUBLE_EQ(28.0, transform.m13());
    EXPECT_DOUBLE_EQ(26.0, transform.m14());
    EXPECT_DOUBLE_EQ(24.0, transform.m21());
    EXPECT_DOUBLE_EQ(22.0, transform.m22());
    EXPECT_DOUBLE_EQ(20.0, transform.m23());
    EXPECT_DOUBLE_EQ(18.0, transform.m24());
    EXPECT_DOUBLE_EQ(8.0, transform.m31());
    EXPECT_DOUBLE_EQ(7.0, transform.m32());
    EXPECT_DOUBLE_EQ(6.0, transform.m33());
    EXPECT_DOUBLE_EQ(5.0, transform.m34());
    EXPECT_DOUBLE_EQ(4.0, transform.m41());
    EXPECT_DOUBLE_EQ(3.0, transform.m42());
    EXPECT_DOUBLE_EQ(2.0, transform.m43());
    EXPECT_DOUBLE_EQ(1.0, transform.m44());
}

static void testScaledByHalf(const WebCore::TransformationMatrix& transform)
{
    EXPECT_DOUBLE_EQ(8.0, transform.m11());
    EXPECT_DOUBLE_EQ(7.5, transform.m12());
    EXPECT_DOUBLE_EQ(7.0, transform.m13());
    EXPECT_DOUBLE_EQ(6.5, transform.m14());
    EXPECT_DOUBLE_EQ(6.0, transform.m21());
    EXPECT_DOUBLE_EQ(5.5, transform.m22());
    EXPECT_DOUBLE_EQ(5.0, transform.m23());
    EXPECT_DOUBLE_EQ(4.5, transform.m24());
    EXPECT_DOUBLE_EQ(8.0, transform.m31());
    EXPECT_DOUBLE_EQ(7.0, transform.m32());
    EXPECT_DOUBLE_EQ(6.0, transform.m33());
    EXPECT_DOUBLE_EQ(5.0, transform.m34());
    EXPECT_DOUBLE_EQ(4.0, transform.m41());
    EXPECT_DOUBLE_EQ(3.0, transform.m42());
    EXPECT_DOUBLE_EQ(2.0, transform.m43());
    EXPECT_DOUBLE_EQ(1.0, transform.m44());
}


TEST(TransformationMatrix, Scale)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

    test.scale(1.0);

    testAffineLikeConstruction(test);

    test.scale(2.0);

    testDoubled(test);

    test.scale(0.5);

    testAffineLikeConstruction(test);

    test.scale(0.5);

    testHalved(test);

    WebCore::TransformationMatrix test2(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test2);

    test2.scale(2.0);

    testScaledByTwo(test2);

    test2.scale(0.5);

    testValueConstruction(test2);

    test2.scale(0.5);

    testScaledByHalf(test2);
}

TEST(TransformationMatrix, ScaleUniformNonUniform)
{
    WebCore::TransformationMatrix test(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(1.0, 1.0);

    testValueConstruction(test);

    test.scaleNonUniform(2.0, 2.0);

    testScaledByTwo(test);

    test.scaleNonUniform(0.5, 0.5);

    testValueConstruction(test);

    test.scaleNonUniform(0.5, 0.5);

    testScaledByHalf(test);
}

TEST(TransformationMatrix, Rotate)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test.rotate(360.0);

    testAffineLikeConstruction(test);

    test.rotate(180.0);

    static double epsilon = 0.0001;

    EXPECT_NEAR(-6.0, test.a(), epsilon);
    EXPECT_NEAR(-5.0, test.b(), epsilon);
    EXPECT_NEAR(-4.0, test.c(), epsilon);
    EXPECT_NEAR(-3.0, test.d(), epsilon);
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.rotate(-180.0);

    testAffineLikeConstruction(test);

    test.rotate(90.0);

    EXPECT_NEAR(4.0, test.a(), epsilon);
    EXPECT_NEAR(3.0, test.b(), epsilon);
    EXPECT_NEAR(-6.0, test.c(), epsilon);
    EXPECT_NEAR(-5.0, test.d(), epsilon);
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.rotate(-90.0);

    testAffineLikeConstruction(test);
}

TEST(TransformationMatrix, TranslateXY)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    test.translate(0.0, 0.0);

    testAffineLikeConstruction(test);

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

TEST(TransformationMatrix, FlipX)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

    test.flipX();

    EXPECT_DOUBLE_EQ(-6.0, test.a());
    EXPECT_DOUBLE_EQ(-5.0, test.b());
    EXPECT_DOUBLE_EQ(4.0, test.c());
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipX();

    testAffineLikeConstruction(test);

    WebCore::TransformationMatrix test2;

    testIdentity(test2);

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());

    test2.flipX();

    EXPECT_DOUBLE_EQ(-1.0, test2.a());
    EXPECT_DOUBLE_EQ(0.0, test2.b());
    EXPECT_DOUBLE_EQ(0.0, test2.c());
    EXPECT_DOUBLE_EQ(1.0, test2.d());
    EXPECT_DOUBLE_EQ(0.0, test2.e());
    EXPECT_DOUBLE_EQ(0.0, test2.f());

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());

    test2.flipX();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
}

TEST(TransformationMatrix, FlipY)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

    test.flipY();

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_DOUBLE_EQ(-4.0, test.c());
    EXPECT_DOUBLE_EQ(-3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.flipY();

    testAffineLikeConstruction(test);

    WebCore::TransformationMatrix test2;

    testIdentity(test2);

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());

    test2.flipY();

    EXPECT_DOUBLE_EQ(1.0, test2.a());
    EXPECT_DOUBLE_EQ(0.0, test2.b());
    EXPECT_DOUBLE_EQ(0.0, test2.c());
    EXPECT_DOUBLE_EQ(-1.0, test2.d());
    EXPECT_DOUBLE_EQ(0.0, test2.e());
    EXPECT_DOUBLE_EQ(0.0, test2.f());

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());

    test2.flipY();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
}

TEST(TransformationMatrix, FlipXandFlipY)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

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

    testAffineLikeConstruction(test);

    WebCore::TransformationMatrix test2;

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());

    test2.flipX();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());

    test2.flipY();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());

    test2.flipX();

    ASSERT_FALSE(test2.isIdentity());
    ASSERT_FALSE(test2.isIdentityOrTranslation());

    test2.flipY();

    ASSERT_TRUE(test2.isIdentity());
    ASSERT_TRUE(test2.isIdentityOrTranslation());
}

TEST(TransformationMatrix, Skew)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

    testAffineLikeConstruction(test);

    test.skew(360.0, 360.0);

    testAffineLikeConstruction(test);

    test.skew(0.0, 0.0);

    testAffineLikeConstruction(test);

    test.skew(180.0, 180.0);

    static double epsilon = 0.0001;

    EXPECT_DOUBLE_EQ(6.0, test.a());
    EXPECT_DOUBLE_EQ(5.0, test.b());
    EXPECT_NEAR(4.0, test.c(), epsilon);
    EXPECT_DOUBLE_EQ(3.0, test.d());
    EXPECT_DOUBLE_EQ(2.0, test.e());
    EXPECT_DOUBLE_EQ(1.0, test.f());

    test.skew(-180.0, -180.0);

    testAffineLikeConstruction(test);
}

TEST(TransformationMatrix, Inverse)
{
    WebCore::TransformationMatrix test;

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

TEST(TransformationMatrix, NonInvertableBlend)
{
    WebCore::TransformationMatrix from;
    WebCore::TransformationMatrix to(2.7133590938, 0.0, 0.0, 0.0, 0.0, 2.4645137761, 0.0, 0.0, 0.0, 0.0, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05);
    WebCore::TransformationMatrix result;
    
    result = to;
    result.blend(from, 0.25);
    EXPECT_TRUE(result == from);
    
    result = to;
    result.blend(from, 0.75);
    EXPECT_TRUE(result == to);
}

TEST(TransformationMatrix, Blend)
{
    WebCore::TransformationMatrix transform;

    WebCore::TransformationMatrix scaled;
    scaled.scale(2.0);

    transform.blend(scaled, 50);

    static const double epsilon = 0.0001;
    EXPECT_NEAR(-48.0, transform.m11(), epsilon);
    EXPECT_NEAR(0.0, transform.m12(), epsilon);
    EXPECT_NEAR(0.0, transform.m13(), epsilon);
    EXPECT_NEAR(0.0, transform.m14(), epsilon);
    EXPECT_NEAR(0.0, transform.m21(), epsilon);
    EXPECT_NEAR(-48.0, transform.m22(), epsilon);
    EXPECT_NEAR(0.0, transform.m23(), epsilon);
    EXPECT_NEAR(0.0, transform.m24(), epsilon);
    EXPECT_NEAR(0.0, transform.m31(), epsilon);
    EXPECT_NEAR(0.0, transform.m32(), epsilon);
    EXPECT_NEAR(1.0, transform.m33(), epsilon);
    EXPECT_NEAR(0.0, transform.m34(), epsilon);
    EXPECT_NEAR(0.0, transform.m41(), epsilon);
    EXPECT_NEAR(0.0, transform.m42(), epsilon);
    EXPECT_NEAR(0.0, transform.m43(), epsilon);
    EXPECT_NEAR(1.0, transform.m44(), epsilon);
}

TEST(TransformationMatrix, Blend2)
{
    WebCore::TransformationMatrix transform;

    WebCore::TransformationMatrix scaled;
    scaled.scale(2.0);

    transform.blend2(scaled, 20);

    static const double epsilon = 0.0001;
    EXPECT_NEAR(-18.0, transform.m11(), epsilon);
    EXPECT_NEAR(0.0, transform.m12(), epsilon);
    EXPECT_NEAR(0.0, transform.m13(), epsilon);
    EXPECT_NEAR(0.0, transform.m14(), epsilon);
    EXPECT_NEAR(0.0, transform.m21(), epsilon);
    EXPECT_NEAR(-18.0, transform.m22(), epsilon);
    EXPECT_NEAR(0.0, transform.m23(), epsilon);
    EXPECT_NEAR(0.0, transform.m24(), epsilon);
    EXPECT_NEAR(0.0, transform.m31(), epsilon);
    EXPECT_NEAR(0.0, transform.m32(), epsilon);
    EXPECT_NEAR(1.0, transform.m33(), epsilon);
    EXPECT_NEAR(0.0, transform.m34(), epsilon);
    EXPECT_NEAR(0.0, transform.m41(), epsilon);
    EXPECT_NEAR(0.0, transform.m42(), epsilon);
    EXPECT_NEAR(0.0, transform.m43(), epsilon);
    EXPECT_NEAR(1.0, transform.m44(), epsilon);
}

TEST(TransformationMatrix, Blend4)
{
    WebCore::TransformationMatrix transform;

    WebCore::TransformationMatrix scaled;
    scaled.scale(2.0);

    transform.blend4(scaled, 30);

    static const double epsilon = 0.0001;
    EXPECT_NEAR(-28.0, transform.m11(), epsilon);
    EXPECT_NEAR(0.0, transform.m12(), epsilon);
    EXPECT_NEAR(0.0, transform.m13(), epsilon);
    EXPECT_NEAR(0.0, transform.m14(), epsilon);
    EXPECT_NEAR(0.0, transform.m21(), epsilon);
    EXPECT_NEAR(-28.0, transform.m22(), epsilon);
    EXPECT_NEAR(0.0, transform.m23(), epsilon);
    EXPECT_NEAR(0.0, transform.m24(), epsilon);
    EXPECT_NEAR(0.0, transform.m31(), epsilon);
    EXPECT_NEAR(0.0, transform.m32(), epsilon);
    EXPECT_NEAR(1.0, transform.m33(), epsilon);
    EXPECT_NEAR(0.0, transform.m34(), epsilon);
    EXPECT_NEAR(0.0, transform.m41(), epsilon);
    EXPECT_NEAR(0.0, transform.m42(), epsilon);
    EXPECT_NEAR(0.0, transform.m43(), epsilon);
    EXPECT_NEAR(1.0, transform.m44(), epsilon);
}

TEST(TransformationMatrix, Equality)
{
    WebCore::TransformationMatrix test(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test2;

    ASSERT_FALSE(test == test2);
    ASSERT_TRUE(test != test2);

    test.makeIdentity();

    ASSERT_TRUE(test == test2);
    ASSERT_FALSE(test != test2);

    WebCore::TransformationMatrix test3(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix test4(test3);

    ASSERT_TRUE(test3 == test4);
    ASSERT_FALSE(test3 != test4);

    test4.setM43(1002.22);

    ASSERT_FALSE(test3 == test4);
    ASSERT_TRUE(test3 != test4);
}

#if USE(CA)
static void testTranslationMatrix(const WebCore::TransformationMatrix& matrix)
{
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
    EXPECT_DOUBLE_EQ(10.0, matrix.m41());
    EXPECT_DOUBLE_EQ(15.0, matrix.m42());
    EXPECT_DOUBLE_EQ(30.0, matrix.m43());
    EXPECT_DOUBLE_EQ(1.0, matrix.m44());
}
#endif

TEST(TransformationMatrix, Casting)
{
    WebCore::TransformationMatrix test(16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0,
        9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0);

#if USE(CA)
    CATransform3D caTransform = CATransform3DMakeTranslation(10.0f, 15.0f, 30.0f);

    WebCore::TransformationMatrix fromCATransform(caTransform);

    testTranslationMatrix(fromCATransform);

    CATransform3D caFromWK = test;

    EXPECT_DOUBLE_EQ(16.0, caFromWK.m11);
    EXPECT_DOUBLE_EQ(15.0, caFromWK.m12);
    EXPECT_DOUBLE_EQ(14.0, caFromWK.m13);
    EXPECT_DOUBLE_EQ(13.0, caFromWK.m14);
    EXPECT_DOUBLE_EQ(12.0, caFromWK.m21);
    EXPECT_DOUBLE_EQ(11.0, caFromWK.m22);
    EXPECT_DOUBLE_EQ(10.0, caFromWK.m23);
    EXPECT_DOUBLE_EQ(9.0, caFromWK.m24);
    EXPECT_DOUBLE_EQ(8.0, caFromWK.m31);
    EXPECT_DOUBLE_EQ(7.0, caFromWK.m32);
    EXPECT_DOUBLE_EQ(6.0, caFromWK.m33);
    EXPECT_DOUBLE_EQ(5.0, caFromWK.m34);
    EXPECT_DOUBLE_EQ(4.0, caFromWK.m41);
    EXPECT_DOUBLE_EQ(3.0, caFromWK.m42);
    EXPECT_DOUBLE_EQ(2.0, caFromWK.m43);
    EXPECT_DOUBLE_EQ(1.0, caFromWK.m44);
#endif

#if USE(CG)
    CGAffineTransform cgTransform = CGAffineTransformMake(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix fromCGTransform(cgTransform);

    testAffineLikeConstruction(fromCGTransform);

    CGAffineTransform cgFromWK = test;
    EXPECT_DOUBLE_EQ(16.0, cgFromWK.a);
    EXPECT_DOUBLE_EQ(15.0, cgFromWK.b);
    EXPECT_DOUBLE_EQ(12.0, cgFromWK.c);
    EXPECT_DOUBLE_EQ(11.0, cgFromWK.d);
    EXPECT_DOUBLE_EQ(4.0, cgFromWK.tx);
    EXPECT_DOUBLE_EQ(3.0, cgFromWK.ty);
#endif

#if PLATFORM(WIN) || (PLATFORM(GTK) && OS(WINDOWS))
    XFORM gdiFromWK = test;
    EXPECT_DOUBLE_EQ(16.0, gdiFromWK.eM11);
    EXPECT_DOUBLE_EQ(15.0, gdiFromWK.eM12);
    EXPECT_DOUBLE_EQ(12.0, gdiFromWK.eM21);
    EXPECT_DOUBLE_EQ(11.0, gdiFromWK.eM22);
    EXPECT_DOUBLE_EQ(4.0, gdiFromWK.eDx);
    EXPECT_DOUBLE_EQ(3.0, gdiFromWK.eDy);
#endif

#if PLATFORM(WIN)
    D2D1_MATRIX_3X2_F d2dTransform = D2D1::Matrix3x2F(6.0, 5.0, 4.0, 3.0, 2.0, 1.0);
    WebCore::TransformationMatrix fromD2DTransform(d2dTransform);

    testAffineLikeConstruction(fromD2DTransform);

    D2D1_MATRIX_3X2_F d2dFromWK = test;
    EXPECT_DOUBLE_EQ(16.0, d2dFromWK._11);
    EXPECT_DOUBLE_EQ(15.0, d2dFromWK._12);
    EXPECT_DOUBLE_EQ(12.0, d2dFromWK._21);
    EXPECT_DOUBLE_EQ(11.0, d2dFromWK._22);
    EXPECT_DOUBLE_EQ(4.0, d2dFromWK._31);
    EXPECT_DOUBLE_EQ(3.0, d2dFromWK._32);
#endif
}

TEST(TransformationMatrix, MakeMapBetweenRects)
{
    WebCore::FloatRect fromRect(10.0f, 10.0f, 100.0f, 100.0f);
    WebCore::FloatRect toRect(70.0f, 70.0f, 200.0f, 50.0f);

    auto mapBetween = WebCore::TransformationMatrix::rectToRect(fromRect, toRect);

    EXPECT_DOUBLE_EQ(2.0, mapBetween.a());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.b());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.c());
    EXPECT_DOUBLE_EQ(0.5, mapBetween.d());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.e());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.f());

    EXPECT_DOUBLE_EQ(2.0, mapBetween.m11());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m12());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m13());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m14());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m21());
    EXPECT_DOUBLE_EQ(0.5, mapBetween.m22());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m23());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m24());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m31());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m32());
    EXPECT_DOUBLE_EQ(1.0, mapBetween.m33());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m34());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.m41());
    EXPECT_DOUBLE_EQ(60.0, mapBetween.m42());
    EXPECT_DOUBLE_EQ(0.0, mapBetween.m43());
    EXPECT_DOUBLE_EQ(1.0, mapBetween.m44());
}

}
