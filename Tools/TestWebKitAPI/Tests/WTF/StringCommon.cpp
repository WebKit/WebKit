/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <wtf/Vector.h>
#include <wtf/text/StringCommon.h>

namespace TestWebKitAPI {

#if CPU(ARM64)
TEST(WTF_StringCommon, Find8NonASCII)
{
    Vector<Latin1Character> vector(4096);
    vector.fill('a');

    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 4096)));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 4095 - i)));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 1024);
    EXPECT_FALSE(WTF::find8NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find8NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find8NonASCII(vector.subspan(i, 4096 - i)) - vector.span().data(), i);
    }
}

TEST(WTF_StringCommon, Find16NonASCII)
{
    Vector<char16_t> vector(4096);
    vector.fill('a');

    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 4096)));

    vector[4095] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 4095);
    for (unsigned i = 0; i < 16; ++i)
        EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 4095 - i)));

    vector[1024] = 0x80;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 1024);
    EXPECT_FALSE(WTF::find16NonASCII(vector.subspan(0, 1023)));

    vector[1024] = 0x7f;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 4095);

    vector[0] = 0xff;
    EXPECT_EQ(WTF::find16NonASCII(vector.subspan(0, 4096)) - vector.span().data(), 0);
    for (int i = 0; i < 16; ++i) {
        vector[i] = 0xff;
        EXPECT_EQ(WTF::find16NonASCII(vector.subspan(i, 4096 - i)) - vector.span().data(), i);
    }
}
#endif

TEST(WTF_StringCommon, FindIgnoringASCIICaseWithoutLengthIdentical)
{
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needle", "needle"), 0UL);
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needle", "needley"), WTF::notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICaseWithoutLength("needley", "needle"), 0UL);
}

