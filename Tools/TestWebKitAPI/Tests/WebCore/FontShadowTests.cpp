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
#include <WebCore/FontShadow.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(FontShadow, InvalidColor)
{
    FontShadow fontShadow;
    fontShadow.offset = { 1.1, 2.2 };
    fontShadow.blurRadius = 3.3;
    EXPECT_EQ(serializationForCSS(fontShadow), "none");
    fontShadow.color = Color();
    EXPECT_EQ(serializationForCSS(fontShadow), "none");
}

TEST(FontShadow, NoOffsetOrBlurRadius)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    EXPECT_EQ(serializationForCSS(fontShadow), "none");
    fontShadow.offset = { 0, 0 };
    EXPECT_EQ(serializationForCSS(fontShadow), "none");
    fontShadow.blurRadius = 0;
    EXPECT_EQ(serializationForCSS(fontShadow), "none");
}

TEST(FontShadow, NoOffset)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    fontShadow.blurRadius = 3.3;
    EXPECT_EQ(serializationForCSS(fontShadow), "0px 0px rgb(255, 0, 0) 3.3px");
    fontShadow.offset = { 0, 0 };
    EXPECT_EQ(serializationForCSS(fontShadow), "0px 0px rgb(255, 0, 0) 3.3px");
}

TEST(FontShadow, NegativeOffset)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    fontShadow.offset = { -1.1, -2.2 };
    fontShadow.blurRadius = 3.3;
    EXPECT_EQ(serializationForCSS(fontShadow), "-1.1px -2.2px rgb(255, 0, 0) 3.3px");
}

TEST(FontShadow, NoBlurRadius)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    fontShadow.offset = { 1.1, 2.2 };
    EXPECT_EQ(serializationForCSS(fontShadow), "1.1px 2.2px rgb(255, 0, 0)");
    fontShadow.blurRadius = 0;
    EXPECT_EQ(serializationForCSS(fontShadow), "1.1px 2.2px rgb(255, 0, 0)");
}

TEST(FontShadow, NegativeBlurRadius)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    fontShadow.offset = { 1.1, 2.2 };
    fontShadow.blurRadius = -3.3;
    EXPECT_EQ(serializationForCSS(fontShadow), "1.1px 2.2px rgb(255, 0, 0) -3.3px");
}

TEST(FontShadow, AllNegative)
{
    FontShadow fontShadow;
    fontShadow.color = Color::red;
    fontShadow.offset = { -1.1, -2.2 };
    fontShadow.blurRadius = -3.3;
    EXPECT_EQ(serializationForCSS(fontShadow), "-1.1px -2.2px rgb(255, 0, 0) -3.3px");
}

} // namespace TestWebKitAPI
