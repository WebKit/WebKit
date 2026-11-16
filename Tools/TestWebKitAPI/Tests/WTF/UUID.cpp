/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>

TEST(WTF, BootSessionUUIDIdentity)
{
    EXPECT_EQ(bootSessionUUIDString(), bootSessionUUIDString());
}

static String parseAndStringifyUUID(const String& value)
{
    auto uuid = WTF::UUID::parseVersion4(value);
    if (!uuid)
        return { };
    return uuid->toString();
}

TEST(WTF, TestUUIDVersion4Parsing)
{
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

    EXPECT_FALSE(!!WTF::UUID::parseVersion4("12345678-9abc-5de0-89AB-0123456789ab"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("12345678-9abc-4dea-79AB-0123456789ab"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("12345678-9abc-4de0-7fff-0123456789ab"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("12345678-9abc-4de0-c0000-0123456789ab"_s));

    EXPECT_FALSE(!!WTF::UUID::parseVersion4("+ef944c1-5cb8-48aa-Ad12-C5f823f005c3"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("6ef944c1-+cb8-48aa-Ad12-C5f823f005c3"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("6ef944c1-5cb8-+8aa-Ad12-C5f823f005c3"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("6ef944c1-5cb8-48aa-+d12-C5f823f005c3"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("6ef944c1-5cb8-48aa-Ad12-+5f823f005c3"_s));

    EXPECT_FALSE(!!WTF::UUID::parseVersion4("00000000-0000-0000-0000-000000000000"_s));
    EXPECT_FALSE(!!WTF::UUID::parseVersion4("00000000-0000-0000-0000-000000000001"_s));
    EXPECT_TRUE(!!WTF::UUID::parseVersion4("00000000-0000-4000-8000-000000000000"_s));
    EXPECT_TRUE(!!WTF::UUID::parseVersion4("00000000-0000-4000-8000-000000000001"_s));

    for (size_t cptr = 0; cptr < 10; ++cptr) {
        auto createdUUID = WTF::UUID::createVersion4();
        auto createdString = createdUUID.toString();
        EXPECT_EQ(createdString.length(), 36u);
        EXPECT_EQ(createdString[14], '4');
        EXPECT_TRUE(createdString[19] == '8' || createdString[19] == '9' || createdString[19] == 'a' || createdString[19] == 'b');

        auto uuid = WTF::UUID::parseVersion4(createdString);
        EXPECT_TRUE(!!uuid);
        EXPECT_EQ(*uuid, createdUUID);
    }

    String testNormal = "12345678-9abc-4de0-89ab-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(testNormal), testNormal);

    String test8000 = "12345678-9abc-4de0-8000-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(test8000), test8000);

    String testBfff = "12345678-9abc-4de0-Bfff-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(testBfff), testBfff.convertToASCIILowercase());

    String testAd12 = "6ef944c1-5cb8-48aa-Ad12-C5f823f005c3"_s;
    EXPECT_EQ(parseAndStringifyUUID(testAd12), testAd12.convertToASCIILowercase());
}

// parseInteger() silently accepts a single leading '+', so UUID::parse() must
// explicitly reject a '+' at the start of every segment. The five segments
// begin at indices 0, 9, 14, 19 and 24. Note that parseVersion4() cannot
// exercise the index-14 case: a leading '+' there consumes a character slot,
// leaving only 3 hex digits, so the version nibble can never be 4 and the
// version check rejects it before the '+' would matter. parse() must be
// tested directly.
TEST(WTF, UUIDParseRejectsLeadingPlusInEachSegment)
{
    EXPECT_FALSE(!!WTF::UUID::parse("+1234567-89ab-cdef-0123-456789abcdef"_s)); // index 0
    EXPECT_FALSE(!!WTF::UUID::parse("01234567-+9ab-cdef-0123-456789abcdef"_s)); // index 9
    EXPECT_FALSE(!!WTF::UUID::parse("01234567-89ab-+def-0123-456789abcdef"_s)); // index 14
    EXPECT_FALSE(!!WTF::UUID::parse("01234567-89ab-cdef-+123-456789abcdef"_s)); // index 19
    EXPECT_FALSE(!!WTF::UUID::parse("01234567-89ab-cdef-0123-+56789abcdef"_s)); // index 24

    EXPECT_TRUE(!!WTF::UUID::parse("01234567-89ab-cdef-0123-456789abcdef"_s));
}

TEST(WTF, TestUUIDVersion4MakeString)
{
    String testNormal = "12345678-9abc-4de0-89ab-0123456789ab"_s;
    auto uuid = WTF::UUID::parseVersion4(testNormal);
    EXPECT_TRUE(!!uuid);
    EXPECT_EQ(makeString(uuid.value()), testNormal);
    EXPECT_EQ(makeString("keyframe-"_s, uuid.value()), makeString("keyframe-"_s, testNormal));

    EXPECT_EQ(WTF::StringTypeAdapter<WTF::UUID>(uuid.value()).length(), 36U);
}

// tryCreate() is the only way to build a UUID out of raw bytes, so it has to reject
// everything the reserved empty and deleted values would otherwise let through.
TEST(WTF, UUIDTryCreateFromSpan)
{
    constexpr auto valid = WTF::toArray<uint8_t>({ 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0x4d, 0xe0, 0x89, 0xab, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab });
    constexpr auto empty = WTF::toArray<uint8_t>({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
    constexpr auto seventeenBytes = WTF::toArray<uint8_t>({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });

    EXPECT_FALSE(!!WTF::UUID::tryCreate({ }));
    EXPECT_FALSE(!!WTF::UUID::tryCreate(std::span { valid }.first(15)));
    EXPECT_FALSE(!!WTF::UUID::tryCreate(seventeenBytes));
    EXPECT_FALSE(!!WTF::UUID::tryCreate(empty));

    // The deleted value is 1 as a UInt128, whose byte pattern depends on endianness.
    UInt128 deletedValue = 1;
    EXPECT_FALSE(!!WTF::UUID::tryCreate(asByteSpan(deletedValue)));

    auto uuid = WTF::UUID::tryCreate(valid);
    EXPECT_TRUE(!!uuid);
    EXPECT_TRUE(equalSpans(uuid->span(), std::span { valid }));
}

// The generated IPC decoder builds a UUID out of the two halves it decoded, so that overload
// has to reject the reserved values just like the span one does.
TEST(WTF, UUIDTryCreateFromHighAndLow)
{
    EXPECT_FALSE(!!WTF::UUID::tryCreate(0, 0));
    EXPECT_FALSE(!!WTF::UUID::tryCreate(0, 1));

    EXPECT_TRUE(!!WTF::UUID::tryCreate(0, 2));
    EXPECT_TRUE(!!WTF::UUID::tryCreate(1, 0));
    EXPECT_TRUE(!!WTF::UUID::tryCreate(1, 1));

    auto uuid = WTF::UUID::tryCreate(0x123456789abc4de0ULL, 0x89ab0123456789abULL);
    EXPECT_TRUE(!!uuid);
    EXPECT_EQ(uuid->high(), 0x123456789abc4de0ULL);
    EXPECT_EQ(uuid->low(), 0x89ab0123456789abULL);
    EXPECT_EQ(makeString(*uuid), "12345678-9abc-4de0-89ab-0123456789ab"_s);
}

// UUIDCanonicalForm renders bits a UUID cannot hold, and has to agree with a UUID on bits it can.
TEST(WTF, UUIDCanonicalForm)
{
    EXPECT_EQ(makeString(WTF::UUIDCanonicalForm { }), "00000000-0000-0000-0000-000000000000"_s);
    EXPECT_EQ(makeString(WTF::UUIDCanonicalForm { 0, 1 }), "00000000-0000-0000-0000-000000000001"_s);

    auto uuid = WTF::UUID::tryCreate(0x123456789abc4de0ULL, 0x89ab0123456789abULL);
    EXPECT_TRUE(!!uuid);
    EXPECT_EQ(makeString(WTF::UUIDCanonicalForm { uuid->high(), uuid->low() }), makeString(*uuid));
}
