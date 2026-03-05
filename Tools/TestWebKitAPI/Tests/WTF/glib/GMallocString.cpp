/*
 * Copyright (C) 2025 Igalia S.L.
 * Copyright (C) 2025 Comcast Inc.
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
#include <wtf/glib/GMallocString.h>

#include "Test.h"
#include <wtf/glib/GSpanExtras.h>

namespace TestWebKitAPI {

TEST(WTF_GMallocString, NullAndEmpty)
{
    GMallocString string;
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), nullptr);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    string = GMallocString(nullptr);
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), nullptr);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    const char* literal = nullptr;
    char* cString = g_strdup(literal);
    string = GMallocString::unsafeAdoptFromUTF8(cString);
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    cString = g_strdup(literal);
    auto gUniquePtr = GUniquePtr<char>(cString);
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniquePtr));
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);
    EXPECT_FALSE(gUniquePtr);

    cString = g_strdup(literal);
    GUniqueOutPtr<char> gUniqueOutPtr;
    gUniqueOutPtr.outPtr() = cString;
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniqueOutPtr));
    EXPECT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);
    EXPECT_FALSE(gUniqueOutPtr);

    literal = "";
    cString = g_strdup(literal);
    string = GMallocString::unsafeAdoptFromUTF8(cString);
    EXPECT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);

    cString = g_strdup(literal);
    gUniquePtr = GUniquePtr<char>(cString);
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniquePtr));
    EXPECT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);
    EXPECT_FALSE(gUniquePtr);

    cString = g_strdup(literal);
    gUniqueOutPtr.outPtr() = cString;
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniqueOutPtr));
    EXPECT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_TRUE(!string);
    EXPECT_FALSE(string);
    EXPECT_FALSE(gUniqueOutPtr);

    literal = "test";
    cString = g_strdup(literal);
    string = GMallocString::unsafeAdoptFromUTF8(cString);
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_FALSE(!string);
    EXPECT_TRUE(string);

    cString = g_strdup(literal);
    gUniquePtr = GUniquePtr<char>(cString);
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniquePtr));
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_FALSE(!string);
    EXPECT_TRUE(string);
    EXPECT_FALSE(gUniquePtr);

    cString = g_strdup(literal);
    gUniqueOutPtr.outPtr() = cString;
    string = GMallocString::unsafeAdoptFromUTF8(WTF::move(gUniqueOutPtr));
    EXPECT_FALSE(string.isNull());
    EXPECT_FALSE(string.isEmpty());
    EXPECT_EQ(string.utf8(), cString);
    EXPECT_FALSE(!string);
    EXPECT_TRUE(string);
    EXPECT_FALSE(gUniqueOutPtr);
}

TEST(WTF_GMallocString, Move)
{
    GMallocString emptyString1;
    EXPECT_TRUE(emptyString1.isNull());
    GMallocString emptyString2(WTF::move(emptyString1));
    EXPECT_TRUE(emptyString1.isNull());
    EXPECT_TRUE(emptyString2.isNull());
    emptyString1 = WTF::move(emptyString2);
    EXPECT_TRUE(emptyString1.isNull());
    EXPECT_TRUE(emptyString2.isNull());

    GMallocString nullString1(nullptr);
    EXPECT_TRUE(nullString1.isNull());
    GMallocString nullString2(WTF::move(nullString1));
    EXPECT_TRUE(nullString1.isNull());
    EXPECT_TRUE(nullString2.isNull());
    nullString1 = WTF::move(nullString2);
    EXPECT_TRUE(nullString1.isNull());
    EXPECT_TRUE(nullString2.isNull());

    auto nonEmptyString1 = GMallocString::unsafeAdoptFromUTF8(g_strdup("test"));
    EXPECT_FALSE(nonEmptyString1.isNull());
    GMallocString nonEmptyString2(WTF::move(nonEmptyString1));
    EXPECT_TRUE(nonEmptyString1.isNull());
    EXPECT_FALSE(nonEmptyString2.isNull());
    nonEmptyString1 = WTF::move(nonEmptyString2);
    EXPECT_FALSE(nonEmptyString1.isNull());
    EXPECT_TRUE(nonEmptyString2.isNull());
}

TEST(WTF_GMallocString, Length)
{
    GMallocString string;
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_EQ(string.span().size(), 0UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 0UZ);

    char* cString = g_strdup("");
    string = GMallocString::unsafeAdoptFromUTF8(cString);
    EXPECT_EQ(string.lengthInBytes(), 0UZ);
    EXPECT_EQ(string.span().size(), 0UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 1UZ);

    cString = g_strdup("test");
    string = GMallocString::unsafeAdoptFromUTF8(cString);
    EXPECT_EQ(string.lengthInBytes(), 4UZ);
    EXPECT_EQ(string.span().size(), 4UZ);
    EXPECT_EQ(string.spanIncludingNullTerminator().size(), 5UZ);
}

TEST(WTF_GMallocString, Equality)
{
    GMallocString string = GMallocString::unsafeAdoptFromUTF8(g_strdup("Test"));
    GMallocString sameString = GMallocString::unsafeAdoptFromUTF8(g_strdup("Test"));
    GMallocString anotherString = GMallocString::unsafeAdoptFromUTF8(g_strdup("another test"));
    GMallocString emptyString;
    GMallocString nullString(nullptr);
    GMallocString lowerCaseString = GMallocString::unsafeAdoptFromUTF8(g_strdup("test"));
    EXPECT_TRUE(string != emptyString);
    EXPECT_EQ(string, string);
    EXPECT_EQ(string, sameString);
    EXPECT_EQ(string, CStringView::unsafeFromUTF8("Test"));
    EXPECT_EQ(string, "Test"_s);
    EXPECT_TRUE(string != anotherString);
    EXPECT_EQ(emptyString, nullString);
}

TEST(WTF_GMallocString, CStringView)
{
    CStringView nullCStringView;
    GMallocString nullGMallocString(nullCStringView);
    EXPECT_TRUE(nullCStringView.isNull());
    EXPECT_TRUE(nullGMallocString.isNull());
    EXPECT_TRUE(nullCStringView.isNull());
    EXPECT_TRUE(nullGMallocString.isNull());

    CStringView cStringView = CStringView::unsafeFromUTF8("Test");
    GMallocString gMallocString(cStringView);
    EXPECT_TRUE(WTF::equal(cStringView.span(), gMallocString.span()));
    EXPECT_FALSE(cStringView.isNull());
    EXPECT_FALSE(gMallocString.isNull());
    EXPECT_FALSE(cStringView.isNull());
    EXPECT_FALSE(gMallocString.isNull());
}

TEST(WTF_GMallocString, LeakUTF8)
{
    char* bareString = g_strdup("test");
    GMallocString string = GMallocString::unsafeAdoptFromUTF8(bareString);
    EXPECT_FALSE(string.isEmpty());
    char* leakedString = string.leakUTF8();
    EXPECT_TRUE(string.isNull());
    EXPECT_EQ(bareString, leakedString);
    g_free(leakedString);
}

TEST(WTF_GMallocString, ToCStringView)
{
    GMallocString gMallocString;
    CStringView cStringView = toCStringView(gMallocString);
    EXPECT_TRUE(cStringView.isNull());
    EXPECT_TRUE(cStringView.isEmpty());
    EXPECT_EQ(cStringView.utf8(), nullptr);
    EXPECT_EQ(cStringView.lengthInBytes(), 0UZ);

    gMallocString = GMallocString::unsafeAdoptFromUTF8(g_strdup(""));
    cStringView = toCStringView(gMallocString);
    EXPECT_FALSE(cStringView.isNull());
    EXPECT_TRUE(cStringView.isEmpty());
    EXPECT_EQ(gMallocString, cStringView);
    EXPECT_EQ(gMallocString.utf8(), cStringView.utf8());
    EXPECT_EQ(cStringView.lengthInBytes(), 0UZ);

    gMallocString = GMallocString::unsafeAdoptFromUTF8(g_strdup("test"));
    cStringView = toCStringView(gMallocString);
    EXPECT_FALSE(cStringView.isNull());
    EXPECT_FALSE(cStringView.isEmpty());
    EXPECT_EQ(gMallocString, cStringView);
    EXPECT_EQ(gMallocString.utf8(), cStringView.utf8());
    EXPECT_EQ(cStringView.lengthInBytes(), 4UZ);
}

} // namespace TestWebKitAPI