TEST(WTF_StringCommon, Equal)
{
    EXPECT_TRUE(WTF::equal(u8"Water🍉Melon"_span, u8"Water🍉Melon"_span));
    EXPECT_FALSE(WTF::equal(u8"Water🍉Melon"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::equal(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::equal(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::equal(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::equal(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::equal(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_FALSE(WTF::equal(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::equal(u8"Water🍉Melon"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::equal(u8"Water🍉Melon"_span, u8""_span));
    // EXPECT_TRUE(WTF::equal("test"_span, "test"_span8)); // This should not compile.
    String string(u8"Water🍉Melon"_span);
    EXPECT_FALSE(string.is8Bit());
    EXPECT_TRUE(WTF::equal(string, u8"Water🍉Melon"_span));
    EXPECT_FALSE(WTF::equal(string, u8"🍉WaterMelon🍉"_span));
}

TEST(WTF_StringCommon, EqualIgnoringASCIICase)
{
    EXPECT_TRUE(WTF::equalIgnoringASCIICase(u8"Test"_span, u8"test"_span));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(u8"another test"_span, u8"test"_span));
    EXPECT_TRUE(WTF::equalIgnoringASCIICase(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::equalIgnoringASCIICase(std::span<const char8_t>(), u8""_span));
    EXPECT_TRUE(WTF::equalIgnoringASCIICase(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::equalIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(u8"🍉Watermelon🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::equalIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::equalIgnoringASCIICase(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, StartsWith)
{
    EXPECT_TRUE(WTF::startsWith(u8"Water🍉Melon"_span, "Water"_s));
    EXPECT_FALSE(WTF::startsWith(u8"Water🍉Melon"_span, "water"_s));
    EXPECT_FALSE(WTF::startsWith(u8"🍉WaterMelon🍉"_span, "Water"_s));
    EXPECT_TRUE(WTF::startsWith(u8"🍉WaterMelon🍉"_span, u8"🍉"_span));
    EXPECT_FALSE(WTF::startsWith(u8"Water🍉Melon"_span, u8"🍉"_span));
    EXPECT_TRUE(WTF::startsWith(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::startsWith(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::startsWith(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::startsWith(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::startsWith(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::startsWith(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::startsWith(u8"🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::startsWith(u8"🍉WaterMelon🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::startsWith(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::startsWith(u8"🍉WaterMelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::startsWith(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, EndsWith)
{
    EXPECT_TRUE(WTF::endsWith(u8"Water🍉Melon"_span, "Melon"_s));
    EXPECT_FALSE(WTF::endsWith(u8"Water🍉Melon"_span, "melon"_s));
    EXPECT_FALSE(WTF::endsWith(u8"🍉WaterMelon🍉"_span, "Melon"_s));
    EXPECT_TRUE(WTF::endsWith(u8"🍉WaterMelon🍉"_span, u8"🍉"_span));
    EXPECT_FALSE(WTF::endsWith(u8"Water🍉Melon"_span, u8"🍉"_span));
    EXPECT_TRUE(WTF::endsWith(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::endsWith(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::endsWith(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::endsWith(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::endsWith(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::endsWith(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::endsWith(u8"🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::endsWith(u8"🍉WaterMelon🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::endsWith(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::endsWith(u8"🍉WaterMelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::endsWith(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, Find)
{
    EXPECT_EQ(WTF::find(u8"Water🍉Melon"_span, "ter"_s), 2UZ);
    EXPECT_EQ(WTF::find(u8"🍉WaterMelon🍉"_span, "ter"_s), 6UZ);
    EXPECT_EQ(WTF::find(u8"Water🍉Melon"_span, u8"🍉"_span), 5UZ);
    EXPECT_EQ(WTF::find(u8"🍉WaterMelon🍉"_span, u8"🍉"_span), 0UZ);
    EXPECT_EQ(WTF::find(std::span<const char8_t>(), std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::find(std::span<const char8_t>(), u8""_span), 0UZ);
    EXPECT_EQ(WTF::find(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::find(u8""_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::find(u8""_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::find(u8"🍉"_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::find(u8"🍉"_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::find(u8"🍉WaterMelon🍉"_span, u8"🍉WaterMelon🍉"_span), 0UZ);
    EXPECT_EQ(WTF::find(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::find(u8"🍉WaterMelon🍉"_span, u8""_span), 0UZ);
    // EXPECT_NE(WTF::find(u8"test"_span, "test"_span8), notFound); // This should not compile.
}

TEST(WTF_StringCommon, ReverseFind)
{
    EXPECT_EQ(WTF::reverseFind(u8"Water🍉Melon"_span, "ter"_s), 2UZ);
    EXPECT_EQ(WTF::reverseFind(u8"🍉WaterMelon🍉"_span, "ter"_s), 6UZ);
    EXPECT_EQ(WTF::reverseFind(u8"Water🍉Melon"_span, u8"🍉"_span), 5UZ);
    EXPECT_EQ(WTF::reverseFind(u8"🍉WaterMelon🍉"_span, u8"🍉"_span), 14UZ);
    EXPECT_EQ(WTF::reverseFind(std::span<const char8_t>(), std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::reverseFind(std::span<const char8_t>(), u8""_span), 0UZ);
    EXPECT_EQ(WTF::reverseFind(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::reverseFind(u8""_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::reverseFind(u8""_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::reverseFind(u8"🍉"_span, std::span<const char8_t>()), 4UZ);
    EXPECT_EQ(WTF::reverseFind(u8"🍉"_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::reverseFind(u8"🍉WaterMelon🍉"_span, u8"🍉WaterMelon🍉"_span), 0UZ);
    EXPECT_EQ(WTF::reverseFind(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()), 18UZ);
    EXPECT_EQ(WTF::reverseFind(u8"🍉WaterMelon🍉"_span, u8""_span), 18UZ);
    // EXPECT_NE(WTF::reverseFind(u8"test"_span, "test"_span8), notFound); // This should not compile.
}

TEST(WTF_StringCommon, Contains)
{
    EXPECT_TRUE(WTF::contains(u8"Water🍉Melon"_span, "Water"_s));
    EXPECT_TRUE(WTF::contains(u8"🍉WaterMelon🍉"_span, "Water"_s));
    EXPECT_TRUE(WTF::contains(u8"Water🍉Melon"_span, u8"🍉"_span));
    EXPECT_TRUE(WTF::contains(u8"🍉WaterMelon🍉"_span, u8"🍉"_span));
    EXPECT_FALSE(WTF::contains(u8"Water🍉Melon"_span, "pear"_s));
    EXPECT_FALSE(WTF::contains(u8"🍉WaterMelon🍉"_span, "pear"_s));
    EXPECT_FALSE(WTF::contains(u8"Water🍉Melon"_span, u8"🍈"_span));
    EXPECT_FALSE(WTF::contains(u8"🍉WaterMelon🍉"_span, u8"🍈"_span));
    EXPECT_TRUE(WTF::contains(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::contains(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::contains(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::contains(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::contains(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::contains(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::contains(u8"🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::contains(u8"🍉WaterMelon🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::contains(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::contains(u8"🍉WaterMelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::contains(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, StartsWithLettersIgnoringASCIICase)
{
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"Water🍉Melon"_span, "water"_s));
    EXPECT_FALSE(WTF::startsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, "water"_s));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::startsWithLettersIgnoringASCIICase(std::span<const char8_t>(), u8"watermelon"_span));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::startsWithLettersIgnoringASCIICase(u8""_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"Water"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::startsWithLettersIgnoringASCIICase(u8"Water"_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"WaterMelon"_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::startsWithLettersIgnoringASCIICase(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, EndsWithLettersIgnoringASCIICase)
{
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"Water🍉Melon"_span, "melon"_s));
    EXPECT_FALSE(WTF::endsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, "melon"_s));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::endsWithLettersIgnoringASCIICase(std::span<const char8_t>(), u8"watermelon"_span));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::endsWithLettersIgnoringASCIICase(u8""_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"Water"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::endsWithLettersIgnoringASCIICase(u8"Water"_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"WaterMelon"_span, u8"watermelon"_span));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::endsWithLettersIgnoringASCIICase(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, FindIgnoringASCIICase)
{
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"Water🍉Melon"_span, "water"_s), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, "water"_s), 4UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"Water🍉Melon"_span, u8"🍉"_span), 5UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, u8"🍉"_span), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(std::span<const char8_t>(), std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(std::span<const char8_t>(), u8""_span), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8""_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8""_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉"_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉"_span, u8"🍉WaterMelon🍉"_span), notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8"🍉WaterMelon🍉"_span), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8"🍉WaterMelon🍉"_span, 5UZ), notFound);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉Watermelon🍉"_span, std::span<const char8_t>()), 0UZ);
    EXPECT_EQ(WTF::findIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8""_span), 0UZ);
    // EXPECT_NE(WTF::findIgnoringASCIICase(u8"test"_span, "test"_span8), notFound); // This should not compile.
}

TEST(WTF_StringCommon, ContainsIgnoringASCIICase)
{
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"Water🍉Melon"_span, "melon"_s));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, "melon"_s));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"Water🍉Melon"_span, u8"🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉WaterMelon🍉"_span, u8"🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(std::span<const char8_t>(), std::span<const char8_t>()));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(std::span<const char8_t>(), u8""_span));
    EXPECT_FALSE(WTF::containsIgnoringASCIICase(std::span<const char8_t>(), u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8""_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::containsIgnoringASCIICase(u8""_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉"_span, std::span<const char8_t>()));
    EXPECT_FALSE(WTF::containsIgnoringASCIICase(u8"🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8"🍉WaterMelon🍉"_span));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉Watermelon🍉"_span, std::span<const char8_t>()));
    EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"🍉Watermelon🍉"_span, u8""_span));
    // EXPECT_TRUE(WTF::containsIgnoringASCIICase(u8"test"_span, "test"_span8)); // This should not compile.
}

TEST(WTF_StringCommon, CharactersAreAllASCII)
{
    EXPECT_TRUE(WTF::charactersAreAllASCII(u8"Test"_span));
    EXPECT_FALSE(WTF::charactersAreAllASCII(u8"🍉"_span));
    EXPECT_TRUE(WTF::charactersAreAllASCII(std::span<const char8_t>()));
    EXPECT_TRUE(WTF::charactersAreAllASCII(u8""_span));
}

TEST(WTF_StringCommon, CopyElements64To8)
{
    Vector<uint8_t> destination;
    destination.resize(4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.mutableSpan(), source.span());
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint8_t>(i));
}

TEST(WTF_StringCommon, CopyElements64To16)
{
    Vector<uint16_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffff);
    source.append(0x10000);
    source.append(UINT64_MAX);
    source.append(0x7fff);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.mutableSpan(), source.span());
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint16_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffU);
    EXPECT_EQ(destination[4096 + 1], 0x0000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint16_t>(i));
}

TEST(WTF_StringCommon, CopyElements64To32)
{
    Vector<uint32_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint64_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffffffffU);
    source.append(0x100000000ULL);
    source.append(UINT64_MAX);
    source.append(0x7fffffffU);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.mutableSpan(), source.span());
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint32_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffffffU);
    EXPECT_EQ(destination[4096 + 1], 0x00000000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffffffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint32_t>(i));
}

TEST(WTF_StringCommon, CopyElements32To16)
{
    Vector<uint16_t> destination;
    destination.resize(4096 + 4 + 4096);

    Vector<uint32_t> source;
    source.reserveInitialCapacity(4096 + 4 + 4096);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);
    source.append(0xffff);
    source.append(0x10000);
    source.append(UINT32_MAX);
    source.append(0x7fff);
    for (unsigned i = 0; i < 4096; ++i)
        source.append(i);

    WTF::copyElements(destination.mutableSpan(), source.span());
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[i], static_cast<uint16_t>(i));
    EXPECT_EQ(destination[4096 + 0], 0xffffU);
    EXPECT_EQ(destination[4096 + 1], 0x0000U);
    EXPECT_EQ(destination[4096 + 2], 0xffffU);
    EXPECT_EQ(destination[4096 + 3], 0x7fffU);
    for (unsigned i = 0; i < 4096; ++i)
        EXPECT_EQ(destination[4096 + 4 + i], static_cast<uint16_t>(i));
}

TEST(WTF_StringCommon, CharactersContain8)
{
    {
        Vector<Latin1Character> source;
        EXPECT_FALSE((charactersContain<Latin1Character, 0>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0, 1>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0, 1, 2>(source.span())));
    }

    {
        Vector<Latin1Character> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_TRUE((charactersContain<Latin1Character, 0>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 1>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 2>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 2, 3>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 16, 14>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 16>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 16, 15>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 16, 15, 17>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 16, 15, 17, 18>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0x81, 0x82>(source.span())));
    }

    {
        Vector<Latin1Character> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_FALSE((charactersContain<Latin1Character, 0>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 0xff>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<Latin1Character, 250>(source.span())));
        EXPECT_TRUE((charactersContain<Latin1Character, 249>(source.span())));
    }
}

TEST(WTF_StringCommon, CharactersContain16)
{
    {
        Vector<char16_t> source;
        EXPECT_FALSE((charactersContain<char16_t, 0>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0, 1>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0, 1, 2>(source.span())));
    }

    {
        Vector<char16_t> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_TRUE((charactersContain<char16_t, 0>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 1>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 2>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 2, 3>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 16, 14>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 16>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 16, 15>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 16, 15, 17>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 16, 15, 17, 18>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x81, 0x82>(source.span())));
    }

    {
        Vector<char16_t> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_FALSE((charactersContain<char16_t, 0>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0xff>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 250>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 249>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 0, 249>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x101>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1001>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1001, 0x1001>(source.span())));
    }

    {
        Vector<char16_t> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i + 0x1000);
        }
        EXPECT_FALSE((charactersContain<char16_t, 0>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0xff>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x81>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 250>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 249>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x101>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 0x1001>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1000>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1100>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1000 + 256>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1000 + 250>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 0x1000 + 249>(source.span())));
        EXPECT_TRUE((charactersContain<char16_t, 0x1000 + 249, 0>(source.span())));
        EXPECT_FALSE((charactersContain<char16_t, 0x1000 + 250, 0>(source.span())));
    }
}

TEST(WTF_StringCommon, CountMatchedCharacters8)
{
    {
        Vector<Latin1Character> source;
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 1)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 2)), 0U);
    }

    {
        Vector<Latin1Character> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 2)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 3)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 14)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 15)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 16)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 17)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 18)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0x81)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0x82)), 0U);
    }

    {
        Vector<Latin1Character> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0x81)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 249)), 1U);
    }

    {
        Vector<Latin1Character> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i) {
                if (i & 0x1)
                    source.append(i);
            }
        }
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 1)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0x81)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 249)), 1024U);
    }

    {
        Vector<Latin1Character> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i)
                source.append(1);
        }
        source.append(1);
        source.append(1);
        source.append(1);

        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 1)), source.size());
        EXPECT_EQ((WTF::countMatchedCharacters<Latin1Character>(source.span(), 0x81)), 0U);
    }
}

TEST(WTF_StringCommon, CountMatchedCharacters16)
{
    {
        Vector<char16_t> source;
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 1)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 2)), 0U);
    }

    {
        Vector<char16_t> source;
        for (unsigned i = 0; i < 15; ++i)
            source.append(i);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 2)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 3)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 14)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 15)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 16)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 17)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 18)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0x81)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0x82)), 0U);
    }

    {
        Vector<char16_t> source;
        for (unsigned i = 0; i < 250; ++i) {
            if (i & 0x1)
                source.append(i);
        }
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 1)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0x81)), 1U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 249)), 1U);
    }

    {
        Vector<char16_t> source;
        for (unsigned c = 0; c < 1024; ++c) {
            for (unsigned i = 0; i < 250; ++i) {
                if (i & 0x1)
                    source.append(i);
            }
        }
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 1)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0xff)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0x81)), 1024U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 250)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 249)), 1024U);
    }

    {
        Vector<char16_t> source;
        for (unsigned c = 0; c < 0xffff; ++c) {
            for (unsigned i = 0; i < 250; ++i)
                source.append(1);
        }
        source.append(1);
        source.append(1);
        source.append(1);

        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0)), 0U);
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 1)), source.size());
        EXPECT_EQ((WTF::countMatchedCharacters<char16_t>(source.span(), 0x81)), 0U);
    }
}

