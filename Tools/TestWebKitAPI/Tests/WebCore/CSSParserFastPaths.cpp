/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/CSSParserContext.h>
#include <WebCore/CSSParserFastPaths.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CSSParserFastPaths, ParseRgbAndRgba)
{
    StringView expectedValidInputs[] = {
        "rgb(255,0,0)"_s,
        "rgba(255,0,0)"_s,
        "rgb(255,0,0,1.0)"_s,
        "rgba(255,0,0,1.0)"_s,
        "rgb(0,255,0)"_s,
        "rgba(0,255,0)"_s,
        "rgb(0,255,0,1.0)"_s,
        "rgba(0,255,0,1.0)"_s,
        "rgb(0,0,255)"_s,
        "rgba(0,0,255)"_s,
        "rgb(0,0,255,1.0)"_s,
        "rgba(0,0,255,1.0)"_s
    };

    for (auto input : expectedValidInputs)
        EXPECT_TRUE(CSSParserFastPaths::parseSimpleColor(input, strictCSSParserContext()));

    StringView expectedInvalidInputs[] = {
        "rgb(255,0,0"_s,
        "rgba(255,0,0"_s,
        "rgb(255,0,0,"_s,
        "rgba(255,0,0,"_s,
        "rgb(255,0,0?"_s,
        "rgba(255,0,0?"_s,
        "rgb(255,0,0,)"_s,
        "rgba(255,0,0,)"_s,
        "rgb(255,0,0,1.0"_s,
        "rgba(255,0,0,1.0"_s,
        "rgb(255,0,0?1.0,"_s,
        "rgba(255,0,0?1.0,"_s,
        "rgb(255,0,0,1.0?"_s,
        "rgba(255,0,0,1.0?"_s,
        "rgb(255,0,0,1.0,)"_s,
        "rgba(255,0,0,1.0,)"_s,
    };

    for (auto input : expectedInvalidInputs)
        EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor(input, strictCSSParserContext()));
}

TEST(CSSParserFastPaths, ParseLegacyHsl)
{
    auto& context = strictCSSParserContext();

    // Basic legacy HSL colors (comma-separated).
    auto red = CSSParserFastPaths::parseSimpleColor("hsl(0, 100%, 50%)"_s, context);
    ASSERT_TRUE(red);
    auto redResolved = red->resolved();
    EXPECT_EQ(redResolved.red, 255);
    EXPECT_EQ(redResolved.green, 0);
    EXPECT_EQ(redResolved.blue, 0);

    auto green = CSSParserFastPaths::parseSimpleColor("hsl(120, 100%, 50%)"_s, context);
    ASSERT_TRUE(green);
    auto greenResolved = green->resolved();
    EXPECT_EQ(greenResolved.red, 0);
    EXPECT_EQ(greenResolved.green, 255);
    EXPECT_EQ(greenResolved.blue, 0);

    auto blue = CSSParserFastPaths::parseSimpleColor("hsl(240, 100%, 50%)"_s, context);
    ASSERT_TRUE(blue);
    auto blueResolved = blue->resolved();
    EXPECT_EQ(blueResolved.red, 0);
    EXPECT_EQ(blueResolved.green, 0);
    EXPECT_EQ(blueResolved.blue, 255);

    // hsla() synonym.
    auto redA = CSSParserFastPaths::parseSimpleColor("hsla(0, 100%, 50%, 0.5)"_s, context);
    ASSERT_TRUE(redA);
    auto redAResolved = redA->resolved();
    EXPECT_EQ(redAResolved.red, 255);
    EXPECT_EQ(redAResolved.green, 0);
    EXPECT_EQ(redAResolved.blue, 0);
    EXPECT_EQ(redAResolved.alpha, 128);

    // With angle units.
    auto degColor = CSSParserFastPaths::parseSimpleColor("hsl(120deg, 100%, 50%)"_s, context);
    ASSERT_TRUE(degColor);
    EXPECT_EQ(degColor->resolved().green, 255);

    // Hue wrapping.
    auto wrapped = CSSParserFastPaths::parseSimpleColor("hsl(480, 100%, 50%)"_s, context);
    ASSERT_TRUE(wrapped);
    auto wrappedResolved = wrapped->resolved();
    EXPECT_EQ(wrappedResolved.red, 0);
    EXPECT_EQ(wrappedResolved.green, 255);
    EXPECT_EQ(wrappedResolved.blue, 0);
}

