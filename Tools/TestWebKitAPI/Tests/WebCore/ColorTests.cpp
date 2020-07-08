/*
 * Copyright (C) 2011, 2012, 2019 Apple Inc. All rights reserved.
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
#include <WebCore/ColorTypes.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(Color, RGBToHSL_White)
{
    Color color = Color::white;

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(1, hslaColor.lightness);
    
    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);
    
    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Black)
{
    Color color = Color::black;

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Red)
{
    Color color = makeSimpleColor(255, 0, 0);

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(1, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.5, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Green)
{
    Color color = makeSimpleColor(0, 255, 0);

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0.33333334, hslaColor.hue);
    EXPECT_FLOAT_EQ(1, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.5, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Blue)
{
    Color color = makeSimpleColor(0, 0, 255);

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0.66666669, hslaColor.hue);
    EXPECT_FLOAT_EQ(1, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.5, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_DarkGray)
{
    Color color = Color::darkGray;

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.50196078431372548, hslaColor.lightness);
    
    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_Gray)
{
    Color color = Color::gray;

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.62745098039215685, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, RGBToHSL_LightGray)
{
    Color color = Color::lightGray;

    auto hslaColor = toHSLA(color.toSRGBALossy<float>());

    EXPECT_FLOAT_EQ(0, hslaColor.hue);
    EXPECT_FLOAT_EQ(0, hslaColor.saturation);
    EXPECT_FLOAT_EQ(0.75294117647058822, hslaColor.lightness);

    EXPECT_FLOAT_EQ(color.lightness(), hslaColor.lightness);

    auto roundTrippedColor = makeSimpleColor(toSRGBA(hslaColor));
    EXPECT_EQ(color, roundTrippedColor);
}

TEST(Color, Validity)
{
    Color invalidColor;
    EXPECT_FALSE(invalidColor.isValid());
    EXPECT_FALSE(invalidColor.isExtended());

    Color otherInvalidColor = invalidColor;
    EXPECT_FALSE(otherInvalidColor.isValid());
    EXPECT_FALSE(otherInvalidColor.isExtended());

    Color validColor = makeSimpleColor(255, 0, 0);
    EXPECT_TRUE(validColor.isValid());
    EXPECT_FALSE(validColor.isExtended());

    Color otherValidColor = validColor;
    EXPECT_TRUE(otherValidColor.isValid());
    EXPECT_FALSE(otherValidColor.isExtended());

    validColor = makeSimpleColor(1, 2, 3, 4);
    EXPECT_TRUE(validColor.isValid());
    EXPECT_FALSE(validColor.isExtended());
    auto validColorComponents = validColor.toSRGBALossy<uint8_t>();
    EXPECT_EQ(validColorComponents.red, 1);
    EXPECT_EQ(validColorComponents.green, 2);
    EXPECT_EQ(validColorComponents.blue, 3);
    EXPECT_EQ(validColorComponents.alpha, 4);

    Color yetAnotherValidColor(WTFMove(validColor));
    EXPECT_TRUE(yetAnotherValidColor.isValid());
    EXPECT_FALSE(yetAnotherValidColor.isExtended());
    auto yetAnotherValidColorComponents = yetAnotherValidColor.toSRGBALossy<uint8_t>();
    EXPECT_EQ(yetAnotherValidColorComponents.red, 1);
    EXPECT_EQ(yetAnotherValidColorComponents.green, 2);
    EXPECT_EQ(yetAnotherValidColorComponents.blue, 3);
    EXPECT_EQ(yetAnotherValidColorComponents.alpha, 4);

    otherValidColor = WTFMove(yetAnotherValidColor);
    EXPECT_TRUE(otherValidColor.isValid());
    EXPECT_FALSE(otherValidColor.isExtended());
    auto otherValidColorComponents = otherValidColor.toSRGBALossy<uint8_t>();
    EXPECT_EQ(otherValidColorComponents.red, 1);
    EXPECT_EQ(otherValidColorComponents.green, 2);
    EXPECT_EQ(otherValidColorComponents.blue, 3);
    EXPECT_EQ(otherValidColorComponents.alpha, 4);
}

TEST(Color, Luminance)
{
    EXPECT_FLOAT_EQ(Color(Color::black).luminance(), 0);
    EXPECT_FLOAT_EQ(Color(Color::white).luminance(), 1);

    auto cComponents = makeSimpleColor(85, 90, 160);
    EXPECT_FLOAT_EQ(Color(cComponents).luminance(), 0.11781692);

    EXPECT_EQ(cComponents.red, 85);
    EXPECT_EQ(cComponents.green, 90);
    EXPECT_EQ(cComponents.blue, 160);

    auto cLigtened = Color(cComponents).lightened().toSRGBALossy<uint8_t>();
    EXPECT_FLOAT_EQ(Color(cLigtened).luminance(), 0.29168808);
    EXPECT_EQ(cLigtened.red, 130);
    EXPECT_EQ(cLigtened.green, 137);
    EXPECT_EQ(cLigtened.blue, 244);

    auto cDarkened = Color(cComponents).darkened().toSRGBALossy<uint8_t>();
    EXPECT_FLOAT_EQ(Color(cDarkened).luminance(), 0.027006727);
    EXPECT_EQ(cDarkened.red, 40);
    EXPECT_EQ(cDarkened.green, 43);
    EXPECT_EQ(cDarkened.blue, 76);
}

} // namespace TestWebKitAPI
