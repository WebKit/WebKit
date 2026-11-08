/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <WebCore/Color.h>
#include <WebCore/ColorConversion.h>
#include <WebCore/ColorSerialization.h>
#include <WebCore/ColorTypes.h>
#include <WebCore/ColorUtilities.h>
#include <wtf/Hasher.h>
#include <wtf/MathExtras.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(Color, RGBToHSL_White)
{
    Color color = Color::white;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(100, hslaColor.lightness);
    
    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);
    
    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Black)
{
    Color color = Color::black;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Red)
{
    Color color = Color::red;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(100, hslaColor.saturation);
    EXPECT_FLOAT_EQ(50, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Green)
{
    Color color = Color::green;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(120, hslaColor.hue);
    EXPECT_FLOAT_EQ(100, hslaColor.saturation);
    EXPECT_FLOAT_EQ(50, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Blue)
{
    Color color = Color::blue;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(240, hslaColor.hue);
    EXPECT_FLOAT_EQ(100, hslaColor.saturation);
    EXPECT_FLOAT_EQ(50, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_DarkGray)
{
    Color color = Color::darkGray;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(50.196083, hslaColor.lightness);
    
    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Gray)
{
    Color color = Color::gray;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(62.745102, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_LightGray)
{
    Color color = Color::lightGray;

    auto hslaColor = color.toColorTypeLossy<HSLA<float>>().resolved();

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(75.294121, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness() * 100, hslaColor.lightness);

    auto roundTrippedColor = convertColor<SRGBA<uint8_t>>(hslaColor);
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, Validity)
{
    Color invalidColor;
    EXPECT_FALSE(invalidColor.isValid());

    Color otherInvalidColor = invalidColor;
    EXPECT_FALSE(otherInvalidColor.isValid());

    Color validColor = Color::red;
    EXPECT_TRUE(validColor.isValid());

    Color otherValidColor = validColor;
    EXPECT_TRUE(otherValidColor.isValid());

    validColor = SRGBA<uint8_t> { 1, 2, 3, 4 };
    EXPECT_TRUE(validColor.isValid());
    auto validColorComponents = validColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_EQ(validColorComponents.red, 1);
    EXPECT_EQ(validColorComponents.green, 2);
    EXPECT_EQ(validColorComponents.blue, 3);
    EXPECT_EQ(validColorComponents.alpha, 4);

    Color yetAnotherValidColor(WTF::move(validColor));
    EXPECT_TRUE(yetAnotherValidColor.isValid());
    auto yetAnotherValidColorComponents = yetAnotherValidColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_EQ(yetAnotherValidColorComponents.red, 1);
    EXPECT_EQ(yetAnotherValidColorComponents.green, 2);
    EXPECT_EQ(yetAnotherValidColorComponents.blue, 3);
    EXPECT_EQ(yetAnotherValidColorComponents.alpha, 4);

    otherValidColor = WTF::move(yetAnotherValidColor);
    EXPECT_TRUE(otherValidColor.isValid());
    auto otherValidColorComponents = otherValidColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_EQ(otherValidColorComponents.red, 1);
    EXPECT_EQ(otherValidColorComponents.green, 2);
    EXPECT_EQ(otherValidColorComponents.blue, 3);
    EXPECT_EQ(otherValidColorComponents.alpha, 4);
}

TEST(Color, Luminance)
{
    EXPECT_FLOAT_EQ(Color(Color::black).luminance(), 0);
    EXPECT_FLOAT_EQ(Color(Color::white).luminance(), 1);

    auto cComponents = SRGBA<uint8_t> { 85, 90, 160 }.resolved();
    EXPECT_FLOAT_EQ(Color(cComponents).luminance(), 0.11781455);

    EXPECT_EQ(cComponents.red, 85);
    EXPECT_EQ(cComponents.green, 90);
    EXPECT_EQ(cComponents.blue, 160);

    auto cLigtened = Color(cComponents).lightened().toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_FLOAT_EQ(Color(cLigtened).luminance(), 0.291682);
    EXPECT_EQ(cLigtened.red, 130);
    EXPECT_EQ(cLigtened.green, 137);
    EXPECT_EQ(cLigtened.blue, 244);

    auto cDarkened = Color(cComponents).darkened().toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_FLOAT_EQ(Color(cDarkened).luminance(), 0.027006242);
    EXPECT_EQ(cDarkened.red, 40);
    EXPECT_EQ(cDarkened.green, 43);
    EXPECT_EQ(cDarkened.blue, 76);
}

TEST(Color, Constructor)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };

    auto [colorSpace, components] = c1.colorSpaceAndResolvedColorComponents();
    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c1), "color(display-p3 1 0.5 0.25)"_s);
}