class CopyElementsDoubleToFloatTest : public testing::Test {
protected:
    void testConversion(size_t length)
    {
        // Allocate source and destination.
        Vector<double> source(length);
        Vector<float> destination(length);

        // Initialize source with test data.
        for (size_t i = 0; i < length; ++i)
            source[i] = static_cast<double>(i) * 1.5 + 0.25;

        // Perform conversion.
        WTF::copyElements(std::span<float>(destination), std::span<const double>(source));

        // Verify results.
        for (size_t i = 0; i < length; ++i) {
            float expected = static_cast<float>(source[i]);
            EXPECT_FLOAT_EQ(destination[i], expected)
                << "Mismatch at index " << i << " for length " << length;
        }
    }
};

TEST_F(CopyElementsDoubleToFloatTest, VerySmallSizes)
{
    // Test sizes smaller than SIMD width.
    for (size_t length = 1; length < 8; ++length)
        testConversion(length);
}

TEST_F(CopyElementsDoubleToFloatTest, ExactlySIMDWidth)
{
    // Test exactly 8 elements (one SIMD iteration).
    testConversion(8);
}

TEST_F(CopyElementsDoubleToFloatTest, JustAboveSIMDWidth)
{
    // Test 9-15 elements (one SIMD iteration + scalar remainder).
    for (size_t length = 9; length < 16; ++length)
        testConversion(length);
}

