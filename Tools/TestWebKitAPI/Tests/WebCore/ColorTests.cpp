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

#include "Test.h"
#include <WebCore/Color.h>
#include <WebCore/ColorConversion.h>
#include <WebCore/ColorSerialization.h>
#include <WebCore/ColorTypes.h>
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

    Color yetAnotherValidColor(WTFMove(validColor));
    EXPECT_TRUE(yetAnotherValidColor.isValid());
    auto yetAnotherValidColorComponents = yetAnotherValidColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    EXPECT_EQ(yetAnotherValidColorComponents.red, 1);
    EXPECT_EQ(yetAnotherValidColorComponents.green, 2);
    EXPECT_EQ(yetAnotherValidColorComponents.blue, 3);
    EXPECT_EQ(yetAnotherValidColorComponents.alpha, 4);

    otherValidColor = WTFMove(yetAnotherValidColor);
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
    Color c2(WTFMove(c1));

    // We should have moved the out of line color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isValid());

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    EXPECT_EQ(colorSpace, ColorSpace::DisplayP3);

    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
}

TEST(Color, MoveAssignment)
{
    Color c1 { DisplayP3<float> { 1.0, 0.5, 0.25, 1.0 } };
    Color c2 = WTFMove(c1);

    // We should have moved the out of line color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isValid());

    auto [colorSpace, components] = c2.colorSpaceAndResolvedColorComponents();
    EXPECT_EQ(colorSpace, ColorSpace::DisplayP3);

    auto [r, g, b, alpha] = components;

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(serializationForCSS(c2), "color(display-p3 1 0.5 0.25)"_s);
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
    EXPECT_FLOAT_EQ(sRGBAColor.green, 0.50120097f);
    EXPECT_FLOAT_EQ(sRGBAColor.blue, 0.26161569f);
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
        EXPECT_EQ(colorSpace, ColorSpace::SRGB);
        auto [r, g, b, a] = components;
        EXPECT_FLOAT_EQ(r, 0.);
        EXPECT_FLOAT_EQ(g, 0.);
        EXPECT_FLOAT_EQ(b, 1.);
        EXPECT_FLOAT_EQ(a, 0.3);
    }

    {
        Color colorWithAlphaMultipliedBy = color.colorWithAlphaMultipliedBy(0.);
        auto [colorSpace, components] = colorWithAlphaMultipliedBy.colorSpaceAndResolvedColorComponents();
        EXPECT_EQ(colorSpace, ColorSpace::SRGB);
        auto [r, g, b, a] = components;
        EXPECT_FLOAT_EQ(r, 0.);
        EXPECT_FLOAT_EQ(g, 0.);
        EXPECT_FLOAT_EQ(b, 1.);
        EXPECT_FLOAT_EQ(a, 0.);
    }
}

} // namespace TestWebKitAPI