TEST(Color, CopyConstructor)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    Color c2(c1);

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
}

TEST(Color, Assignment)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    Color c2 = c1;

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
}

TEST(Color, Equality)
{
    {
        Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        Color c2 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        EXPECT_EQ(c1, c2);
    }

    {
        Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        Color c2 { SRGBA<float> { 1.0, 0.5, 0.25, 1.0 } };
        EXPECT_NE(c1, c2);
    }

    auto componentBytes = SRGBA<uint8_t> { 255, 128, 63, 127 };
    Color rgb1 { convertColor<SRGBA<float>>(componentBytes) };
    Color rgb2 { componentBytes };
    EXPECT_NE(rgb1, rgb2);
    EXPECT_NE(rgb2, rgb1);
}

TEST(Color, Hash)
{
    {
        Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        Color c2 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        EXPECT_EQ(computeHash(c1), computeHash(c2));
    }

    {
        Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
        Color c2 { SRGBA<float> { 1.0, 0.5, 0.25, 1.0 } };
        EXPECT_NE(computeHash(c1), computeHash(c2));
    }

    auto componentBytes = SRGBA<uint8_t> { 255, 128, 63, 127 };
    Color rgb1 { convertColor<SRGBA<float>>(componentBytes) };
    Color rgb2 { componentBytes };
    EXPECT_NE(computeHash(rgb1), computeHash(rgb2));
}

TEST(Color, MoveConstructor)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    Color c2(WTF::move(c1));

    // We should have moved the out of line color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isValid());

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    EXPECT_EQ(colorSpace, ColorSpaceName::DisplayP3);

    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
}

namespace {
template<typename T>
void selfMove(T& a, T&& b)
{
    a = WTF::move(b);
}
}

TEST(Color, MoveAssignment)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    Color c2;
    c2 = WTF::move(c1);

    EXPECT_TRUE(c2.isValid());

    // We should have moved the out of line color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isValid());

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    EXPECT_EQ(colorSpace, ColorSpaceName::DisplayP3);

    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);

    // The API contract is that move invalidates. We should mark the source as invalid even if the
    // colors are the same. Tests a bug where the implementation would return early from move with
    // invalid condition.
    Color c3 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    EXPECT_EQ(c2, c3);
    c2 = WTF::move(c3);
    EXPECT_FALSE(c3.isValid()); // Moved-from source marked invalid.
    EXPECT_TRUE(c2.isValid());
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);

    // Moving into itself works.
    selfMove(c2, WTF::move(c2));
    EXPECT_TRUE(c2.isValid());
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
    selfMove(c1, WTF::move(c1));
    EXPECT_FALSE(c1.isValid());
}

