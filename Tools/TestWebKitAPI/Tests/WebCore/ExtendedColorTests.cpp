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

#include "Test.h"
#include "WTFStringUtilities.h"
#include <WebCore/Color.h>
#include <wtf/MathExtras.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(ExtendedColor, Constructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());

    auto [r, g, b, alpha] = c1.asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(1u, c1.asExtended().refCount());
    EXPECT_EQ(c1.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, CopyConstructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2(c1);

    auto [r, g, b, alpha] = c2.asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(2u, c1.asExtended().refCount());
    EXPECT_EQ(2u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, Assignment)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2 = c1;

    auto [r, g, b, alpha] = c2.asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(2u, c1.asExtended().refCount());
    EXPECT_EQ(2u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, Equality)
{
    {
        Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        Color c2(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        EXPECT_EQ(c1, c2);
    }

    {
        Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        Color c2(1.0, 0.5, 0.25, 1.0, ColorSpace::SRGB);
        EXPECT_TRUE(c1.isExtended());

        EXPECT_NE(c1, c2);
    }

    int r = 255;
    int g = 128;
    int b = 63;
    int a = 127;
    Color rgb1(r / 255.0, g / 255.0, b / 255.0, a / 255.0, ColorSpace::SRGB);
    Color rgb2(r, g, b, a);
    EXPECT_NE(rgb1, rgb2);
    EXPECT_NE(rgb2, rgb1);
}

TEST(ExtendedColor, Hash)
{
    {
        Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        Color c2(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        EXPECT_EQ(c1.hash(), c2.hash());
    }

    {
        Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
        EXPECT_TRUE(c1.isExtended());

        Color c2(1.0, 0.5, 0.25, 1.0, ColorSpace::SRGB);
        EXPECT_TRUE(c1.isExtended());

        EXPECT_NE(c1.hash(), c2.hash());
    }

    int r = 255;
    int g = 128;
    int b = 63;
    int a = 127;
    Color rgb1(r / 255.0, g / 255.0, b / 255.0, a / 255.0, ColorSpace::SRGB);
    Color rgb2(r, g, b, a);
    EXPECT_NE(rgb1.hash(), rgb2.hash());
}

TEST(ExtendedColor, MoveConstructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2(WTFMove(c1));
    // We should have moved the extended color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isExtended());
    EXPECT_FALSE(c1.isValid());

    auto [r, g, b, alpha] = c2.asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(1u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, MoveAssignment)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2 = WTFMove(c1);

    // We should have moved the extended color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isExtended());
    EXPECT_FALSE(c1.isValid());

    auto [r, g, b, alpha] = c2.asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(1u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, BasicReferenceCounting)
{
    Color* c1 = new Color(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1->isExtended());

    Color* c2 = new Color(*c1);
    Color* c3 = new Color(*c2);

    auto [r, g, b, alpha] = c2->asExtended().channels();

    EXPECT_FLOAT_EQ(1.0, r);
    EXPECT_FLOAT_EQ(0.5, g);
    EXPECT_FLOAT_EQ(0.25, b);
    EXPECT_FLOAT_EQ(1.0, alpha);
    EXPECT_EQ(3u, c2->asExtended().refCount());
    EXPECT_EQ(c2->cssText(), "color(display-p3 1 0.5 0.25)");

    delete c1;
    EXPECT_EQ(2u, c2->asExtended().refCount());

    delete c2;
    EXPECT_EQ(1u, c3->asExtended().refCount());

    delete c3;
}

Color makeColor()
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpace::DisplayP3);
    EXPECT_TRUE(c1.isExtended());
    EXPECT_EQ(1u, c1.asExtended().refCount());

    return c1;
}

TEST(ExtendedColor, ReturnValues)
{
    Color c2 = makeColor();
    EXPECT_TRUE(c2.isExtended());

    EXPECT_EQ(1u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, P3ConversionToSRGB)
{
    {
        Color p3Color(1.0, 0.5, 0.25, 0.75, ColorSpace::DisplayP3);
        EXPECT_TRUE(p3Color.isExtended());

        auto sRGBComponents = p3Color.toSRGBAComponentsLossy();
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[0], 1.0f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[1], 0.462537885f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[2], 0.149147838f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[3], 0.75f));
    }

    {
        Color linearColor(1.0, 0.5, 0.25, 0.75, ColorSpace::LinearRGB);
        EXPECT_TRUE(linearColor.isExtended());
        auto sRGBComponents = linearColor.toSRGBAComponentsLossy();
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[0], 1.0f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[1], 0.735356927f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[2], 0.537098706f));
        EXPECT_TRUE(WTF::areEssentiallyEqual(sRGBComponents.components[3], 0.75f));
    }
}


} // namespace TestWebKitAPI
