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
    EXPECT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_TRUE(string.utf8());
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    string = CStringView("test"_s);
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string.isEmpty());
    EXPECT_TRUE(string.utf8());
    EXPECT_FALSE(!string);
    EXPECT_TRUE(string);
}

TEST(WTF, CStringViewSize)
{
    CStringView string;
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_EQ(string.span().size(), 0UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 0UZ);

    string = CStringView(""_s);
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_EQ(string.span().size(), 0UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 1UZ);

    string = CStringView("test"_s);
    EXPECT_EQ(string.lengthInBytes(), 4UZ);
    EXPECT_EQ(string.span().size(), 4UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 5UZ);

    string = CStringView::unsafeFromUTF8("water🍉melon");
    EXPECT_EQ(string.lengthInBytes(), 14UZ);
    EXPECT_EQ(string.span().size(), 14UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 15UZ);
}

TEST(WTF, CStringViewFrom)
{
    const char* stringPtr = "test";
    CStringView string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.lengthInBytes(), 4UZ);
    EXPECT_TRUE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
    string = CStringView::fromUTF8(byteCast<char8_t>(unsafeSpanIncludingNullTerminator(stringPtr)));
    EXPECT_EQ(string.lengthInBytes(), 4UZ);
    EXPECT_TRUE(string);
    EXPECT_EQ(string.utf8(), stringPtr);

    stringPtr = nullptr;
    string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_FALSE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
    string = CStringView::fromUTF8(byteCast<char8_t>(unsafeSpanIncludingNullTerminator(stringPtr)));
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_FALSE(string);
    EXPECT_EQ(string.utf8(), stringPtr);

    stringPtr = "";
    string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
    string = CStringView::fromUTF8(byteCast<char8_t>(unsafeSpanIncludingNullTerminator(stringPtr)));
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string);
    EXPECT_EQ(string.utf8(), stringPtr);

    stringPtr = "water🍉melon";
    string = CStringView::unsafeFromUTF8(stringPtr);
    EXPECT_EQ(string.lengthInBytes(), 14UZ);
    EXPECT_TRUE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
    string = CStringView::fromUTF8(byteCast<char8_t>(unsafeSpanIncludingNullTerminator(stringPtr)));
    EXPECT_EQ(string.lengthInBytes(), 14UZ);
    EXPECT_TRUE(string);
    EXPECT_EQ(string.utf8(), stringPtr);
}

TEST(WTF, CStringViewEquality)
{
    CStringView string("Test"_s);
    CStringView sameString("Test"_s);
    CStringView anotherString("another test"_s);
    CStringView nullString;
    CStringView nullString2(nullptr);
    CStringView emptyLiteral(""_s);

    EXPECT_EQ(string, string);
    EXPECT_EQ(string, sameString);
    EXPECT_TRUE(string != anotherString);
    EXPECT_TRUE(string != nullString);

    // Null vs null.
    EXPECT_EQ(nullString, nullString2);

    // Null vs empty (both have zero lengthInBytes).
    EXPECT_EQ(nullString, emptyLiteral);

    // Empty from unsafeFromUTF8 vs null and vs empty literal.
    char* bareEmptyString = strdup("");
    char* bareEmptyString2 = strdup("");
    auto emptyFromUTF8 = CStringView::unsafeFromUTF8(bareEmptyString);
    auto emptyFromUTF8_2 = CStringView::unsafeFromUTF8(bareEmptyString2);
    EXPECT_EQ(emptyFromUTF8, emptyFromUTF8_2);
    EXPECT_EQ(emptyFromUTF8, nullString);
    EXPECT_EQ(emptyFromUTF8, emptyLiteral);
    free(bareEmptyString);
    free(bareEmptyString2);

    // CStringView vs ASCIILiteral.
    EXPECT_EQ(string, "Test"_s);
    EXPECT_EQ("Test"_s, string);
    EXPECT_TRUE(string != "Other"_s);
    EXPECT_EQ(nullString, ""_s);
    EXPECT_EQ(emptyLiteral, ""_s);
}

} // namespace TestWebKitAPI
