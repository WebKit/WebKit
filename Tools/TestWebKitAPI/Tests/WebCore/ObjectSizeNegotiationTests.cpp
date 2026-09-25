/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/FloatSize.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/NaturalDimensions.h>
#include <WebCore/ObjectSizeNegotiation.h>
#include <limits>

namespace TestWebKitAPI {

using namespace WebCore;
using namespace WebCore::ObjectSizeNegotiation;

static constexpr FloatSize defaultObjectSize { 300, 150 };

static Inputs inputs(float density = 1)
{
    return { defaultObjectSize, density };
}

static NaturalDimensions ratioOnly(FloatSize ratio)
{
    return { std::nullopt, std::nullopt, ratio };
}

static NaturalDimensions widthOnly(float width)
{
    return { width, std::nullopt, std::nullopt };
}

static NaturalDimensions heightOnly(float height)
{
    return { std::nullopt, height, std::nullopt };
}

TEST(ObjectSizeNegotiation, DefiniteWidthAndHeight)
{
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::fixed(7, 9), { 100, 40 }, inputs()).size());
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::none(), { 100, 40 }, inputs()).size());
}

TEST(ObjectSizeNegotiation, DefiniteWidthOnly)
{
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(ratioOnly({ 2, 1 }), { 100, std::nullopt }, inputs()).size());

    EXPECT_EQ(FloatSize(100, 30), defaultSizingAlgorithm(heightOnly(30), { 100, std::nullopt }, inputs()).size());

    EXPECT_EQ(FloatSize(100, 150), defaultSizingAlgorithm(NaturalDimensions::none(), { 100, std::nullopt }, inputs()).size());
}

TEST(ObjectSizeNegotiation, DefiniteHeightOnly)
{
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(ratioOnly({ 2, 1 }), { std::nullopt, 50 }, inputs()).size());

    EXPECT_EQ(FloatSize(40, 50), defaultSizingAlgorithm(widthOnly(40), { std::nullopt, 50 }, inputs()).size());

    EXPECT_EQ(FloatSize(300, 50), defaultSizingAlgorithm(NaturalDimensions::none(), { std::nullopt, 50 }, inputs()).size());
}

TEST(ObjectSizeNegotiation, NoSpecifiedSize)
{
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm({ 200, std::nullopt, FloatSize { 2, 1 } }, SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(FloatSize(200, 150), defaultSizingAlgorithm(widthOnly(200), SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(FloatSize(300, 40), defaultSizingAlgorithm(heightOnly(40), SpecifiedSize::none(), inputs()).size());

    EXPECT_EQ(FloatSize(150, 150), defaultSizingAlgorithm(ratioOnly({ 1, 1 }), SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(FloatSize(300, 150), defaultSizingAlgorithm(NaturalDimensions::none(), SpecifiedSize::none(), inputs()).size());
}

TEST(ObjectSizeNegotiation, ContainConstraint)
{
    EXPECT_EQ(FloatSize(150, 150), resolveContainConstraint(ratioOnly({ 1, 1 }), defaultObjectSize, inputs()).size());
    EXPECT_EQ(FloatSize(300, 150), resolveContainConstraint(ratioOnly({ 2, 1 }), defaultObjectSize, inputs()).size());
    EXPECT_EQ(FloatSize(75, 150), resolveContainConstraint(ratioOnly({ 1, 2 }), defaultObjectSize, inputs()).size());

    EXPECT_EQ(defaultObjectSize, resolveContainConstraint(NaturalDimensions::none(), defaultObjectSize, inputs()).size());
}

TEST(ObjectSizeNegotiation, CoverConstraint)
{
    EXPECT_EQ(FloatSize(300, 300), resolveCoverConstraint(ratioOnly({ 1, 1 }), defaultObjectSize, inputs()).size());
    EXPECT_EQ(FloatSize(300, 150), resolveCoverConstraint(ratioOnly({ 2, 1 }), defaultObjectSize, inputs()).size());
    EXPECT_EQ(FloatSize(600, 300), resolveCoverConstraint(ratioOnly({ 2, 1 }), FloatSize { 600, 150 }, inputs()).size());

    EXPECT_EQ(defaultObjectSize, resolveCoverConstraint(NaturalDimensions::none(), defaultObjectSize, inputs()).size());
}

TEST(ObjectSizeNegotiation, UsableAspectRatio)
{
    constexpr auto infinity = std::numeric_limits<float>::infinity();
    constexpr auto nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_TRUE(ratioOnly({ 2, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(NaturalDimensions::none().hasUsableAspectRatio());

    EXPECT_FALSE(ratioOnly({ 0, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(ratioOnly({ 1, 0 }).hasUsableAspectRatio());
    EXPECT_FALSE(ratioOnly({ infinity, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(ratioOnly({ 1, infinity }).hasUsableAspectRatio());
    EXPECT_FALSE(ratioOnly({ nan, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(ratioOnly({ 1, nan }).hasUsableAspectRatio());

    EXPECT_EQ(FloatSize(100, 150), defaultSizingAlgorithm(ratioOnly({ 0, 1 }), { 100, std::nullopt }, inputs()).size());

    EXPECT_EQ(FloatSize(0, 0), defaultSizingAlgorithm(NaturalDimensions::fixed(0, 0), SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(defaultObjectSize, defaultSizingAlgorithm(NaturalDimensions::none(), SpecifiedSize::none(), inputs()).size());
}

TEST(ObjectSizeNegotiation, ZoomIsCarriedOnlyByAStatedSize)
{
    EXPECT_EQ(1, defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), SpecifiedSize::none(), inputs()).zoom());

    auto stated = ConcreteObjectSize::fixed(FloatSize { 400, 200 }, 2);
    EXPECT_EQ(FloatSize(400, 200), stated.size());
    EXPECT_EQ(2, stated.zoom());
}

TEST(ObjectSizeNegotiation, Density)
{
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), SpecifiedSize::none(), inputs(0.5)).size());

    EXPECT_EQ(FloatSize(150, 150), resolveContainConstraint(ratioOnly({ 1, 1 }), defaultObjectSize, inputs(0.5)).size());
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), { 100, 40 }, inputs(0.5)).size());
}

TEST(ObjectSizeNegotiation, OrientedNaturalDimensions)
{
    constexpr auto rotated = ImageOrientation::Orientation::OriginRightTop;
    EXPECT_EQ(FloatSize(100, 200), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).oriented(rotated), SpecifiedSize::none(), inputs()).size());
    EXPECT_EQ(FloatSize(100, 200), defaultSizingAlgorithm(ratioOnly({ 2, 1 }).oriented(rotated), { 100, std::nullopt }, inputs()).size());
    EXPECT_EQ(FloatSize(75, 150), resolveContainConstraint(ratioOnly({ 2, 1 }).oriented(rotated), defaultObjectSize, inputs()).size());

    constexpr auto mirrored = ImageOrientation::Orientation::OriginTopRight;
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).oriented(mirrored), SpecifiedSize::none(), inputs()).size());
}

} // namespace TestWebKitAPI
