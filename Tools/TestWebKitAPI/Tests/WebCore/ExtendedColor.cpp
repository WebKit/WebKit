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

using namespace WebCore;

namespace TestWebKitAPI {

TEST(ExtendedColor, Constructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1.isExtended());
    EXPECT_FLOAT_EQ(1.0, c1.asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c1.asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c1.asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c1.asExtended().alpha());
    EXPECT_EQ(1u, c1.asExtended().refCount());
    EXPECT_EQ(c1.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, CopyConstructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2(c1);

    EXPECT_FLOAT_EQ(1.0, c2.asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c2.asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c2.asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c2.asExtended().alpha());
    EXPECT_EQ(2u, c1.asExtended().refCount());
    EXPECT_EQ(2u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, Assignment)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2 = c1;

    EXPECT_FLOAT_EQ(1.0, c2.asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c2.asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c2.asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c2.asExtended().alpha());
    EXPECT_EQ(2u, c1.asExtended().refCount());
    EXPECT_EQ(2u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, MoveConstructor)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2(WTFMove(c1));
    // We should have moved the extended color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isExtended());
    EXPECT_FALSE(c1.isValid());

    EXPECT_FLOAT_EQ(1.0, c2.asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c2.asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c2.asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c2.asExtended().alpha());
    EXPECT_EQ(1u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, MoveAssignment)
{
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1.isExtended());

    Color c2 = WTFMove(c1);

    // We should have moved the extended color pointer into c2,
    // and set c1 to invalid so that it doesn't cause deletion.
    EXPECT_FALSE(c1.isExtended());
    EXPECT_FALSE(c1.isValid());

    EXPECT_FLOAT_EQ(1.0, c2.asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c2.asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c2.asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c2.asExtended().alpha());
    EXPECT_EQ(1u, c2.asExtended().refCount());
    EXPECT_EQ(c2.cssText(), "color(display-p3 1 0.5 0.25)");
}

TEST(ExtendedColor, BasicReferenceCounting)
{
    Color* c1 = new Color(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
    EXPECT_TRUE(c1->isExtended());

    Color* c2 = new Color(*c1);
    Color* c3 = new Color(*c2);

    EXPECT_FLOAT_EQ(1.0, c2->asExtended().red());
    EXPECT_FLOAT_EQ(0.5, c2->asExtended().green());
    EXPECT_FLOAT_EQ(0.25, c2->asExtended().blue());
    EXPECT_FLOAT_EQ(1.0, c2->asExtended().alpha());
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
    Color c1(1.0, 0.5, 0.25, 1.0, ColorSpaceDisplayP3);
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


} // namespace TestWebKitAPI
