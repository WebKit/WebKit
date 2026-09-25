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

#include <WebCore/BitmapImage.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/NaturalDimensions.h>
#include <WebCore/ObjectSizeNegotiation.h>
#include <limits>

namespace TestWebKitAPI {

using namespace WebCore;
using namespace WebCore::ObjectSizeNegotiation;

static NaturalDimensions aspectRatioOnly(FloatSize ratio)
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
    // If the specified size is a definite width and height, the concrete object size is
    // given that width and height, whatever the object's natural dimensions are.
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::fixed(7, 9), { 100, 40 }, defaultObjectSize).size());
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::none(), { 100, 40 }, defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, DefiniteWidthOnly)
{
    // The missing height comes from the natural aspect ratio, ...
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(aspectRatioOnly({ 2, 1 }), { 100, std::nullopt }, defaultObjectSize).size());

    // ... otherwise from the natural height, ...
    EXPECT_EQ(FloatSize(100, 30), defaultSizingAlgorithm(heightOnly(30), { 100, std::nullopt }, defaultObjectSize).size());

    // ... otherwise from the default object size.
    EXPECT_EQ(FloatSize(100, 150), defaultSizingAlgorithm(NaturalDimensions::none(), { 100, std::nullopt }, defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, DefiniteHeightOnly)
{
    // The missing width comes from the natural aspect ratio, ...
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(aspectRatioOnly({ 2, 1 }), { std::nullopt, 50 }, defaultObjectSize).size());

    // ... otherwise from the natural width, ...
    EXPECT_EQ(FloatSize(40, 50), defaultSizingAlgorithm(widthOnly(40), { std::nullopt, 50 }, defaultObjectSize).size());

    // ... otherwise from the default object size.
    EXPECT_EQ(FloatSize(300, 50), defaultSizingAlgorithm(NaturalDimensions::none(), { std::nullopt, 50 }, defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, NoSpecifiedSize)
{
    // With a natural width or height, the size is resolved as if the natural dimensions
    // had been given as the specified size.
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm({ 200, std::nullopt, FloatSize { 2, 1 } }, SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(200, 150), defaultSizingAlgorithm(widthOnly(200), SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(300, 40), defaultSizingAlgorithm(heightOnly(40), SpecifiedSize::none(), defaultObjectSize).size());

    // With neither, the size is resolved as a contain constraint against the default
    // object size.
    EXPECT_EQ(FloatSize(150, 150), defaultSizingAlgorithm(aspectRatioOnly({ 1, 1 }), SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(300, 150), defaultSizingAlgorithm(NaturalDimensions::none(), SpecifiedSize::none(), defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, ContainConstraint)
{
    // The largest rectangle with the natural aspect ratio that fits inside the
    // constraint rectangle.
    EXPECT_EQ(FloatSize(150, 150), resolveContainConstraint(aspectRatioOnly({ 1, 1 }), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(300, 150), resolveContainConstraint(aspectRatioOnly({ 2, 1 }), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(75, 150), resolveContainConstraint(aspectRatioOnly({ 1, 2 }), defaultObjectSize).size());

    // Without a natural aspect ratio, the concrete object size is the constraint rectangle.
    EXPECT_EQ(defaultObjectSize, resolveContainConstraint(NaturalDimensions::none(), defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, CoverConstraint)
{
    // The smallest rectangle with the natural aspect ratio that covers the constraint
    // rectangle.
    EXPECT_EQ(FloatSize(300, 300), resolveCoverConstraint(aspectRatioOnly({ 1, 1 }), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(300, 150), resolveCoverConstraint(aspectRatioOnly({ 2, 1 }), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(600, 300), resolveCoverConstraint(aspectRatioOnly({ 2, 1 }), FloatSize { 600, 150 }).size());

    // Without a natural aspect ratio, the concrete object size is the constraint rectangle.
    EXPECT_EQ(defaultObjectSize, resolveCoverConstraint(NaturalDimensions::none(), defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, UsableAspectRatio)
{
    constexpr auto infinity = std::numeric_limits<float>::infinity();
    constexpr auto nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_TRUE(aspectRatioOnly({ 2, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(NaturalDimensions::none().hasUsableAspectRatio());

    // Degenerate: "at least one part being zero or infinity". NaN is degenerate too, and is
    // the case a test against zero alone lets through.
    EXPECT_FALSE(aspectRatioOnly({ 0, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(aspectRatioOnly({ 1, 0 }).hasUsableAspectRatio());
    EXPECT_FALSE(aspectRatioOnly({ infinity, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(aspectRatioOnly({ 1, infinity }).hasUsableAspectRatio());
    EXPECT_FALSE(aspectRatioOnly({ nan, 1 }).hasUsableAspectRatio());
    EXPECT_FALSE(aspectRatioOnly({ 1, nan }).hasUsableAspectRatio());

    // Treated as having no ratio, so a lone specified width takes its height from the
    // default object size rather than deriving one.
    EXPECT_EQ(FloatSize(100, 150), defaultSizingAlgorithm(aspectRatioOnly({ 0, 1 }), { 100, std::nullopt }, defaultObjectSize).size());

    // The dimensions a degenerate ratio was built from are still natural dimensions: an
    // unusable ratio is not the same answer as an absent width and height.
    EXPECT_EQ(FloatSize(0, 0), defaultSizingAlgorithm(NaturalDimensions::fixed(0, 0), SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(defaultObjectSize, defaultSizingAlgorithm(NaturalDimensions::none(), SpecifiedSize::none(), defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, ZoomIsCarriedOnlyByAStatedSize)
{
    // Zoom is not an input. It scales the size the negotiation returns, not the dimensions it
    // resolves from, because a dimension taken from the default object size has to scale with
    // the rest and the default object size is a constant. So it reaches painting through a
    // size the caller states, never one the negotiation works out.

    EXPECT_EQ(1, defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100), SpecifiedSize::none(), defaultObjectSize).zoom());

    auto stated = ConcreteObjectSize::fixed(FloatSize { 400, 200 }, 2);
    EXPECT_EQ(FloatSize(400, 200), stated.size());
    EXPECT_EQ(2, stated.zoom());
}

TEST(ObjectSizeNegotiation, DensityCorrected)
{
    // A 2x 'srcset' candidate has a multiplier of 0.5, halving its raw natural dimensions.
    EXPECT_EQ(widthOnly(100), widthOnly(200).densityCorrected(0.5));
    EXPECT_EQ(heightOnly(50), heightOnly(100).densityCorrected(0.5));
    EXPECT_EQ(FloatSize(100, 50), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).densityCorrected(0.5), SpecifiedSize::none(), defaultObjectSize).size());

    // Only the dimensions scale: an aspect ratio is scale invariant, and the specified size
    // and default object size are already in the caller's coordinate space.
    EXPECT_EQ(aspectRatioOnly({ 1, 1 }), aspectRatioOnly({ 1, 1 }).densityCorrected(0.5));
    EXPECT_EQ(FloatSize(150, 150), resolveContainConstraint(aspectRatioOnly({ 1, 1 }).densityCorrected(0.5), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(100, 40), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).densityCorrected(0.5), { 100, 40 }, defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, OrientedNaturalDimensions)
{
    // Orientation is applied to the dimensions before they reach the negotiation, because
    // only an image can resolve Orientation::FromImage.

    // A 90 degree rotation transposes the natural dimensions and the natural aspect ratio.
    constexpr auto rotated = ImageOrientation::Orientation::OriginRightTop;
    EXPECT_EQ(FloatSize(100, 200), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).oriented(rotated), SpecifiedSize::none(), defaultObjectSize).size());
    EXPECT_EQ(FloatSize(100, 200), defaultSizingAlgorithm(aspectRatioOnly({ 2, 1 }).oriented(rotated), { 100, std::nullopt }, defaultObjectSize).size());
    EXPECT_EQ(FloatSize(75, 150), resolveContainConstraint(aspectRatioOnly({ 2, 1 }).oriented(rotated), defaultObjectSize).size());

    // An orientation that does not swap the axes leaves them alone.
    constexpr auto mirrored = ImageOrientation::Orientation::OriginTopRight;
    EXPECT_EQ(FloatSize(200, 100), defaultSizingAlgorithm(NaturalDimensions::fixed(200, 100).oriented(mirrored), SpecifiedSize::none(), defaultObjectSize).size());
}

TEST(ObjectSizeNegotiation, BitmapImageNaturalDimensions)
{
    // A bitmap's natural dimensions are its pixels.

    RefPtr imageBuffer = ImageBuffer::create({ 20, 10 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::SRGB(), PixelFormat::BGRA8);
    ASSERT_TRUE(imageBuffer);
    RefPtr image = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer)));
    ASSERT_TRUE(image);

    auto naturalDimensions = image->naturalDimensions();
    EXPECT_EQ(20, naturalDimensions.width.value_or(0));
    EXPECT_EQ(10, naturalDimensions.height.value_or(0));

    auto turned = image->naturalDimensions(ImageOrientation::Orientation::OriginRightTop);
    EXPECT_EQ(10, turned.width.value_or(0));
    EXPECT_EQ(20, turned.height.value_or(0));
}

} // namespace TestWebKitAPI