Color makeColor()
{
    return Color { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
}

TEST(Color, ReturnValues)
{
    Color c2 = makeColor();
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
}

TEST(Color, P3ConversionToSRGB)
{
    Color p3Color { DisplayP3<float> { 1.0, 0.5, 0.25, 0.75 } };
    auto sRGBAColor = p3Color.toColorTypeLossy<SRGBA<float>>().resolved();
    EXPECT_FLOAT_EQ(sRGBAColor.red, 1.0f);
    EXPECT_FLOAT_EQ(sRGBAColor.green, 0.46253288f);
    EXPECT_FLOAT_EQ(sRGBAColor.blue, 0.14912745f);
    EXPECT_FLOAT_EQ(sRGBAColor.alpha, 0.75f);
}

TEST(Color, P3ConversionToExtendedSRGB)
{
    Color p3Color { DisplayP3<float> { 1.0, 0.5, 0.25, 0.75 } };
    auto sRGBAColor = p3Color.toColorTypeLossy<ExtendedSRGBA<float>>().resolved();
    EXPECT_FLOAT_EQ(sRGBAColor.red, 1.0740442f);
    EXPECT_FLOAT_EQ(sRGBAColor.green, 0.46253282f);
    EXPECT_FLOAT_EQ(sRGBAColor.blue, 0.14912748f);
    EXPECT_FLOAT_EQ(sRGBAColor.alpha, 0.75f);
}

TEST(Color, LinearSRGBConversionToSRGB)
{
    Color linearSRGBAColor { LinearSRGBA<float> { 1.0, 0.5, 0.25, 0.75 } };
    auto sRGBAColor = linearSRGBAColor.toColorTypeLossy<SRGBA<float>>().resolved();
    EXPECT_FLOAT_EQ(sRGBAColor.red, 1.0f);
    EXPECT_FLOAT_EQ(sRGBAColor.green, 0.735356927f);
    EXPECT_FLOAT_EQ(sRGBAColor.blue, 0.537098706f);
    EXPECT_FLOAT_EQ(sRGBAColor.alpha, 0.75f);
}

TEST(Color, ColorWithAlphaMultipliedBy)
{
    Color color { SRGBA<float> { 0., 0., 1., 0.6 } };

    {
        Color colorWithAlphaMultipliedBy = color.colorWithAlphaMultipliedBy(1.);
        EXPECT_EQ(color, colorWithAlphaMultipliedBy);
    }

    {
        Color colorWithAlphaMultipliedBy = color.colorWithAlphaMultipliedBy(0.5);
        auto [colorSpace, components] = colorWithAlphaMultipliedBy.colorSpaceAndResolvedColorComponents();
        EXPECT_EQ(colorSpace, ColorSpaceName::SRGB);
        auto [r, g, b, a] = components;
        EXPECT_FLOAT_EQ(r, 0.);
        EXPECT_FLOAT_EQ(g, 0.);
        EXPECT_FLOAT_EQ(b, 1.);
        EXPECT_FLOAT_EQ(a, 0.3);
    }

    {
        Color colorWithAlphaMultipliedBy = color.colorWithAlphaMultipliedBy(0.);
        auto [colorSpace, components] = colorWithAlphaMultipliedBy.colorSpaceAndResolvedColorComponents();
        EXPECT_EQ(colorSpace, ColorSpaceName::SRGB);
        auto [r, g, b, a] = components;
        EXPECT_FLOAT_EQ(r, 0.);
        EXPECT_FLOAT_EQ(g, 0.);
        EXPECT_FLOAT_EQ(b, 1.);
        EXPECT_FLOAT_EQ(a, 0.);
    }
}

template<typename T> concept CanPremultiply = requires(const T& color) { premultiplied(color); };
template<typename T> concept CanPremultiplyFlooring = requires(const T& color) { premultipliedFlooring(color); };
template<typename T> concept CanUnpremultiply = requires(const T& color) { unpremultiplied(color); };
template<typename T> concept CanResolve = requires(const T& color) { color.resolved(); };
template<typename Output, typename Input> concept CanConvertColor = requires(const Input& color) { convertColor<Output>(color); };
template<typename Output, typename Input> concept CanConvertColorCarryingForwardMissing = requires(const Input& color) { convertColorCarryingForwardMissing<Output>(color); };
template<typename T> concept CanWrapInPremultiplied = requires { typename Premultiplied<T>; };

using ResolvedSRGBA = ResolvedColorType<SRGBA<float>>;
using UnresolvedSRGBA = UnresolvedColorType<SRGBA<float>>;

static_assert(std::same_as<ResolvedPremultiplied<SRGBA<float>>, Premultiplied<ResolvedColorType<SRGBA<float>>>>);
static_assert(std::same_as<UnresolvedPremultiplied<SRGBA<float>>, Premultiplied<UnresolvedColorType<SRGBA<float>>>>);

static_assert(CanWrapInPremultiplied<ResolvedSRGBA>);
static_assert(CanWrapInPremultiplied<UnresolvedSRGBA>);
static_assert(!CanWrapInPremultiplied<SRGBA<float>>);
static_assert(!CanWrapInPremultiplied<ResolvedPremultiplied<SRGBA<float>>>);

static_assert(std::same_as<decltype(premultiplied(std::declval<UnresolvedSRGBA>())), UnresolvedPremultiplied<SRGBA<float>>>);
static_assert(std::same_as<decltype(premultiplied(std::declval<ResolvedSRGBA>())), ResolvedPremultiplied<SRGBA<float>>>);

static_assert(CanResolve<ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanResolve<UnresolvedPremultiplied<SRGBA<float>>>);

static_assert(CanPremultiply<UnresolvedSRGBA>);
static_assert(CanPremultiply<ResolvedSRGBA>);
static_assert(CanPremultiply<ResolvedColorType<ExtendedSRGBA<float>>>);
static_assert(CanPremultiply<ResolvedColorType<HSLA<float>>>);
static_assert(CanPremultiply<ResolvedColorType<HWBA<float>>>);
static_assert(CanPremultiply<ResolvedColorType<LCHA<float>>>);
static_assert(CanPremultiply<ResolvedColorType<OKLCHA<float>>>);
static_assert(CanPremultiply<ResolvedColorType<Lab<float>>>);
static_assert(CanPremultiply<ResolvedColorType<OKLab<float>>>);
static_assert(CanPremultiply<ResolvedColorType<XYZA<float, WhitePoint::D65>>>);
static_assert(!CanPremultiply<SRGBA<float>>);
static_assert(!CanPremultiply<UnresolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanPremultiply<ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanPremultiply<ResolvedColorType<SRGBA<uint8_t>>>);

static_assert(CanPremultiplyFlooring<SRGBA<uint8_t>>);
static_assert(!CanPremultiplyFlooring<ResolvedPremultiplied<SRGBA<uint8_t>>>);

static_assert(std::same_as<decltype(unpremultiplied(std::declval<UnresolvedPremultiplied<SRGBA<float>>>())), UnresolvedSRGBA>);
static_assert(std::same_as<decltype(unpremultiplied(std::declval<ResolvedPremultiplied<SRGBA<float>>>())), ResolvedSRGBA>);
static_assert(std::same_as<decltype(unpremultiplied(std::declval<ResolvedPremultiplied<SRGBA<uint8_t>>>())), ResolvedColorType<SRGBA<uint8_t>>>);
static_assert(std::is_convertible_v<ResolvedColorType<SRGBA<uint8_t>>, std::optional<Color>>);

static_assert(CanUnpremultiply<UnresolvedPremultiplied<SRGBA<float>>>);
static_assert(CanUnpremultiply<ResolvedPremultiplied<SRGBA<float>>>);
static_assert(CanUnpremultiply<ResolvedPremultiplied<SRGBA<uint8_t>>>);
static_assert(!CanUnpremultiply<SRGBA<float>>);
static_assert(!CanUnpremultiply<SRGBA<uint8_t>>);

static_assert(CanConvertColor<LinearSRGBA<float>, SRGBA<float>>);
static_assert(!CanConvertColor<LinearSRGBA<float>, UnresolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanConvertColor<LinearSRGBA<float>, ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanConvertColor<ResolvedPremultiplied<LinearSRGBA<float>>, SRGBA<float>>);
static_assert(!CanConvertColor<ResolvedPremultiplied<LinearSRGBA<float>>, ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!CanConvertColorCarryingForwardMissing<LinearSRGBA<float>, ResolvedPremultiplied<SRGBA<float>>>);

static_assert(!std::is_convertible_v<SRGBA<float>, ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!std::is_convertible_v<ResolvedSRGBA, ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!std::is_convertible_v<ResolvedPremultiplied<SRGBA<float>>, SRGBA<float>>);
static_assert(!std::is_constructible_v<SRGBA<float>, ResolvedPremultiplied<SRGBA<float>>>);
static_assert(!std::is_constructible_v<Color, ResolvedPremultiplied<SRGBA<float>>>);

TEST(Color, PremultipliedFloat)
{
    auto [r, g, b, a] = premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.5f }.unresolved());
    EXPECT_FLOAT_EQ(r, 0.5f);
    EXPECT_FLOAT_EQ(g, 0.25f);
    EXPECT_FLOAT_EQ(b, 0.125f);
    EXPECT_FLOAT_EQ(a, 0.5f);
}

