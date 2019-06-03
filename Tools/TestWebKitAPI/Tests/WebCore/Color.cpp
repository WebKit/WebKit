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

using namespace WebCore;

namespace TestWebKitAPI {

TEST(Color, RGBToHSV_White)
{
    Color color = Color::white;
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(0, s);
    EXPECT_DOUBLE_EQ(1, v);
}

TEST(Color, RGBToHSV_Black)
{
    Color color = Color::black;
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(0, s);
    EXPECT_DOUBLE_EQ(0, v);
}

TEST(Color, RGBToHSV_Red)
{
    Color color(255, 0, 0);
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(1, s);
    EXPECT_DOUBLE_EQ(1, v);
}

TEST(Color, RGBToHSV_Green)
{
    Color color(0, 255, 0);
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0.33333333333333331, h);
    EXPECT_DOUBLE_EQ(1, s);
    EXPECT_DOUBLE_EQ(1, v);
}

TEST(Color, RGBToHSV_Blue)
{
    Color color(0, 0, 255);
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0.66666666666666663, h);
    EXPECT_DOUBLE_EQ(1, s);
    EXPECT_DOUBLE_EQ(1, v);
}

TEST(Color, RGBToHSV_DarkGray)
{
    Color color = Color::darkGray;
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(0, s);
    EXPECT_DOUBLE_EQ(0.50196078431372548, v);
}

TEST(Color, RGBToHSV_Gray)
{
    Color color = Color::gray;
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(0, s);
    EXPECT_DOUBLE_EQ(0.62745098039215685, v);
}

TEST(Color, RGBToHSV_LightGray)
{
    Color color = Color::lightGray;
    
    double h = 0;
    double s = 0;
    double v = 0;
    color.getHSV(h, s, v);

    EXPECT_DOUBLE_EQ(0, h);
    EXPECT_DOUBLE_EQ(0, s);
    EXPECT_DOUBLE_EQ(0.75294117647058822, v);
}

TEST(Color, RGBToHSL_White)
{
    Color color = Color::white;

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(0, saturation);
    EXPECT_DOUBLE_EQ(1, lightness);
}

TEST(Color, RGBToHSL_Black)
{
    Color color = Color::black;

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(0, saturation);
    EXPECT_DOUBLE_EQ(0, lightness);
}

TEST(Color, RGBToHSL_Red)
{
    Color color(255, 0, 0);

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(1, saturation);
    EXPECT_DOUBLE_EQ(0.5, lightness);
}

TEST(Color, RGBToHSL_Green)
{
    Color color(0, 255, 0);

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(2, hue);
    EXPECT_DOUBLE_EQ(1, saturation);
    EXPECT_DOUBLE_EQ(0.5, lightness);
}

TEST(Color, RGBToHSL_Blue)
{
    Color color(0, 0, 255);

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(4, hue);
    EXPECT_DOUBLE_EQ(1, saturation);
    EXPECT_DOUBLE_EQ(0.5, lightness);
}

TEST(Color, RGBToHSL_DarkGray)
{
    Color color = Color::darkGray;

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(0, saturation);
    EXPECT_DOUBLE_EQ(0.50196078431372548, lightness);
}

TEST(Color, RGBToHSL_Gray)
{
    Color color = Color::gray;

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(0, saturation);
    EXPECT_DOUBLE_EQ(0.62745098039215685, lightness);
}

TEST(Color, RGBToHSL_LightGray)
{
    Color color = Color::lightGray;

    double hue, saturation, lightness;
    color.getHSL(hue, saturation, lightness);

    EXPECT_DOUBLE_EQ(0, hue);
    EXPECT_DOUBLE_EQ(0, saturation);
    EXPECT_DOUBLE_EQ(0.75294117647058822, lightness);
}

TEST(Color, Validity)
{
    Color invalidColor;
    EXPECT_FALSE(invalidColor.isValid());
    EXPECT_FALSE(invalidColor.isExtended());

    Color otherInvalidColor = invalidColor;
    EXPECT_FALSE(otherInvalidColor.isValid());
    EXPECT_FALSE(otherInvalidColor.isExtended());

    Color validColor(255, 0, 0);
    EXPECT_TRUE(validColor.isValid());
    EXPECT_FALSE(validColor.isExtended());

    Color otherValidColor = validColor;
    EXPECT_TRUE(otherValidColor.isValid());
    EXPECT_FALSE(otherValidColor.isExtended());

    validColor = Color(1, 2, 3, 4);
    EXPECT_TRUE(validColor.isValid());
    EXPECT_FALSE(validColor.isExtended());
    EXPECT_EQ(validColor.red(), 1);
    EXPECT_EQ(validColor.green(), 2);
    EXPECT_EQ(validColor.blue(), 3);
    EXPECT_EQ(validColor.alpha(), 4);

    Color yetAnotherValidColor(WTFMove(validColor));
    EXPECT_TRUE(yetAnotherValidColor.isValid());
    EXPECT_FALSE(yetAnotherValidColor.isExtended());
    EXPECT_EQ(yetAnotherValidColor.red(), 1);
    EXPECT_EQ(yetAnotherValidColor.green(), 2);
    EXPECT_EQ(yetAnotherValidColor.blue(), 3);
    EXPECT_EQ(yetAnotherValidColor.alpha(), 4);

    otherValidColor = WTFMove(yetAnotherValidColor);
    EXPECT_TRUE(otherValidColor.isValid());
    EXPECT_FALSE(otherValidColor.isExtended());
    EXPECT_EQ(otherValidColor.red(), 1);
    EXPECT_EQ(otherValidColor.green(), 2);
    EXPECT_EQ(otherValidColor.blue(), 3);
    EXPECT_EQ(otherValidColor.alpha(), 4);
}

} // namespace TestWebKitAPI