TEST_F(CopyElementsDoubleToFloatTest, ExactlyTwoSIMDIterations)
{
    // Test exactly 16 elements (two SIMD iterations).
    testConversion(16);
}

TEST_F(CopyElementsDoubleToFloatTest, MediumSizes)
{
    // Test various medium sizes.
    Vector<size_t> sizes = { 17, 20, 24, 31, 32, 48, 63, 64, 96, 127, 128 };
    for (size_t length : sizes)
        testConversion(length);
}

TEST_F(CopyElementsDoubleToFloatTest, LargeSizes)
{
    // Test large sizes.
    Vector<size_t> sizes = { 192, 256, 512, 1024, 2048, 4096 };
    for (size_t length : sizes)
        testConversion(length);
}

TEST_F(CopyElementsDoubleToFloatTest, EdgeCasesAroundSIMDBoundaries)
{
    // Test specifically around multiples of 8 (SIMD width).
    Vector<size_t> sizes = { 7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33 };
    for (size_t length : sizes)
        testConversion(length);
}

TEST_F(CopyElementsDoubleToFloatTest, SpecialValues)
{
    size_t length = 16;
    Vector<double> source(length);
    Vector<float> destination(length);

    // Test special floating point values.
    source[0] = 0.0;
    source[1] = -0.0;
    source[2] = 1.0;
    source[3] = -1.0;
    source[4] = std::numeric_limits<double>::infinity();
    source[5] = -std::numeric_limits<double>::infinity();
    source[6] = std::numeric_limits<double>::quiet_NaN();
    source[7] = std::numeric_limits<double>::max();
    source[8] = std::numeric_limits<double>::min();
    source[9] = std::numeric_limits<double>::lowest();
    source[10] = std::numeric_limits<double>::epsilon();
    source[11] = std::numeric_limits<double>::denorm_min();
    source[12] = 3.14159265358979323846;
    source[13] = 2.71828182845904523536;
    source[14] = 1.41421356237309504880;
    source[15] = 1.61803398874989484820;

    WTF::copyElements(std::span<float>(destination), std::span<const double>(source));

    // Verify special values.
    EXPECT_EQ(destination[0], 0.0f);
    EXPECT_EQ(destination[1], -0.0f);
    EXPECT_EQ(destination[2], 1.0f);
    EXPECT_EQ(destination[3], -1.0f);
    EXPECT_TRUE(std::isinf(destination[4]) && destination[4] > 0);
    EXPECT_TRUE(std::isinf(destination[5]) && destination[5] < 0);
    EXPECT_TRUE(std::isnan(destination[6]));
    EXPECT_EQ(destination[7], std::numeric_limits<float>::infinity()); // Overflow to inf.
    EXPECT_EQ(destination[8], 0.0f); // Underflows to zero.
    EXPECT_FALSE(std::signbit(destination[8])); // But should be positive zero.
    EXPECT_LT(destination[9], 0.0f); // Should be negative.

    // Check mathematical constants (with appropriate tolerance).
    EXPECT_NEAR(destination[12], 3.14159265f, 1e-6f);
    EXPECT_NEAR(destination[13], 2.71828183f, 1e-6f);
    EXPECT_NEAR(destination[14], 1.41421356f, 1e-6f);
    EXPECT_NEAR(destination[15], 1.61803399f, 1e-6f);
}

