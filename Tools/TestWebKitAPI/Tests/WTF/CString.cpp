/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/text/CString.h>
#include <wtf/text/StringCommon.h>

TEST(WTF, CStringNullStringConstructor)
{
    CString string;
    constexpr size_t zeroLength = 0;
    ASSERT_TRUE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.data(), static_cast<const char*>(0));
    ASSERT_EQ(string.length(), zeroLength);

    CString stringFromCharPointer(static_cast<const char*>(0));
    ASSERT_TRUE(stringFromCharPointer.isNull());
    EXPECT_TRUE(stringFromCharPointer.isEmpty());
    ASSERT_EQ(stringFromCharPointer.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharPointer.length(), zeroLength);

    CString stringFromCharAndLength(std::span { static_cast<const char*>(0), zeroLength });
    ASSERT_TRUE(stringFromCharAndLength.isNull());
    EXPECT_TRUE(stringFromCharAndLength.isEmpty());
    ASSERT_EQ(stringFromCharAndLength.data(), static_cast<const char*>(0));
    ASSERT_EQ(stringFromCharAndLength.length(), zeroLength);
}

TEST(WTF, CStringEmptyEmptyConstructor)
{
    const char* emptyString = "";

    CString stringFromEmptySpanWithNonNullPointer(unsafeMakeSpan(emptyString, 0));
    EXPECT_FALSE(stringFromEmptySpanWithNonNullPointer.isNull());
    EXPECT_TRUE(stringFromEmptySpanWithNonNullPointer.isEmpty());
    EXPECT_EQ(stringFromEmptySpanWithNonNullPointer.length(), 0UZ);

    CString string(emptyString);
    ASSERT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    ASSERT_EQ(string.length(), static_cast<size_t>(0));
    ASSERT_EQ(string.data()[0], 0);

    CString stringWithLength(""_span);
    ASSERT_FALSE(stringWithLength.isNull());
    EXPECT_TRUE(stringWithLength.isEmpty());
    ASSERT_EQ(stringWithLength.length(), static_cast<size_t>(0));
    ASSERT_EQ(stringWithLength.data()[0], 0);
}

TEST(WTF, CStringEmptyRegularConstructor)
{
    const char* referenceString = "WebKit";

    CString string(referenceString);
    ASSERT_FALSE(string.isNull());
    ASSERT_EQ(string.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, string.data());

    CString stringWithLength(std::span { referenceString, 6 });
    ASSERT_FALSE(stringWithLength.isNull());
    ASSERT_EQ(stringWithLength.length(), strlen(referenceString));
    ASSERT_STREQ(referenceString, stringWithLength.data());
}

TEST(WTF, CStringUninitializedConstructor)
{
    std::span<char> buffer;
    CString emptyString = CString::newUninitialized(0, buffer);
    ASSERT_FALSE(emptyString.isNull());
    ASSERT_EQ(buffer.data(), emptyString.data());
    ASSERT_TRUE(buffer.empty());

    const size_t length = 25;
    CString uninitializedString = CString::newUninitialized(length, buffer);
    ASSERT_FALSE(uninitializedString.isNull());
    ASSERT_EQ(buffer.data(), uninitializedString.data());
    ASSERT_EQ(uninitializedString.data()[length], 0);
}

TEST(WTF, CStringZeroTerminated)
{
    const char* referenceString = "WebKit";
    CString stringWithLength(std::span { referenceString, 3 });
    ASSERT_EQ(stringWithLength.data()[3], 0);
}

TEST(WTF, CStringCopyOnWrite)
{
    const char* initialString = "Webkit";
    CString string(initialString);
    CString copy = string;

    string.mutableSpan()[3] = 'K';
    ASSERT_TRUE(string != copy);
    ASSERT_STREQ(string.data(), "WebKit");
    ASSERT_STREQ(copy.data(), initialString);
}

TEST(WTF, CStringComparison)
{
    // Comparison with another CString.
    CString a;
    CString b;
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "a";
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "b";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "a";
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "a";
    b = "aa";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "";
    b = "";
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    a = "";
    b = CString();
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    a = "a";
    b = "";
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);

    // Comparison with a const char*.
    CString c;
    const char* d = 0;
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c";
    d = 0;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = CString();
    d = "d";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c";
    d = "d";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "c";
    d = "c";
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "c";
    d = "cc";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "cc";
    d = "c";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "";
    d = "";
    ASSERT_TRUE(c == d);
    ASSERT_FALSE(c != d);
    c = "";
    d = 0;
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = CString();
    d = "";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "a";
    d = "";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
    c = "";
    d = "b";
    ASSERT_FALSE(c == d);
    ASSERT_TRUE(c != d);
}

TEST(WTF, CStringStdStringInterop)
{
    // Null CString round-trip is lossy: null CStrings convert to empty std::strings that convert to empty CStrings.
    {
        CString a;
        EXPECT_TRUE(a.isNull());
        std::string stda;
        EXPECT_EQ(a.toStdString(), stda);
        CString b = stda;
        EXPECT_NE(a, b);
        EXPECT_EQ(b.length(), 0u);
        EXPECT_FALSE(b.isNull());
    }

    // Non-null string round-trip is exact.
    constexpr ASCIILiteral inputs[] = {
        ""_s,
        "some thing"_s,
        "some\0thing"_s
    };
    for (auto& input : inputs) {
        SCOPED_TRACE(::testing::Message() << "input: " << (input.characters() ? input.characters() : "nullptr"));
        // As const char*.
        {
            CString a { input.characters() };
            std::string stda { input.characters() };
            EXPECT_EQ(a.toStdString(), stda);
            CString b = stda;
            EXPECT_EQ(a, b);
        }
        // As ASCIILiteral / span.
        {
            CString a { input };
            auto inputSpan = input.span();
            std::string stda { inputSpan.begin(), inputSpan.end() };
            EXPECT_EQ(a.toStdString(), stda);
            CString b = stda;
            EXPECT_EQ(a, b);
        }
    }

    // Explict length strings, i.e. strings with nul chars inside, are exact.
    {
        auto inputSpan = unsafeMakeSpan("some\0thing", 10);
        CString a { inputSpan };
        EXPECT_EQ(a.length(), 10u);
        std::string stda { inputSpan.begin(), inputSpan.end() };
        EXPECT_EQ(stda.length(), 10u);
        EXPECT_EQ(a.toStdString(), stda);
    }
}

TEST(WTF, CStringViewASCIICaseConversions)
{
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Test"_span), CString("test"));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Test"_span), CString("TEST"));
    EXPECT_EQ(WTF::convertToASCIILowercase(u8"Water🍉Melon"_span), CString("water🍉melon"));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8"Water🍉Melon"_span), CString("WATER🍉MELON"));
    EXPECT_EQ(WTF::convertToASCIILowercase(std::span<const char8_t>()), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIIUppercase(std::span<const char8_t>()), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIILowercase(u8""_span), CString(""_s));
    EXPECT_EQ(WTF::convertToASCIIUppercase(u8""_span), CString(""_s));
}