TEST(Color, PremultipliedExtended)
{
    auto [r, g, b, a] = premultiplied(ExtendedSRGBA<float> { 2.0f, -0.5f, 0.25f, 0.5f }.unresolved());
    EXPECT_FLOAT_EQ(r, 1.0f);
    EXPECT_FLOAT_EQ(g, -0.25f);
    EXPECT_FLOAT_EQ(b, 0.125f);
    EXPECT_FLOAT_EQ(a, 0.5f);
}

TEST(Color, PremultipliedRectangularNonRGB)
{
    auto [L, a, b, alpha] = premultiplied(OKLab<float> { 0.5f, 0.25f, -0.25f, 0.5f }.unresolved());
    EXPECT_FLOAT_EQ(L, 0.25f);
    EXPECT_FLOAT_EQ(a, 0.125f);
    EXPECT_FLOAT_EQ(b, -0.125f);
    EXPECT_FLOAT_EQ(alpha, 0.5f);
}

TEST(Color, PremultipliedLeavesHue)
{
    auto color = premultiplied(LCHA<float> { 50.0f, 40.0f, 120.0f, 0.5f }.unresolved());

    auto [lightness, chroma, hue, alpha] = color;
    EXPECT_FLOAT_EQ(lightness, 25.0f);
    EXPECT_FLOAT_EQ(chroma, 20.0f);
    EXPECT_FLOAT_EQ(hue, 120.0f);
    EXPECT_FLOAT_EQ(alpha, 0.5f);

    auto [unpremultipliedLightness, unpremultipliedChroma, unpremultipliedHue, unpremultipliedAlpha] = unpremultiplied(color);
    EXPECT_FLOAT_EQ(unpremultipliedLightness, 50.0f);
    EXPECT_FLOAT_EQ(unpremultipliedChroma, 40.0f);
    EXPECT_FLOAT_EQ(unpremultipliedHue, 120.0f);
    EXPECT_FLOAT_EQ(unpremultipliedAlpha, 0.5f);
}

