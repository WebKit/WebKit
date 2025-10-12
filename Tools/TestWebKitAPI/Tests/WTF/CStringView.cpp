/*
 * Copyright (C) 2025 Comcast Inc.
 * Copyright (C) 2025 Igalia S.L.
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
#include <wtf/text/CStringView.h>

#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, CStringViewNullAndEmpty)
{
    CStringView string;
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), nullptr);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    string = CStringView(nullptr);
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), nullptr);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    string = CStringView(""_s);
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), nullptr);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    string = CStringView("test"_s);
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string.isEmpty());
    EXPECT_TRUE(string.utf8());
    EXPECT_FALSE(!string);
    EXPECT_TRUE(string);
}

TEST(WTF, CStringViewLength)
{
    CStringView string;
    EXPECT_EQ(string.length(), static_cast<size_t>(0));
    EXPECT_EQ(string.span8().size(), static_cast<size_t>(0));

    string = CStringView("test"_s);
    EXPECT_EQ(string.length(), static_cast<size_t>(4));
    EXPECT_EQ(string.span8().size(), static_cast<size_t>(4));
}

TEST(WTF, CStringViewFrom)
{
    const char* stringPtr = "test";
    CStringView string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.length(), static_cast<size_t>(4));
    EXPECT_TRUE(string);
    EXPECT_EQ(string.utf8(), stringPtr);

    stringPtr = "";
    string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.length(), static_cast<size_t>(0));
    EXPECT_FALSE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
}

TEST(WTF, CStringViewEquality)
{
    CStringView string("Test"_s);
    CStringView sameString("Test"_s);
    CStringView anotherString("another test"_s);
    CStringView emptyString;
    CStringView nullString(nullptr);
    EXPECT_TRUE(string != emptyString);
    EXPECT_EQ(string, string);
    EXPECT_EQ(string, sameString);
    EXPECT_TRUE(string != anotherString);
    EXPECT_EQ(emptyString, nullString);
}

} // namespace TestWebKitAPI