TEST(CSSParserFastPaths, ParseModernHsl)
{
    auto& context = strictCSSParserContext();

    // Basic modern HSL colors (space-separated).
    auto red = CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50%)"_s, context);
    ASSERT_TRUE(red);
    auto redResolved = red->resolved();
    EXPECT_EQ(redResolved.red, 255);
    EXPECT_EQ(redResolved.green, 0);
    EXPECT_EQ(redResolved.blue, 0);

    auto green = CSSParserFastPaths::parseSimpleColor("hsl(120 100% 50%)"_s, context);
    ASSERT_TRUE(green);
    auto greenResolved = green->resolved();
    EXPECT_EQ(greenResolved.red, 0);
    EXPECT_EQ(greenResolved.green, 255);
    EXPECT_EQ(greenResolved.blue, 0);

    auto blue = CSSParserFastPaths::parseSimpleColor("hsl(240 100% 50%)"_s, context);
    ASSERT_TRUE(blue);
    auto blueResolved = blue->resolved();
    EXPECT_EQ(blueResolved.red, 0);
    EXPECT_EQ(blueResolved.green, 0);
    EXPECT_EQ(blueResolved.blue, 255);

    // Black and white.
    auto black = CSSParserFastPaths::parseSimpleColor("hsl(0 0% 0%)"_s, context);
    ASSERT_TRUE(black);
    auto blackResolved = black->resolved();
    EXPECT_EQ(blackResolved.red, 0);
    EXPECT_EQ(blackResolved.green, 0);
    EXPECT_EQ(blackResolved.blue, 0);

    auto white = CSSParserFastPaths::parseSimpleColor("hsl(0 0% 100%)"_s, context);
    ASSERT_TRUE(white);
    auto whiteResolved = white->resolved();
    EXPECT_EQ(whiteResolved.red, 255);
    EXPECT_EQ(whiteResolved.green, 255);
    EXPECT_EQ(whiteResolved.blue, 255);

    // With alpha (/ separator).
    auto semiRed = CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50% / 0.5)"_s, context);
    ASSERT_TRUE(semiRed);
    auto semiRedResolved = semiRed->resolved();
    EXPECT_EQ(semiRedResolved.red, 255);
    EXPECT_EQ(semiRedResolved.green, 0);
    EXPECT_EQ(semiRedResolved.blue, 0);
    EXPECT_EQ(semiRedResolved.alpha, 128);

    // Alpha as percentage.
    auto alphaPercent = CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50% / 50%)"_s, context);
    ASSERT_TRUE(alphaPercent);
    auto alphaPercentResolved = alphaPercent->resolved();
    EXPECT_EQ(alphaPercentResolved.red, 255);
    EXPECT_EQ(alphaPercentResolved.alpha, 128);

    // With angle units.
    auto degColor = CSSParserFastPaths::parseSimpleColor("hsl(120deg 100% 50%)"_s, context);
    ASSERT_TRUE(degColor);
    auto degColorResolved = degColor->resolved();
    EXPECT_EQ(degColorResolved.red, 0);
    EXPECT_EQ(degColorResolved.green, 255);
    EXPECT_EQ(degColorResolved.blue, 0);

    auto radColor = CSSParserFastPaths::parseSimpleColor("hsl(3.14rad 100% 50%)"_s, context);
    ASSERT_TRUE(radColor);
    // 3.14 rad ~= 179.9 deg, close to cyan (180 deg).
    EXPECT_EQ(radColor->resolved().red, 0);

    // hsla() synonym with modern syntax.
    auto hslaModern = CSSParserFastPaths::parseSimpleColor("hsla(120 100% 50% / 1)"_s, context);
    ASSERT_TRUE(hslaModern);
    auto hslaModernResolved = hslaModern->resolved();
    EXPECT_EQ(hslaModernResolved.green, 255);
    EXPECT_EQ(hslaModernResolved.alpha, 255);

    // No space around slash.
    auto noSpaceSlash = CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50%/0.5)"_s, context);
    ASSERT_TRUE(noSpaceSlash);
    auto noSpaceSlashResolved = noSpaceSlash->resolved();
    EXPECT_EQ(noSpaceSlashResolved.red, 255);
    EXPECT_EQ(noSpaceSlashResolved.alpha, 128);

    // Hue wrapping.
    auto wrapped = CSSParserFastPaths::parseSimpleColor("hsl(480 100% 50%)"_s, context);
    ASSERT_TRUE(wrapped);
    auto wrappedResolved = wrapped->resolved();
    EXPECT_EQ(wrappedResolved.red, 0);
    EXPECT_EQ(wrappedResolved.green, 255);
    EXPECT_EQ(wrappedResolved.blue, 0);

    // Modern and legacy should produce the same result.
    auto modern = CSSParserFastPaths::parseSimpleColor("hsl(120 50% 75%)"_s, context);
    auto legacy = CSSParserFastPaths::parseSimpleColor("hsl(120, 50%, 75%)"_s, context);
    ASSERT_TRUE(modern);
    ASSERT_TRUE(legacy);
    EXPECT_EQ(*modern, *legacy);
}

TEST(CSSParserFastPaths, ParseModernHslInvalid)
{
    auto& context = strictCSSParserContext();

    // Missing closing paren.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50%"_s, context));

    // Missing lightness.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100%)"_s, context));

    // Missing saturation and lightness.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0)"_s, context));

    // Empty.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl()"_s, context));

    // Mixing commas and spaces.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100%, 50%)"_s, context));

    // Alpha with comma instead of slash.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50%, 0.5)"_s, context));

    // Slash without alpha value.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50% /)"_s, context));

    // Double slash.
    EXPECT_FALSE(CSSParserFastPaths::parseSimpleColor("hsl(0 100% 50% / / 0.5)"_s, context));
}

} // namespace TestWebKitAPI
