/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

} // namespace TestWebKitAPI