TEST(Color, PremultipliedMissingComponent)
{
    auto [r, g, b, a] = premultiplied(SRGBA<float> { std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.25f, 0.5f }.unresolved());
    EXPECT_TRUE(std::isnan(r));
    EXPECT_FLOAT_EQ(g, 0.25f);
    EXPECT_FLOAT_EQ(b, 0.125f);
    EXPECT_FLOAT_EQ(a, 0.5f);
}

TEST(Color, PremultipliedMissingAlpha)
{
    auto [r, g, b, a] = premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, std::numeric_limits<float>::quiet_NaN() }.unresolved());
    EXPECT_FLOAT_EQ(r, 1.0f);
    EXPECT_FLOAT_EQ(g, 0.5f);
    EXPECT_FLOAT_EQ(b, 0.25f);
    EXPECT_TRUE(std::isnan(a));
}

TEST(Color, PremultipliedResolved)
{
    auto color = premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.5f }.resolved());

    auto [r, g, b, a] = color;
    EXPECT_FLOAT_EQ(r, 0.5f);
    EXPECT_FLOAT_EQ(g, 0.25f);
    EXPECT_FLOAT_EQ(b, 0.125f);
    EXPECT_FLOAT_EQ(a, 0.5f);

    auto [unpremultipliedR, unpremultipliedG, unpremultipliedB, unpremultipliedA] = unpremultiplied(color);
    EXPECT_FLOAT_EQ(unpremultipliedR, 1.0f);
    EXPECT_FLOAT_EQ(unpremultipliedG, 0.5f);
    EXPECT_FLOAT_EQ(unpremultipliedB, 0.25f);
    EXPECT_FLOAT_EQ(unpremultipliedA, 0.5f);
}

TEST(Color, PremultipliedResolvedMissingAlpha)
{
    auto [r, g, b, a] = premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, std::numeric_limits<float>::quiet_NaN() }.resolved());
    EXPECT_FLOAT_EQ(r, 0.0f);
    EXPECT_FLOAT_EQ(g, 0.0f);
    EXPECT_FLOAT_EQ(b, 0.0f);
    EXPECT_FLOAT_EQ(a, 0.0f);
}

TEST(Color, UnpremultipliedFloat)
{
    auto [r, g, b, a] = unpremultiplied(premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.5f }.unresolved())).resolved();
    EXPECT_FLOAT_EQ(r, 1.0f);
    EXPECT_FLOAT_EQ(g, 0.5f);
    EXPECT_FLOAT_EQ(b, 0.25f);
    EXPECT_FLOAT_EQ(a, 0.5f);
}