TEST_F(CopyElementsDoubleToFloatTest, PrecisionLoss)
{
    size_t length = 8;
    Vector<double> source(length);
    Vector<float> destination(length);

    // Test values that will lose precision when converted to float.
    source[0] = 1.0000000001; // Extra precision lost.
    source[1] = 1234567890.123456789; // Large number.
    source[2] = 0.123456789012345; // Many decimal places.
    source[3] = 1e-40; // Very small number.
    source[4] = 1e40; // Very large number.
    source[5] = 9007199254740992.0; // 2^53, exact in double.
    source[6] = 16777217.0; // 2^24 + 1, loses precision in float.
    source[7] = 0.1 + 0.2; // Classic floating point issue.

    WTF::copyElements(std::span<float>(destination), std::span<const double>(source));

    // Verify conversions (with appropriate tolerance for precision loss).
    for (size_t i = 0; i < length; ++i) {
        float expected = static_cast<float>(source[i]);
        EXPECT_FLOAT_EQ(destination[i], expected) << "Mismatch at index " << i;
    }
}

TEST_F(CopyElementsDoubleToFloatTest, StressTestMultipleIterations)
{
    // Stress test: run many conversions to catch any memory corruption.
    for (int iteration = 0; iteration < 100; ++iteration) {
        for (size_t length = 1; length <= 32; ++length) {
            Vector<double> source(length);
            Vector<float> destination(length);

            for (size_t i = 0; i < length; ++i)
                source[i] = static_cast<double>(iteration * 100 + i);

            WTF::copyElements(std::span<float>(destination), std::span<const double>(source));

            for (size_t i = 0; i < length; ++i) {
                EXPECT_FLOAT_EQ(destination[i], static_cast<float>(source[i]))
                    << "Iteration " << iteration << ", length " << length << ", index " << i;
            }
        }
    }
}

TEST_F(CopyElementsDoubleToFloatTest, AlignmentVariations)
{
    // Test with different alignments to ensure SIMD code handles unaligned data.
    size_t baseLength = 32;
    Vector<double> largeSource(baseLength + 8);
    Vector<float> largeDest(baseLength + 8);

    // Initialize.
    for (size_t i = 0; i < largeSource.size(); ++i)
        largeSource[i] = static_cast<double>(i) * 0.5;

    // Test with different offsets (different alignments).
    for (size_t offset = 0; offset < 8; ++offset) {
        std::span<const double> sourceSpan(largeSource.subspan(offset).data(), baseLength);
        std::span<float> destSpan(largeDest.mutableSpan().subspan(offset).data(), baseLength);

        WTF::copyElements(destSpan, sourceSpan);

        for (size_t i = 0; i < baseLength; ++i) {
            EXPECT_FLOAT_EQ(destSpan[i], static_cast<float>(sourceSpan[i]))
                << "Offset " << offset << ", index " << i;
        }
    }
}

} // namespace