TEST(Color, UnpremultipliedFloatTransparent)
{
    auto [r, g, b, a] = unpremultiplied(premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.0f }.unresolved())).resolved();
    EXPECT_FLOAT_EQ(r, 0.0f);
    EXPECT_FLOAT_EQ(g, 0.0f);
    EXPECT_FLOAT_EQ(b, 0.0f);
    EXPECT_FLOAT_EQ(a, 0.0f);
}

TEST(Color, UnpremultipliedMissingComponent)
{
    auto [r, g, b, a] = unpremultiplied(Premultiplied { SRGBA<float> { std::numeric_limits<float>::quiet_NaN(), 0.25f, 0.125f, 0.5f }.unresolved() });
    EXPECT_TRUE(std::isnan(r));
    EXPECT_FLOAT_EQ(g, 0.5f);
    EXPECT_FLOAT_EQ(b, 0.25f);
    EXPECT_FLOAT_EQ(a, 0.5f);
}

TEST(Color, UnpremultipliedMissingAlpha)
{
    auto [r, g, b, a] = unpremultiplied(Premultiplied { SRGBA<float> { 0.5f, 0.25f, 0.125f, std::numeric_limits<float>::quiet_NaN() }.unresolved() });
    EXPECT_FLOAT_EQ(r, 0.5f);
    EXPECT_FLOAT_EQ(g, 0.25f);
    EXPECT_FLOAT_EQ(b, 0.125f);
    EXPECT_TRUE(std::isnan(a));
}

TEST(Color, PremultipliedFlooring)
{
    EXPECT_TRUE((premultipliedFlooring(SRGBA<uint8_t> { 255, 128, 64, 128 }) == Premultiplied { SRGBA<uint8_t> { 128, 64, 32, 128 }.resolved() }));
    EXPECT_TRUE((premultipliedFlooring(SRGBA<uint8_t> { 10, 20, 30, 255 }) == Premultiplied { SRGBA<uint8_t> { 10, 20, 30, 255 }.resolved() }));
    EXPECT_TRUE((premultipliedFlooring(SRGBA<uint8_t> { 10, 20, 30, 0 }) == Premultiplied { SRGBA<uint8_t> { 0, 0, 0, 0 }.resolved() }));
}

TEST(Color, ConvertComponentTypeOfPremultiplied)
{
    auto color = convertComponentType<uint8_t>(premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.0f, 0.5f }.resolved()));
    EXPECT_TRUE((color == Premultiplied { SRGBA<uint8_t> { 128, 64, 0, 128 }.resolved() }));
}

TEST(Color, ColorComponentsOfPremultiplied)
{
    EXPECT_EQ(asColorComponents(premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.5f }.unresolved())), (ColorComponents<float, 4> { 0.5f, 0.25f, 0.125f, 0.5f }));
    EXPECT_EQ(asColorComponents(premultiplied(SRGBA<float> { 1.0f, 0.5f, 0.25f, 0.5f }.resolved())), (ColorComponents<float, 4> { 0.5f, 0.25f, 0.125f, 0.5f }));
}

TEST(Color, PackedColorFromPremultiplied)
{
    EXPECT_EQ(PackedColor::RGBA { premultipliedFlooring(SRGBA<uint8_t> { 255, 128, 64, 128 }) }.value, 0x80402080u);
    EXPECT_EQ(PackedColor::ARGB { premultipliedFlooring(SRGBA<uint8_t> { 255, 128, 64, 128 }) }.value, 0x80804020u);
}

TEST(Color, TransferFunctionsKeepTheSignOfExtendedComponents)
{
    auto expectRoundTrip = [&]<typename ColorType>() {
        using TransferFunction = typename ColorType::TransferFunction;
        for (float component : { -0.01f, -0.09f, -0.5f, -1.5f, 0.5f, 1.5f })
            EXPECT_NEAR(TransferFunction::toGammaEncoded(TransferFunction::toLinear(component)), component, 0.0001f);
    };

    expectRoundTrip.template operator()<ExtendedA98RGB<float>>();
    expectRoundTrip.template operator()<ExtendedProPhotoRGB<float>>();
    expectRoundTrip.template operator()<ExtendedRec2020<float>>();
    expectRoundTrip.template operator()<ExtendedSRGBA<float>>();
}

} // namespace TestWebKitAPI
